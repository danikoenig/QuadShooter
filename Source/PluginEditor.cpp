#include "PluginEditor.h"
#include "Colours.h"
#include <cmath>

using namespace ShooterColours;

namespace
{
    constexpr int libWidth = 290;
    constexpr int margin = 14;
    constexpr int gap = 12;
    constexpr int headerH = 40;
    constexpr int moduleH = 160;
}

QuadShooterEditor::QuadShooterEditor (QuadShooterProcessor& p)
    : AudioProcessorEditor (&p), processor (p), library (p), xyPad (p),
      clipMeter (clip, true), outMeter (accent2, false)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (library);
    addAndMakeVisible (xyPad);

    for (int s = 0; s < numSlots; ++s)
    {
        slots[s] = std::make_unique<SlotView> (processor, s);
        slots[s]->onSelect = [this] (int i) { selectSlot (i); };
        slots[s]->onStep = [this] (int i, int d) { library.step (i, d); };
        addAndMakeVisible (*slots[s]);
    }

    // Header
    libraryButton.getProperties().set ("style", "sidebar");
    libraryButton.setTooltip ("Show / hide library");
    libraryButton.setToggleState (processor.showLibrary, juce::dontSendNotification);
    libraryButton.onClick = [this] { setLibraryVisible (libraryButton.getToggleState()); };
    addAndMakeVisible (libraryButton);

    chromaticButton.getProperties().set ("style", "keys");
    chromaticButton.getProperties().set ("accent", accent.toString());
    chromaticButton.setTooltip ("Chromatic: play all four slots across the keyboard (C3 = original pitch), polyphonic. "
                                "Off: every key plays the same pitch.");
    addAndMakeVisible (chromaticButton);
    chromaticAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::chromatic, chromaticButton);

    keySyncButton.getProperties().set ("accent", text.toString());
    keySyncButton.getProperties().set ("style", "labelled");
    keySyncButton.setTooltip ("Key Sync: pitch the other slots to the root note of the lead slot");
    addAndMakeVisible (keySyncButton);
    keySyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::keySync, keySyncButton);

    for (int s = 0; s < numSlots; ++s)
    {
        auto& b = leadButtons[s];
        b.setButtonText (slotLetter (s));
        b.getProperties().set ("fill", slot (s).toString());
        b.setTooltip ("Slot " + slotLetter (s) + " is the lead: the others are pitched to its note");
        b.onClick = [this, s]
        {
            auto* prm = processor.apvts.getParameter (ParamIDs::lead);
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) s));
            prm->endChangeGesture();
            updateLeadButtons();
        };
        addAndMakeVisible (b);
    }

    syncModeBox.addItemList (syncModes, 1);
    syncModeBox.setTooltip ("Nearest: pitch to the lead note in the closest octave (keeps the character). "
                            "Exact: pitch to exactly the same note.");
    addAndMakeVisible (syncModeBox);
    syncModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, ParamIDs::syncMode, syncModeBox);

    // Module
    mAmp    = { "AMP ENV",    env,       {} };
    mClip   = { "CLIPPER",    clip,      {} };
    mOut    = { "OUTPUT",     accent2,   {} };

    auto& a = processor.apvts;
    kAttack.setup    (*this, a, ParamIDs::attack,    "Attack",  env);
    kHold.setup      (*this, a, ParamIDs::hold,      "Hold",    env);
    kDecay.setup     (*this, a, ParamIDs::decay,     "Decay",   env);
    kRelease.setup   (*this, a, ParamIDs::release,   "Release", env);
    kClipDrive.setup (*this, a, ParamIDs::clipDrive, "Drive",   clip);
    kVelocity.setup  (*this, a, ParamIDs::velocity,  "Velocity", accent2);
    kGain.setup      (*this, a, ParamIDs::gain,      "Gain",    accent2);

    clipOn.getProperties().set ("accent", clip.toString());
    clipHard.getProperties().set ("accent", clip.toString());
    clipHard.getProperties().set ("style", "segment");
    addAndMakeVisible (clipOn);
    addAndMakeVisible (clipHard);
    clipOnAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ParamIDs::clipOn, clipOn);
    clipHardAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ParamIDs::clipHard, clipHard);
    addAndMakeVisible (clipMeter);
    addAndMakeVisible (outMeter);

    processor.addChangeListener (this);

    library.setVisible (processor.showLibrary);
    const int libExtra = libWidth + margin;
    setResizable (true, true);
    setResizeLimits (processor.showLibrary ? 1180 : 880, 680, 2200, 1400);
    setSize (processor.showLibrary ? 1056 + libExtra : 1056, 760);

    updateSlotInfo();
    updateLeadButtons();
    startTimerHz (30);
}

QuadShooterEditor::~QuadShooterEditor()
{
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void QuadShooterEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Logo: vier Quadrate in den Slot-Farben
    {
        const float x0 = (float) logoArea.getX(), y0 = (float) logoArea.getY() + 9.0f, q = 10.0f, gp = 2.5f;
        for (int s = 0; s < numSlots; ++s)
        {
            g.setColour (slot (s));
            g.fillRoundedRectangle (x0 + (float) (s % 2) * (q + gp), y0 + (float) (s / 2) * (q + gp), q, q, 2.5f);
        }
    }
    g.setColour (text);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText ("QUAD SHOOTER", logoArea.getX() + 34, logoArea.getY() + 4, 220, 22, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("by Dani Koenig", logoArea.getX() + 34, logoArea.getY() + 24, 200, 14, juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("LEAD", leadLabel, juce::Justification::centredRight);

    for (auto* m : { &mAmp, &mClip, &mOut })
    {
        auto r = m->bounds.toFloat();
        g.setColour (card);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (cardBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (m->colour);
        g.fillEllipse (r.getX() + 14.0f, r.getY() + 15.0f, 7.0f, 7.0f);
        g.setColour (text);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (m->title, (int) r.getX() + 28, (int) r.getY() + 10, 160, 18, juce::Justification::centredLeft);
    }
}

void QuadShooterEditor::resized()
{
    auto r = getLocalBounds().reduced (margin);

    if (library.isVisible())
    {
        library.setBounds (r.removeFromLeft (libWidth));
        r.removeFromLeft (margin);
    }

    // Header
    auto header = r.removeFromTop (headerH);
    libraryButton.setBounds (header.removeFromLeft (36).reduced (0, 4));
    header.removeFromLeft (12);
    logoArea = header.removeFromLeft (210);

    chromaticButton.setBounds (header.removeFromRight (118).reduced (0, 6));
    header.removeFromRight (16);
    syncModeBox.setBounds (header.removeFromRight (100).reduced (0, 6));
    header.removeFromRight (8);
    for (int s = numSlots - 1; s >= 0; --s)
    {
        leadButtons[s].setBounds (header.removeFromRight (28).reduced (0, 6));
        header.removeFromRight (4);
    }
    leadLabel = header.removeFromRight (40);
    header.removeFromRight (8);
    keySyncButton.setBounds (header.removeFromRight (96).reduced (0, 6));

    r.removeFromTop (gap);

    // Module unten
    auto modules = r.removeFromBottom (moduleH);
    r.removeFromBottom (gap);

    // Slots + XY-Pad: A | XY | B oben, C | XY | D unten
    const int padSize = juce::jmax (200, juce::jmin (r.getHeight(), (r.getWidth() - gap) / 2));
    const int slotSize = (padSize - gap) / 2;
    const int totalW = padSize + 2 * slotSize + 2 * gap;
    auto main = r.withSizeKeepingCentre (totalW, padSize);
    auto left = main.removeFromLeft (slotSize);
    main.removeFromLeft (gap);
    auto right = main.removeFromRight (slotSize);
    main.removeFromRight (gap);
    xyPad.setBounds (main);
    slots[0]->setBounds (left.removeFromTop (slotSize));
    slots[2]->setBounds (left.removeFromBottom (slotSize));
    slots[1]->setBounds (right.removeFromTop (slotSize));
    slots[3]->setBounds (right.removeFromBottom (slotSize));

    // Modul-Reihe: Amp 4 | Clip 3 | Out 2.6
    const float units[] = { 4.0f, 3.0f, 2.6f };
    const float unit = (float) (modules.getWidth() - 2 * gap) / (4.0f + 3.0f + 2.6f);
    Module* ms[] = { &mAmp, &mClip, &mOut };
    for (int i = 0; i < 3; ++i)
    {
        ms[i]->bounds = i == 2 ? modules : modules.removeFromLeft ((int) (unit * units[i]));
        if (i < 2) modules.removeFromLeft (gap);
    }

    auto inner = [] (juce::Rectangle<int> b) { return b.reduced (10, 8).withTrimmedTop (26); };

    Knob::layoutRow (inner (mAmp.bounds), { &kAttack, &kHold, &kDecay, &kRelease });

    {
        auto c = inner (mClip.bounds);
        clipMeter.setBounds (c.removeFromRight (22).withTrimmedTop (4));
        c.removeFromRight (10);
        auto toggles = c.removeFromRight (juce::jmin (92, c.getWidth() / 2));
        const int ty = toggles.getCentreY();
        clipOn.setBounds (toggles.getX(), ty - 34, toggles.getWidth(), 28);
        clipHard.setBounds (toggles.getX(), ty + 6, toggles.getWidth(), 28);
        c.removeFromRight (6);
        Knob::layoutRow (c, { &kClipDrive });
    }
    {
        auto o = inner (mOut.bounds);
        outMeter.setBounds (o.removeFromRight (22).withTrimmedTop (4));
        o.removeFromRight (10);
        Knob::layoutRow (o, { &kVelocity, &kGain });
    }
}

//==============================================================================
void QuadShooterEditor::selectSlot (int s)
{
    if (processor.activeSlot.load() == s)
        return;
    processor.activeSlot = s;
    library.activeSlotChanged();
    for (auto& sv : slots)
        sv->refresh();
}

void QuadShooterEditor::setLibraryVisible (bool show)
{
    if (show == processor.showLibrary)
        return;
    processor.showLibrary = show;
    library.setVisible (show);
    const int libExtra = libWidth + margin;
    setResizeLimits (show ? 1180 : 880, 680, 2200, 1400);
    setSize (juce::jlimit (show ? 1180 : 880, 2200, getWidth() + (show ? libExtra : -libExtra)), getHeight());
    resized();
    repaint();
}

void QuadShooterEditor::updateSlotInfo()
{
    for (int s = 0; s < numSlots; ++s)
    {
        slots[s]->samplesChanged();
        auto folder = library.folderOfLoaded (s);
        if (folder.isEmpty())
            folder = processor.getSlotFile (s).getParentDirectory().getFileName();
        slots[s]->setFolderText (folder.replace ("/", " / "));
    }
}

void QuadShooterEditor::updateLeadButtons()
{
    const int lead = (int) std::round (processor.apvts.getRawParameterValue (ParamIDs::lead)->load());
    const bool sync = processor.apvts.getRawParameterValue (ParamIDs::keySync)->load() > 0.5f;
    for (int s = 0; s < numSlots; ++s)
    {
        leadButtons[s].setToggleState (s == lead, juce::dontSendNotification);
        leadButtons[s].setAlpha (sync ? 1.0f : 0.45f);
    }
    syncModeBox.setAlpha (sync ? 1.0f : 0.45f);
}

bool QuadShooterEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        juce::File file (f);
        if (file.isDirectory() || QuadShooterProcessor::isSupportedFile (file))
            return true;
    }
    return false;
}

void QuadShooterEditor::filesDropped (const juce::StringArray& files, int, int)
{
    // Ordner -> Library-Index, einzelne Dateien -> aktiver Slot (bzw. die folgenden Slots)
    int target = processor.activeSlot.load();
    for (auto& f : files)
    {
        juce::File file (f);
        if (file.isDirectory())
            processor.getLibrary().addFolder (file);
        else if (target < numSlots && QuadShooterProcessor::isSupportedFile (file) && processor.loadSample (target, file))
            ++target;
    }
}

void QuadShooterEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateSlotInfo();
    library.repaint();
}

void QuadShooterEditor::timerCallback()
{
    for (auto& s : slots)
        s->refresh();
    xyPad.refresh();
    updateLeadButtons();
    clipMeter.setValue (processor.getClipReductionDb());
    outMeter.setValue (processor.getOutputPeak());
    library.refreshStatus();
}
