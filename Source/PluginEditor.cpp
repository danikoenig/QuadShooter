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
    constexpr int moduleH = 150;
    constexpr int editH = 112;
}

QuadShooterEditor::QuadShooterEditor (QuadShooterProcessor& p)
    : AudioProcessorEditor (&p), processor (p), library (p), xyPad (p), editStrip (p),
      clipMeter (clip, true), outMeter (accent2, false), limMeter (accent2, true)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (library);
    addAndMakeVisible (xyPad);
    addAndMakeVisible (editStrip);

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
    mAmp    = { "AMP",        env,       {} };
    mDelay  = { "DELAY",      delayCol,  {} };
    mReverb = { "REVERB",     reverbCol, {} };
    mClip   = { "CLIPPER",    clip,      {} };
    mOut    = { "OUTPUT",     accent2,   {} };

    auto& a = processor.apvts;
    kAttack.setup    (*this, a, ParamIDs::attack,    "Attack",  env);
    kAmpDecay.setup  (*this, a, ParamIDs::ampDecay,  "Decay",   env);
    kAmpDecay.slider.setTooltip ("Decay for all slots together (on top of each slot's own decay). Max = off");
    kRelease.setup   (*this, a, ParamIDs::release,   "Release", env);
    kClipDrive.setup (*this, a, ParamIDs::clipDrive, "Drive",   clip);
    kGain.setup      (*this, a, ParamIDs::gain,      "Gain",    accent2);

    // Delay
    kDlyNote.setup (*this, a, ParamIDs::dlyNote, "Time",     delayCol);
    kDlyMs.setup   (*this, a, ParamIDs::dlyMs,   "Time",     delayCol);
    kDlyFb.setup   (*this, a, ParamIDs::dlyFb,   "Feedback", delayCol);
    kDlyMix.setup  (*this, a, ParamIDs::dlyMix,  "Mix",      delayCol);
    kDlyNote.slider.setTooltip ("Delay time synced to the DAW tempo");
    kDlyMs.slider.setTooltip ("Free delay time in ms");
    {
        const bool sync = a.getRawParameterValue (ParamIDs::dlySync)->load() > 0.5f;
        lastSync = sync;
        kDlyNote.slider.setVisible (sync);  kDlyNote.label.setVisible (sync);
        kDlyMs.slider.setVisible (! sync);  kDlyMs.label.setVisible (! sync);
    }
    for (auto* b : { &dlyOn, &revOn })
        addAndMakeVisible (b);
    dlyOn.getProperties().set ("accent", delayCol.toString());
    revOn.getProperties().set ("accent", reverbCol.toString());
    dlyPing.getProperties().set ("style", "segment");
    dlyPing.getProperties().set ("left", "MONO");
    dlyPing.getProperties().set ("right", "PING");
    dlyPing.getProperties().set ("accent", delayCol.toString());
    dlyPing.setTooltip ("Mono: echoes stay in place. Ping: echoes bounce left / right.");
    dlySync.getProperties().set ("style", "labelled");
    dlySync.getProperties().set ("accent", delayCol.toString());
    dlySync.setTooltip ("On: time in note values (follows the DAW tempo). Off: free time in ms.");
    addAndMakeVisible (dlyPing);
    addAndMakeVisible (dlySync);
    dlyOnAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ParamIDs::dlyOn, dlyOn);
    dlyPingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ParamIDs::dlyPing, dlyPing);
    dlySyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ParamIDs::dlySync, dlySync);

    // Reverb
    kRevSize.setup (*this, a, ParamIDs::revSize, "Size", reverbCol);
    kRevDamp.setup (*this, a, ParamIDs::revDamp, "Damp", reverbCol);
    kRevMix.setup  (*this, a, ParamIDs::revMix,  "Mix",  reverbCol);
    kRevSize.slider.setTooltip ("Small = tight room for drums, big = wide space for pads");
    kRevDamp.slider.setTooltip ("How dark the reverb tail gets");
    revOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ParamIDs::revOn, revOn);

    // Limiter
    limiterButton.getProperties().set ("style", "labelled");
    limiterButton.getProperties().set ("accent", accent2.toString());
    limiterButton.setTooltip ("Peak limiter at the very end: the output never goes above -1 dB. "
                              "Turn Gain up until GR shows 1-3 dB.");
    addAndMakeVisible (limiterButton);
    limiterAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ParamIDs::limiter, limiterButton);

    // Pfeile
    for (int m = 0; m < numFx; ++m)
    {
        moveLeft[m].setButtonText ("<");
        moveRight[m].setButtonText (">");
        moveLeft[m].setTooltip ("Move left in the FX chain");
        moveRight[m].setTooltip ("Move right in the FX chain");
        moveLeft[m].onClick  = [this, m] { moveFx (m, -1); };
        moveRight[m].onClick = [this, m] { moveFx (m, +1); };
        addAndMakeVisible (moveLeft[m]);
        addAndMakeVisible (moveRight[m]);
    }

    clipOn.getProperties().set ("accent", clip.toString());
    clipHard.getProperties().set ("accent", clip.toString());
    clipHard.getProperties().set ("style", "segment");
    addAndMakeVisible (clipOn);
    addAndMakeVisible (clipHard);
    clipOnAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ParamIDs::clipOn, clipOn);
    clipHardAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ParamIDs::clipHard, clipHard);
    clipAuto.getProperties().set ("style", "labelled");
    clipAuto.getProperties().set ("accent", clip.toString());
    clipAuto.setTooltip ("Auto Gain: turns the level back down by the drive amount, so you hear the saturation, not just more volume");
    addAndMakeVisible (clipAuto);
    clipAutoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (a, ParamIDs::clipAuto, clipAuto);
    addAndMakeVisible (clipMeter);
    addAndMakeVisible (outMeter);
    addAndMakeVisible (limMeter);

    processor.addChangeListener (this);

    library.setVisible (processor.showLibrary);
    const int libExtra = libWidth + margin;
    setResizable (true, true);
    setResizeLimits (processor.showLibrary ? 1380 : 1076, 760, 2400, 1500);
    setSize (processor.showLibrary ? 1180 + libExtra : 1180, 840);

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
    g.drawText ("by Dani Koenig   v" JucePlugin_VersionString, logoArea.getX() + 34, logoArea.getY() + 24, 200, 14, juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("LEAD", leadLabel, juce::Justification::centredRight);

    for (auto* m : { &mAmp, &mDelay, &mReverb, &mClip, &mOut })
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
    frameOverlay.attach (*this, 0.0f);
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

    // Module unten, darueber die Edit-Zeile des aktiven Slots
    auto modules = r.removeFromBottom (moduleH);
    r.removeFromBottom (gap);
    auto editArea = r.removeFromBottom (editH);
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
    editStrip.setBounds (editArea.withSizeKeepingCentre (totalW, editH));

    layoutModules (modules);
}

void QuadShooterEditor::layoutModules (juce::Rectangle<int> row)
{
    // Amp | 3 FX in gewaehlter Reihenfolge | Output
    const int orderIdx = juce::jlimit (0, 5, (int) std::round (processor.apvts.getRawParameterValue (ParamIDs::fxOrder)->load()));
    lastOrder = orderIdx;
    Module* fxMods[numFx] = { &mDelay, &mReverb, &mClip };
    const float fxUnits[numFx] = { 4.2f, 3.2f, 3.0f };

    std::vector<std::pair<Module*, float>> seq;
    seq.push_back ({ &mAmp, 3.0f });
    for (int k = 0; k < numFx; ++k)
    {
        const int m = fxOrders[orderIdx][k];
        seq.push_back ({ fxMods[m], fxUnits[m] });
    }
    seq.push_back ({ &mOut, 2.8f });

    float total = 0.0f;
    for (auto& e : seq) total += e.second;
    const float unit = (float) (row.getWidth() - gap * ((int) seq.size() - 1)) / total;
    for (size_t i = 0; i < seq.size(); ++i)
    {
        seq[i].first->bounds = i + 1 == seq.size() ? row : row.removeFromLeft ((int) (unit * seq[i].second));
        if (i + 1 < seq.size()) row.removeFromLeft (gap);
    }

    auto inner = [] (juce::Rectangle<int> b) { return b.reduced (10, 8).withTrimmedTop (26); };

    // Kopfzeile der FX: [ON] [<] [>]
    juce::ToggleButton* onButtons[numFx] = { &dlyOn, &revOn, &clipOn };
    for (int m = 0; m < numFx; ++m)
    {
        auto h = fxMods[m]->bounds.reduced (10, 0).withHeight (34).withTrimmedTop (8);
        moveRight[m].setBounds (h.removeFromRight (22).withSizeKeepingCentre (22, 20));
        h.removeFromRight (3);
        moveLeft[m].setBounds (h.removeFromRight (22).withSizeKeepingCentre (22, 20));
        h.removeFromRight (8);
        onButtons[m]->setBounds (h.removeFromRight (58).withSizeKeepingCentre (58, 22));
        const int pos = [&] { for (int k = 0; k < numFx; ++k) if (fxOrders[orderIdx][k] == m) return k; return 0; }();
        moveLeft[m].setEnabled (pos > 0);
        moveRight[m].setEnabled (pos < numFx - 1);
    }

    Knob::layoutRow (inner (mAmp.bounds), { &kAttack, &kAmpDecay, &kRelease });

    {
        auto d = inner (mDelay.bounds);
        auto col = d.removeFromLeft (juce::jmin (104, d.getWidth() / 4));
        const int cy = col.getCentreY();
        dlyPing.setBounds (col.getX(), cy - 30, col.getWidth(), 26);
        dlySync.setBounds (col.getX(), cy + 4, col.getWidth(), 26);
        d.removeFromLeft (6);
        auto timeCol = d.removeFromLeft (d.getWidth() / 3);
        kDlyNote.setBounds (timeCol);
        kDlyMs.setBounds (timeCol);
        Knob::layoutRow (d, { &kDlyFb, &kDlyMix });
    }

    Knob::layoutRow (inner (mReverb.bounds), { &kRevSize, &kRevDamp, &kRevMix });

    {
        auto c = inner (mClip.bounds);
        clipMeter.setBounds (c.removeFromRight (22).withTrimmedTop (4));
        c.removeFromRight (10);
        auto toggles = c.removeFromRight (juce::jmin (100, c.getWidth() / 2));
        clipHard.setBounds (toggles.getX(), toggles.getCentreY() - 30, toggles.getWidth(), 26);
        clipAuto.setBounds (toggles.getX(), toggles.getCentreY() + 4, toggles.getWidth(), 26);
        c.removeFromRight (6);
        Knob::layoutRow (c, { &kClipDrive });
    }
    {
        auto o = inner (mOut.bounds);
        outMeter.setBounds (o.removeFromRight (22).withTrimmedTop (4));
        o.removeFromRight (10);
        limMeter.setBounds (o.removeFromRight (22).withTrimmedTop (4));
        o.removeFromRight (8);
        auto lc = o.removeFromRight (juce::jmin (78, o.getWidth() / 2));
        limiterButton.setBounds (lc.getX(), lc.getCentreY() - 13, lc.getWidth(), 26);
        o.removeFromRight (4);
        Knob::layoutRow (o, { &kGain });
    }
}

void QuadShooterEditor::moveFx (int module, int delta)
{
    auto* prm = processor.apvts.getParameter (ParamIDs::fxOrder);
    const int cur = juce::jlimit (0, 5, (int) std::round (prm->convertFrom0to1 (prm->getValue())));
    int order[3] = { fxOrders[cur][0], fxOrders[cur][1], fxOrders[cur][2] };
    int pos = 0;
    for (int k = 0; k < numFx; ++k) if (order[k] == module) pos = k;
    const int np = pos + delta;
    if (np < 0 || np >= numFx)
        return;
    std::swap (order[pos], order[np]);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) fxOrderIndexFor (order)));
    prm->endChangeGesture();
    resized();
    repaint();
}

//==============================================================================
void QuadShooterEditor::selectSlot (int s)
{
    if (processor.activeSlot.load() == s)
        return;
    processor.activeSlot = s;
    library.activeSlotChanged();
    editStrip.setSlot (s);
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
    setResizeLimits (show ? 1380 : 1076, 760, 2400, 1500);
    setSize (juce::jlimit (show ? 1380 : 1076, 2400, getWidth() + (show ? libExtra : -libExtra)), getHeight());
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

    const int order = (int) std::round (processor.apvts.getRawParameterValue (ParamIDs::fxOrder)->load());
    if (order != lastOrder)
    {
        resized();
        repaint();
    }
    const bool sync = processor.apvts.getRawParameterValue (ParamIDs::dlySync)->load() > 0.5f;
    if (sync != lastSync)
    {
        lastSync = sync;
        kDlyNote.slider.setVisible (sync);  kDlyNote.label.setVisible (sync);
        kDlyMs.slider.setVisible (! sync);  kDlyMs.label.setVisible (! sync);
    }
    clipMeter.setValue (processor.getClipReductionDb());
    outMeter.setValue (processor.getOutputPeak());
    limMeter.setValue (processor.getLimitReductionDb());
    library.refreshStatus();
}
