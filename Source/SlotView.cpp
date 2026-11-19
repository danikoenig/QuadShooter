#include "SlotView.h"
#include <cmath>

using namespace ShooterColours;

SlotView::SlotView (QuadShooterProcessor& p, int i)
    : processor (p), index (i), colour (slot (i))
{
    for (auto* b : { &prevButton, &nextButton, &loadButton })
        addAndMakeVisible (b);
    prevButton.setTooltip ("Previous sample in this folder");
    nextButton.setTooltip ("Next sample in this folder");
    loadButton.setTooltip ("Load a sample into slot " + slotLetter (index));
    prevButton.onClick = [this] { if (onSelect) onSelect (index); if (onStep) onStep (index, -1); };
    nextButton.onClick = [this] { if (onSelect) onSelect (index); if (onStep) onStep (index, +1); };
    loadButton.onClick = [this] { if (onSelect) onSelect (index); openFileChooser(); };

    muteButton.setClickingTogglesState (true);
    muteButton.getProperties().set ("fill", clip.toString());
    muteButton.setTooltip ("Mute slot " + slotLetter (index));
    addAndMakeVisible (muteButton);
    muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::mute (index), muteButton);

    levelSlider.setSliderStyle (juce::Slider::LinearBar);
    levelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    levelSlider.getProperties().set ("accent", colour.toString());
    levelSlider.setTooltip ("Level of slot " + slotLetter (index));
    levelSlider.onValueChange = [this] { levelValue.setText (levelSlider.getTextFromValue (levelSlider.getValue()), juce::dontSendNotification); };
    addAndMakeVisible (levelSlider);
    levelAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::level (index), levelSlider);
    if (auto* prm = processor.apvts.getParameter (ParamIDs::level (index)))
        levelSlider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    levelSlider.textFromValueFunction = [] (double v) { return v <= -59.9 ? juce::String ("-inf") : juce::String (v, 1) + " dB"; };
    levelValue.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    levelValue.setColour (juce::Label::textColourId, text);
    levelValue.setJustificationType (juce::Justification::centredRight);
    levelValue.setInterceptsMouseClicks (false, false);
    levelValue.setText (levelSlider.getTextFromValue (levelSlider.getValue()), juce::dontSendNotification);
    addAndMakeVisible (levelValue);

    samplesChanged();
}

//==============================================================================
juce::String SlotView::noteText() const
{
    if (peaks.isEmpty())
        return "--";
    const float root = processor.getEffectiveRoot (index);
    if (root <= noPitch + 1.0f)
        return "No pitch";
    const int n = (int) std::round (root);
    const int cents = (int) std::round ((root - (float) n) * 100.0f);
    juce::String s = PitchDetector::noteName (n);
    if (std::abs (cents) >= 3)
        s << (cents > 0 ? " +" : " ") << cents << "c";
    return s;
}

void SlotView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool active = processor.activeSlot.load() == index;
    const bool muted = processor.apvts.getRawParameterValue (ParamIDs::mute (index))->load() > 0.5f;

    g.setColour (card);
    g.fillRoundedRectangle (r, 10.0f);
    // Farbiger Schimmer oben
    g.setGradientFill (juce::ColourGradient (colour.withAlpha (active ? 0.14f : 0.06f), 0.0f, 0.0f,
                                             colour.withAlpha (0.0f), 0.0f, r.getHeight() * 0.5f, false));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (active || dragHover ? colour : cardBorder);
    g.drawRoundedRectangle (r.reduced (active ? 1.0f : 0.5f), 10.0f, active ? 2.0f : 1.0f);

    // Buchstabe
    auto b = badge.toFloat();
    g.setColour (muted ? colour.withAlpha (0.3f) : colour);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (bgBottom);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (slotLetter (index), badge, juce::Justification::centred);

    // Name + Ordner
    auto na = nameArea;
    g.setColour (peaks.isEmpty() ? textDim : text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (peaks.isEmpty() ? juce::String ("Empty") : peaks.name, na.removeFromTop (na.getHeight() / 2 + 2),
                juce::Justification::bottomLeft, true);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (peaks.isEmpty() ? juce::String ("Drop a sample here") : folderText, na, juce::Justification::topLeft, true);

    // Waveform
    auto w = waveArea.toFloat();
    g.setColour (inset);
    g.fillRoundedRectangle (w, 7.0f);
    if (dragHover)
    {
        g.setColour (colour.withAlpha (0.15f));
        g.fillRoundedRectangle (w, 7.0f);
    }

    if (! peaks.isEmpty())
    {
        const int bins = (int) peaks.mins.size();
        const float midY = w.getCentreY();
        const float half = w.getHeight() * 0.44f;
        float mx = 0.0001f;
        for (int k = 0; k < bins; ++k)
            mx = juce::jmax (mx, std::abs (peaks.mins[(size_t) k]), std::abs (peaks.maxs[(size_t) k]));
        const float norm = juce::jmin (4.0f, 1.0f / mx);

        juce::Path path;
        const int cols = juce::jmax (2, (int) w.getWidth() - 8);
        const float x0 = w.getX() + 4.0f;
        for (int c = 0; c < cols; ++c)
        {
            const int k0 = c * bins / cols, k1 = juce::jmax (k0 + 1, (c + 1) * bins / cols);
            float lo = 0.0f, hi = 0.0f;
            for (int k = k0; k < k1 && k < bins; ++k)
            {
                lo = juce::jmin (lo, peaks.mins[(size_t) k]);
                hi = juce::jmax (hi, peaks.maxs[(size_t) k]);
            }
            const float x = x0 + (float) c;
            path.addRectangle (x, midY - hi * norm * half, 1.0f, juce::jmax (1.0f, (hi - lo) * norm * half));
        }

        const float startX = x0 + startPos * (float) cols;
        const float endX = x0 + juce::jmax (startPos, endPos) * (float) cols;
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (w.withLeft (startX).withRight (endX + 1.0f).toNearestInt());
            g.setColour (muted ? textDim.withAlpha (0.5f) : colour.withAlpha (0.9f));
            g.fillPath (path);
        }
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::RectangleList<int> outside;
            outside.add (w.withRight (startX).toNearestInt());
            outside.add (w.withLeft (endX + 1.0f).toNearestInt());
            g.reduceClipRegion (outside);
            g.setColour (textDim.withAlpha (0.25f));
            g.fillPath (path);
        }
        g.setColour (text.withAlpha (0.7f));
        if (startPos > 0.0f)
            g.drawVerticalLine ((int) startX, w.getY() + 3.0f, w.getBottom() - 3.0f);
        if (endPos < 0.999f)
            g.drawVerticalLine ((int) endX, w.getY() + 3.0f, w.getBottom() - 3.0f);
        if (playPos >= 0.0f)
        {
            g.setColour (text);
            g.drawVerticalLine ((int) (x0 + playPos * (float) cols), w.getY() + 2.0f, w.getBottom() - 2.0f);
        }

        g.setColour (textDim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::String (juce::roundToInt (peaks.numSourceSamples * 1000.0 / peaks.sampleRate)) + " ms",
                    w.reduced (7.0f, 4.0f).toNearestInt(), juce::Justification::topRight);
    }
    else
    {
        g.setColour (textDim.withAlpha (0.6f));
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText ("Drag a sample here\nfrom the library", waveArea, juce::Justification::centred, 2);
    }

    // Ton-Pill
    auto np = notePill.toFloat();
    const bool keySync = processor.apvts.getRawParameterValue (ParamIDs::keySync)->load() > 0.5f;
    const int lead = (int) std::round (processor.apvts.getRawParameterValue (ParamIDs::lead)->load());
    const bool isLead = keySync && lead == index;
    const float root = processor.getEffectiveRoot (index);
    const bool tonal = root > noPitch + 1.0f;

    g.setColour (inset);
    g.fillRoundedRectangle (np, 6.0f);
    g.setColour (isLead ? colour : cardBorder);
    g.drawRoundedRectangle (np.reduced (0.5f), 6.0f, 1.0f);

    auto inner = notePill.reduced (8, 0);
    // Stimmgabel-Symbol als Punkt
    g.setColour (tonal ? colour : textDim.withAlpha (0.5f));
    g.fillEllipse ((float) inner.getX(), (float) inner.getCentreY() - 3.0f, 6.0f, 6.0f);
    inner.removeFromLeft (12);
    g.setColour (tonal ? text : textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (noteText(), inner, juce::Justification::centredLeft);

    const int ov = processor.getRootOverride (index);
    juce::String right;
    juce::Colour rightCol = textDim;
    if (isLead)                                { right = "LEAD"; rightCol = colour; }
    else if (keySync && std::abs (lastSync) > 0.004f)
    {
        right = juce::String::fromUTF8 ("\xe2\x86\x92 ") + (lastSync > 0 ? "+" : "") + juce::String (lastSync, 2) + " st";
        rightCol = text;
    }
    else if (ov != rootAuto && ! peaks.isEmpty()) right = "manual";
    else if (! peaks.isEmpty() && tonal)      right = "auto";
    g.setColour (rightCol);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (right, inner, juce::Justification::centredRight);

    // Level-Zeile
    g.setColour (textDim);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("LEVEL", levelArea.withWidth (40), juce::Justification::centredLeft);
}

void SlotView::resized()
{
    auto r = getLocalBounds().reduced (10);

    auto header = r.removeFromTop (28);
    badge = header.removeFromLeft (28);
    header.removeFromLeft (8);
    loadButton.setBounds (header.removeFromRight (46).reduced (0, 2));
    header.removeFromRight (4);
    nextButton.setBounds (header.removeFromRight (24).reduced (0, 2));
    header.removeFromRight (2);
    prevButton.setBounds (header.removeFromRight (24).reduced (0, 2));
    header.removeFromRight (4);
    muteButton.setBounds (header.removeFromRight (24).reduced (0, 2));
    header.removeFromRight (6);
    nameArea = header;

    r.removeFromTop (8);
    levelArea = r.removeFromBottom (20);
    {
        auto la = levelArea;
        la.removeFromLeft (42);
        levelValue.setBounds (la.removeFromRight (56));
        la.removeFromRight (6);
        levelSlider.setBounds (la.reduced (0, 4));
    }
    r.removeFromBottom (8);
    notePill = r.removeFromBottom (24);
    r.removeFromBottom (6);
    waveArea = r;
}

void SlotView::mouseDown (const juce::MouseEvent& e)
{
    if (onSelect)
        onSelect (index);

    const auto pos = e.getPosition();
    if (notePill.contains (pos))
    {
        if (! peaks.isEmpty())
            showRootMenu();
        return;
    }
    if (waveArea.contains (pos) || badge.contains (pos))
    {
        if (e.mods.isPopupMenu())
            showSlotMenu();
        else if (! peaks.isEmpty())
            processor.triggerPreview (index);
        else
            openFileChooser();
    }
}

//==============================================================================
void SlotView::showRootMenu()
{
    const auto det = processor.getDetectedPitch (index);
    const int ov = processor.getRootOverride (index);

    juce::PopupMenu m;
    m.addSectionHeader ("Root note of slot " + slotLetter (index));
    m.addItem (1, det.tonal ? "Auto (detected " + PitchDetector::noteName ((int) std::round (det.midi)) + ")"
                            : juce::String ("Auto (no pitch detected)"), true, ov == rootAuto);
    m.addItem (2, "Not tonal (never pitch to the lead)", true, ov == rootNone);
    m.addSeparator();
    for (int oct = -1; oct <= 6; ++oct)
    {
        juce::PopupMenu sub;
        for (int n = 0; n < 12; ++n)
        {
            const int midi = (oct + 2) * 12 + n;
            sub.addItem (100 + midi, PitchDetector::noteName (midi), true, ov == midi);
        }
        m.addSubMenu ("Octave " + juce::String (oct), sub, true, nullptr, ov >= (oct + 2) * 12 && ov < (oct + 3) * 12);
    }

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                         .withTargetScreenArea (localAreaToGlobal (notePill)),
                     [safe = juce::Component::SafePointer<SlotView> (this)] (int res)
                     {
                         if (safe == nullptr || res == 0) return;
                         auto& self = *safe;
                         if (res == 1)        self.processor.setRootOverride (self.index, rootAuto);
                         else if (res == 2)   self.processor.setRootOverride (self.index, rootNone);
                         else if (res >= 100) self.processor.setRootOverride (self.index, res - 100);
                     });
}

void SlotView::showSlotMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Load sample...");
    m.addItem (2, "Clear slot", ! peaks.isEmpty());
    m.addItem (3, "Reveal in Finder", ! peaks.isEmpty());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [safe = juce::Component::SafePointer<SlotView> (this)] (int res)
                     {
                         if (safe == nullptr) return;
                         if (res == 1) safe->openFileChooser();
                         if (res == 2) safe->processor.clearSlot (safe->index);
                         if (res == 3) safe->processor.getSlotFile (safe->index).revealToUser();
                     });
}

void SlotView::openFileChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Load sample into slot " + slotLetter (index),
                                                   processor.getSlotFile (index).getParentDirectory(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file.existsAsFile() && processor.loadSample (index, file))
                                  processor.triggerPreview (index);
                          });
}

bool SlotView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (QuadShooterProcessor::isSupportedFile (juce::File (f)))
            return true;
    return false;
}

void SlotView::loadAndSelect (const juce::File& file)
{
    dragHover = false;
    if (onSelect)
        onSelect (index);
    if (QuadShooterProcessor::isSupportedFile (file) && processor.loadSample (index, file))
        processor.triggerPreview (index);
    repaint();
}

void SlotView::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (QuadShooterProcessor::isSupportedFile (juce::File (f)))
            return loadAndSelect (juce::File (f));
    dragHover = false;
    repaint();
}

bool SlotView::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("qs-sample:");
}

void SlotView::itemDropped (const SourceDetails& d)
{
    loadAndSelect (juce::File (d.description.toString().fromFirstOccurrenceOf ("qs-sample:", false, false)));
}

//==============================================================================
void SlotView::samplesChanged()
{
    peaks = processor.getPeaks (index);
    lastRoot = processor.getEffectiveRoot (index);
    repaint();
}

void SlotView::refresh()
{
    bool dirty = false;
    const float pp = processor.getPlayPosition (index);
    if (std::abs (pp - playPos) > 0.0005f) { playPos = pp; dirty = true; }

    const float sp = processor.apvts.getRawParameterValue (ParamIDs::start (index))->load();
    if (std::abs (sp - startPos) > 0.0005f) { startPos = sp; dirty = true; }
    const float ep = processor.apvts.getRawParameterValue (ParamIDs::end (index))->load();
    if (std::abs (ep - endPos) > 0.0005f) { endPos = ep; dirty = true; }

    const float so = processor.getSyncOffset (index);
    if (std::abs (so - lastSync) > 0.001f) { lastSync = so; dirty = true; }

    const float root = processor.getEffectiveRoot (index);
    if (std::abs (root - lastRoot) > 0.001f) { lastRoot = root; dirty = true; }

    const bool active = processor.activeSlot.load() == index;
    const bool lead = processor.apvts.getRawParameterValue (ParamIDs::keySync)->load() > 0.5f
                      && (int) std::round (processor.apvts.getRawParameterValue (ParamIDs::lead)->load()) == index;
    const bool muted = processor.apvts.getRawParameterValue (ParamIDs::mute (index))->load() > 0.5f;
    if (active != wasActive || lead != wasLead || muted != wasMuted)
    {
        wasActive = active; wasLead = lead; wasMuted = muted; dirty = true;
    }

    if (dirty)
        repaint();
}
