#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <vector>

// Gemeinsamer One-Shot-Index fuer alle Plugin-Instanzen.
// Wird unter ~/Library/Application Support/Quad Shooter/library.json (beim ersten Start wird der One-Shooter-Index uebernommen) gespeichert.
class SampleLibrary : public juce::ChangeBroadcaster,
                      private juce::Thread
{
public:
    struct Entry
    {
        juce::File file;
        juce::String name;
        juce::String folder;     // Name des hinzugefuegten Wurzelordners
        double seconds = 0.0;
    };

    SampleLibrary();
    ~SampleLibrary() override;

    void addFolder (const juce::File& folder);   // scannt im Hintergrund
    void clear();

    std::vector<Entry> getEntries() const;
    bool isScanning() const                 { const juce::ScopedLock sl (lock); return scannerBusy; }
    int  getFilesChecked() const            { return filesChecked.load(); }
    int  getOneShotsFound() const           { return oneShotsFound.load(); }
    juce::String getCurrentFolderName() const;

    // One-Shot-Erkennung (auch fuer Tests oeffentlich)
    static bool isOneShot (juce::AudioFormatReader& reader, const juce::String& fileName, double& secondsOut);

private:
    void run() override;
    void scanFolder (const juce::File& root);
    void load();
    void save() const;
    static juce::File indexFile();

    juce::AudioFormatManager formatManager;

    mutable juce::CriticalSection lock;
    std::vector<Entry> entries;
    juce::Array<juce::File> pendingFolders;
    juce::String currentFolder;
    bool scannerBusy = false;

    std::atomic<int> filesChecked { 0 }, oneShotsFound { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleLibrary)
};
