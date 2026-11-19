#pragma once

#include "PluginProcessor.h"
#include "Look.h"
#include "Knob.h"
#include "SlotView.h"
#include "XYPad.h"
#include "EditStrip.h"
#include "LibraryPanel.h"
#include <juce_audio_utils/juce_audio_utils.h>

class QuadShooterEditor : public juce::AudioProcessorEditor,
                          public juce::FileDragAndDropTarget,
                          public juce::DragAndDropContainer,
                          private juce::ChangeListener,
                          private juce::Timer
{
public:
    explicit QuadShooterEditor (QuadShooterProcessor&);
    ~QuadShooterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void selectSlot (int slot);
    void setLibraryVisible (bool show);
    void updateSlotInfo();
    void updateLeadButtons();

    struct Module
    {
        juce::String title;
        juce::Colour colour;
        juce::Rectangle<int> bounds;
    };

    QuadShooterProcessor& processor;
    QuadLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 500 };

    LibraryPanel library;
    std::unique_ptr<SlotView> slots[numSlots];
    XYPad xyPad;
    EditStrip editStrip;

    // Header
    juce::ToggleButton libraryButton, chromaticButton, keySyncButton { "KEY SYNC" };
    juce::TextButton leadButtons[numSlots];
    juce::ComboBox syncModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chromaticAtt, keySyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncModeAtt;
    juce::Rectangle<int> logoArea, leadLabel;

    // Module
    Knob kAttack, kRelease;
    Knob kClipDrive;
    Knob kGain;
    juce::ToggleButton clipOn { "ON" }, clipHard { "HARD" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> clipOnAtt, clipHardAtt;
    Meter clipMeter, outMeter;

    Module mAmp, mClip, mOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuadShooterEditor)
};
