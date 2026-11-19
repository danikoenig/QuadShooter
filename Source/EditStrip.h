#pragma once

#include "PluginProcessor.h"
#include "Knob.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Edit-Zeile fuer den aktiven Slot (wie bei XLN XO):
// Sample | Level | Pan | Envelope | Pitch | Tone | Cut | VelSens
class EditStrip : public juce::Component
{
public:
    explicit EditStrip (QuadShooterProcessor& p);

    void setSlot (int slot);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Group
    {
        juce::String title;
        std::vector<Knob*> knobs;
        juce::Rectangle<int> bounds;
    };

    QuadShooterProcessor& processor;
    int slot = -1;

    Knob kStart, kEnd, kLevel, kPan, kHold, kDecay, kTune, kTone, kLowCut, kHighCut, kVel;
    std::vector<Group> groups;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditStrip)
};
