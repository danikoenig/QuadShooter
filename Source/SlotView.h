#pragma once

#include "PluginProcessor.h"
#include "Knob.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Ein quadratischer Sample-Slot (A..D): Name, Waveform, erkannter Ton, Level.
// Bearbeitet wird in der Edit-Zeile (EditStrip) unter den Slots.
class SlotView : public juce::Component,
                 public juce::FileDragAndDropTarget,
                 public juce::DragAndDropTarget
{
public:
    SlotView (QuadShooterProcessor& p, int index);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override             { dragHover = false; repaint(); }

    // Drag aus der Library
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    void loadAndSelect (const juce::File& file);

    void refresh();            // vom Editor-Timer
    void samplesChanged();     // nach Laden / Root-Aenderung
    void setFolderText (const juce::String& t) { if (t != folderText) { folderText = t; repaint(); } }

    std::function<void (int)> onSelect;
    std::function<void (int, int)> onStep;

private:
    void openFileChooser();
    void showRootMenu();
    void showSlotMenu();
    juce::String noteText() const;

    QuadShooterProcessor& processor;
    const int index;
    const juce::Colour colour;

    WaveformPeaks peaks;
    juce::String folderText;
    float playPos = -1.0f, startPos = 0.0f, endPos = 1.0f, lastSync = 0.0f, lastRoot = noPitch;
    bool wasActive = false, wasLead = false, wasMuted = false, dragHover = false;

    juce::TextButton prevButton { "<" }, nextButton { ">" }, loadButton { "Load" }, muteButton { "M" };
    juce::Slider levelSlider;
    juce::Label levelValue;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> badge, nameArea, waveArea, notePill, levelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotView)
};
