#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// XY-Pad in der Mitte: mischt die vier Slots (A oben links, B oben rechts, C unten links, D unten rechts)
class XYPad : public juce::Component,
              public juce::SettableTooltipClient
{
public:
    explicit XYPad (QuadShooterProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void refresh();   // vom Editor-Timer

private:
    juce::Rectangle<float> padArea() const;
    void setFromMouse (juce::Point<float> pos);
    void setXY (float x, float y);

    QuadShooterProcessor& processor;
    juce::RangedAudioParameter* px;
    juce::RangedAudioParameter* py;
    float lastX = -1.0f, lastY = -1.0f;
    juce::uint32 lastMuteMask = 0xff;
    bool dragging = false;
    friend struct ReelAccess;   // nur fuer den Reel-Renderer
};
