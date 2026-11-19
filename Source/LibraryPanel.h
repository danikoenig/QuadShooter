#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Seitenleiste: One-Shot-Index als Ordner-Browser mit Suche und Vorhoeren
class LibraryPanel : public juce::Component,
                     private juce::ListBoxModel,
                     private juce::ChangeListener
{
public:
    explicit LibraryPanel (QuadShooterProcessor& p);
    ~LibraryPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshStatus();

    // Naechstes/vorheriges Sample im Ordner des Samples in diesem Slot (Pfeile im Slot)
    void step (int slot, int delta);
    // Ordner des Samples im Slot relativ zur Library (leer wenn nicht im Index)
    juce::String folderOfLoaded (int slot) const;
    // Aktiver Slot hat gewechselt: Liste neu markieren
    void activeSlotChanged();
    // Alle One-Shots eines importierten Ordners, Unterordner aufgeloest
    std::vector<SampleLibrary::Entry> siblingsOf (const juce::String& root) const;

private:
    struct Item
    {
        enum Type { up, folder, sample } type = sample;
        juce::String label, path;   // path: Ordnerpfad (folder/up)
        int count = 0;              // Samples im Ordner
        SampleLibrary::Entry entry;
    };

    // ListBoxModel
    int  getNumRows() override { return (int) items.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void deleteKeyPressed (int) override { goUp(); }

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuild();
    void openFolder (const juce::String& path);
    void goUp();
    void audition (int row);
    bool loadAndPlay (int slot, const juce::File& file);
    int  active() const;
    void chooseFolder();
    void revealLoaded();
    juce::File loadedFile() const;

    QuadShooterProcessor& processor;
    std::vector<SampleLibrary::Entry> all;
    std::vector<Item> items;
    juce::String currentPath;

    juce::TextEditor search;
    juce::ListBox list { "Library", this };
    juce::TextButton addButton { "+ Folder" }, clearButton { "Clear" }, upButton { "<" };
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> crumbArea;
    int lastAuditioned = -1;
    bool suppressAudition = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryPanel)
};
