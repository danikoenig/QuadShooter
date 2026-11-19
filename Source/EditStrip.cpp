#include "EditStrip.h"

using namespace ShooterColours;

EditStrip::EditStrip (QuadShooterProcessor& p) : processor (p)
{
    // Erst an Slot A binden, setSlot haengt um
    auto& a = processor.apvts;
    const auto c = ShooterColours::slot (0);
    kStart.setup   (*this, a, ParamIDs::start (0),   "Start", c, true);
    kEnd.setup     (*this, a, ParamIDs::end (0),     "End",   c, true);
    kPan.setup     (*this, a, ParamIDs::pan (0),     "",      c, true);
    kHold.setup    (*this, a, ParamIDs::hold (0),    "Hold",  c, true);
    kDecay.setup   (*this, a, ParamIDs::decay (0),   "Decay", c, true);
    kTune.setup    (*this, a, ParamIDs::tune (0),    "",      c, true);
    kTone.setup    (*this, a, ParamIDs::tone (0),    "",      c, true);
    kLowCut.setup  (*this, a, ParamIDs::lowCut (0),  "Low",   c, true);
    kHighCut.setup (*this, a, ParamIDs::highCut (0), "High",  c, true);
    kVel.setup     (*this, a, ParamIDs::vel (0),     "",      c, true);

    for (auto* k : { &kPan, &kTune, &kTone })
        k->slider.getProperties().set ("bipolar", true);

    kEnd.slider.setTooltip ("Where the sample stops");
    kHold.slider.setTooltip ("Full level before the decay starts");
    kDecay.slider.setTooltip ("Fade-out time of this slot (Max = play to the end)");
    kTune.slider.setTooltip ("Pitch of this slot in semitones (added on top of Key Sync)");
    kTone.slider.setTooltip ("Tilt: left = darker, right = brighter");
    kLowCut.slider.setTooltip ("Cuts the lows below this frequency");
    kHighCut.slider.setTooltip ("Cuts the highs above this frequency");
    kVel.slider.setTooltip ("How much velocity changes the level of this slot");

    groups = { { "SAMPLE",   { &kStart, &kEnd },       {} },
               { "PAN",      { &kPan },                {} },
               { "ENVELOPE", { &kHold, &kDecay },      {} },
               { "PITCH",    { &kTune },               {} },
               { "TONE",     { &kTone },               {} },
               { "CUT",      { &kLowCut, &kHighCut },  {} },
               { "VELSENS",  { &kVel },                {} } };

    setSlot (processor.activeSlot.load());
}

void EditStrip::setSlot (int s)
{
    s = juce::jlimit (0, numSlots - 1, s);
    if (s == slot)
        return;
    slot = s;
    auto& a = processor.apvts;
    const auto c = ShooterColours::slot (s);
    kStart.rebind   (a, ParamIDs::start (s),   c);
    kEnd.rebind     (a, ParamIDs::end (s),     c);
    kPan.rebind     (a, ParamIDs::pan (s),     c);
    kHold.rebind    (a, ParamIDs::hold (s),    c);
    kDecay.rebind   (a, ParamIDs::decay (s),   c);
    kTune.rebind    (a, ParamIDs::tune (s),    c);
    kTone.rebind    (a, ParamIDs::tone (s),    c);
    kLowCut.rebind  (a, ParamIDs::lowCut (s),  c);
    kHighCut.rebind (a, ParamIDs::highCut (s), c);
    kVel.rebind     (a, ParamIDs::vel (s),     c);
    repaint();
}

void EditStrip::paint (juce::Graphics& g)
{
    const auto c = ShooterColours::slot (juce::jmax (0, slot));
    auto r = getLocalBounds().toFloat();
    g.setColour (card);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (c.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.2f);

    // Slot-Badge links
    auto badge = juce::Rectangle<float> (14.0f, 12.0f, 28.0f, 28.0f);
    g.setColour (c);
    g.fillRoundedRectangle (badge, 6.0f);
    g.setColour (bgBottom);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (slotLetter (slot), badge, juce::Justification::centred);
    g.setColour (text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("EDIT", 14, 44, 60, 16, juce::Justification::centredLeft);

    // Gruppen-Titel mit Trennlinie (wie XO)
    for (auto& grp : groups)
    {
        auto b = grp.bounds;
        g.setColour (textDim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (grp.title, b.removeFromTop (16), juce::Justification::centred);
        g.setColour (cardBorder.brighter (0.1f));
        g.drawHorizontalLine (grp.bounds.getY() + 19, (float) grp.bounds.getX() + 6.0f, (float) grp.bounds.getRight() - 6.0f);
    }
}

void EditStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromLeft (64);

    int units = 0;
    for (auto& grp : groups)
        units += (int) grp.knobs.size();
    const int unitW = r.getWidth() / juce::jmax (1, units);

    for (auto& grp : groups)
    {
        grp.bounds = r.removeFromLeft (unitW * (int) grp.knobs.size());
        auto k = grp.bounds.withTrimmedTop (22);
        const int w = k.getWidth() / (int) grp.knobs.size();
        for (auto* knob : grp.knobs)
            knob->setBounds (k.removeFromLeft (w), 13);
    }
}
