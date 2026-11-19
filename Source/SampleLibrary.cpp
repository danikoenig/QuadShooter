#include "SampleLibrary.h"
#include <cmath>

namespace
{
    constexpr double maxOneShotSeconds = 3.0;
    constexpr double minOneShotSeconds = 0.02;
    const char* audioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf";
}

SampleLibrary::SampleLibrary() : juce::Thread ("Quad Shooter Library Scan")
{
    formatManager.registerBasicFormats();
    load();
}

SampleLibrary::~SampleLibrary()
{
    stopThread (4000);
}

static juce::File appDataDir (const char* name)
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support").getChildFile (name);
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile (name);
   #endif
}

juce::File SampleLibrary::indexFile()
{
    return appDataDir ("Quad Shooter").getChildFile ("library.json");
}

void SampleLibrary::load()
{
    auto f = indexFile();
    if (! f.existsAsFile())
        f = appDataDir ("One Shooter").getChildFile ("library.json");   // Index von One Shooter uebernehmen
    if (! f.existsAsFile())
        return;

    auto json = juce::JSON::parse (f);
    if (auto* arr = json.getProperty ("entries", {}).getArray())
    {
        const juce::ScopedLock sl (lock);
        for (auto& v : *arr)
        {
            Entry e;
            e.file = juce::File (v.getProperty ("path", {}).toString());
            e.name = e.file.getFileNameWithoutExtension();
            e.folder = v.getProperty ("folder", {}).toString();
            e.seconds = (double) v.getProperty ("seconds", 0.0);
            if (e.file.getFullPathName().isNotEmpty())
                entries.push_back (e);
        }
    }
}

void SampleLibrary::save() const
{
    juce::Array<juce::var> arr;
    {
        const juce::ScopedLock sl (lock);
        for (auto& e : entries)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("path", e.file.getFullPathName());
            o->setProperty ("folder", e.folder);
            o->setProperty ("seconds", e.seconds);
            arr.add (juce::var (o));
        }
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("entries", arr);

    auto f = indexFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (juce::var (root), false));
}

void SampleLibrary::addFolder (const juce::File& folder)
{
    if (! folder.isDirectory())
        return;

    bool needsStart = false;
    {
        const juce::ScopedLock sl (lock);
        pendingFolders.addIfNotAlreadyThere (folder);
        needsStart = ! scannerBusy;
        scannerBusy = true;
    }

    if (needsStart)
    {
        stopThread (4000);   // falls der alte Thread gerade endet
        filesChecked = 0;
        oneShotsFound = 0;
        startThread (juce::Thread::Priority::low);
    }
    sendChangeMessage();
}

void SampleLibrary::clear()
{
    signalThreadShouldExit();
    stopThread (4000);
    {
        const juce::ScopedLock sl (lock);
        entries.clear();
        pendingFolders.clear();
        scannerBusy = false;
    }
    save();
    sendChangeMessage();
}

std::vector<SampleLibrary::Entry> SampleLibrary::getEntries() const
{
    const juce::ScopedLock sl (lock);
    return entries;
}

juce::String SampleLibrary::getCurrentFolderName() const
{
    const juce::ScopedLock sl (lock);
    return currentFolder;
}

void SampleLibrary::run()
{
    for (;;)
    {
        juce::File next;
        {
            const juce::ScopedLock sl (lock);
            if (pendingFolders.isEmpty() || threadShouldExit())
            {
                currentFolder = {};
                scannerBusy = false;
                break;
            }
            next = pendingFolders.removeAndReturn (0);
            currentFolder = next.getFileName();
        }
        scanFolder (next);
        save();
        sendChangeMessage();
    }
    sendChangeMessage();
}

void SampleLibrary::scanFolder (const juce::File& root)
{
    std::vector<Entry> found;
    int sinceUpdate = 0;

    for (const auto& item : juce::RangedDirectoryIterator (root, true, audioWildcard, juce::File::findFiles))
    {
        if (threadShouldExit())
            return;

        const auto file = item.getFile();
        if (file.getFileName().startsWithChar ('.'))
            continue;   // macOS ._ Dateien

        ++filesChecked;

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr)
            continue;

        double secs = 0.0;
        if (isOneShot (*reader, file.getFileName(), secs))
        {
            found.push_back ({ file, file.getFileNameWithoutExtension(),
                                file.getParentDirectory().getRelativePathFrom (root.getParentDirectory()).replace ("\\", "/"), secs });
            ++oneShotsFound;
        }

        if (++sinceUpdate >= 50)
        {
            sinceUpdate = 0;
            sendChangeMessage();
        }
    }

    // In den Index uebernehmen (Duplikate ersetzen), sortiert nach Ordner/Name
    const juce::ScopedLock sl (lock);
    for (auto& f : found)
    {
        auto it = std::find_if (entries.begin(), entries.end(), [&] (const Entry& e) { return e.file == f.file; });
        if (it != entries.end()) *it = f;
        else                     entries.push_back (f);
    }
    std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b)
    {
        const int c = a.folder.compareIgnoreCase (b.folder);
        return c != 0 ? c < 0 : a.file.getFullPathName().compareNatural (b.file.getFullPathName()) < 0;
    });
}

bool SampleLibrary::isOneShot (juce::AudioFormatReader& reader, const juce::String& fileName, double& secondsOut)
{
    if (reader.sampleRate <= 0.0 || reader.lengthInSamples <= 0)
        return false;

    secondsOut = (double) reader.lengthInSamples / reader.sampleRate;
    if (secondsOut > maxOneShotSeconds || secondsOut < minOneShotSeconds)
        return false;

    // Dateiname: Loops ausschliessen
    const auto lower = fileName.toLowerCase();
    if (lower.contains ("loop") || lower.contains ("bpm"))
        return false;

    // Metadaten: Loop-Punkte oder ACID-Loop
    const auto& md = reader.metadataValues;
    if (md.getValue ("NumSampleLoops", "0").getIntValue() > 0)
        return false;
    if (md.containsKey (juce::WavAudioFormat::acidOneShot))
    {
        if (md.getValue (juce::WavAudioFormat::acidOneShot, "0").getIntValue() == 1)
            return true;   // explizit als One-Shot markiert
        return false;      // ACID-Loop
    }

    // Huellkurve: Transient am Anfang, danach Abklingen
    const int n = (int) reader.lengthInSamples;
    const int ch = (int) juce::jlimit (1u, 2u, reader.numChannels);
    juce::AudioBuffer<float> buf (ch, n);
    if (! reader.read (&buf, 0, n, 0, true, ch > 1))
        return false;

    const int win = juce::jmax (16, (int) (reader.sampleRate * 0.005));   // 5 ms
    const int numWin = n / win;
    if (numWin < 3)
        return true;   // extrem kurz, aber laenger als 20 ms: Click/Hit

    std::vector<float> rms ((size_t) numWin);
    float peak = 0.0f;
    int peakIdx = 0;
    for (int w = 0; w < numWin; ++w)
    {
        float sum = 0.0f;
        for (int c = 0; c < ch; ++c)
        {
            const float r = buf.getRMSLevel (c, w * win, win);
            sum += r * r;
        }
        rms[(size_t) w] = std::sqrt (sum / (float) ch);
        if (rms[(size_t) w] > peak) { peak = rms[(size_t) w]; peakIdx = w; }
    }

    if (peak < 0.001f)
        return false;   // stumm

    // Onset: erstes Fenster ueber 10 % des Peaks, muss innerhalb von 60 ms liegen
    int onset = 0;
    while (onset < numWin && rms[(size_t) onset] < peak * 0.1f)
        ++onset;
    if (onset * 0.005 > 0.06)
        return false;

    // Peak kurz nach dem Onset (max. 150 ms)
    if ((peakIdx - onset) * 0.005 > 0.15)
        return false;

    // Ende deutlich leiser als der Peak
    const int tailStart = numWin - juce::jmax (1, numWin / 4);
    float tail = 0.0f;
    for (int w = tailStart; w < numWin; ++w)
        tail += rms[(size_t) w];
    tail /= (float) (numWin - tailStart);

    return tail < peak * 0.3f;
}
