#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "SampleLibrary.h"
#include "PitchDetector.h"

inline constexpr int numSlots = 4;

namespace ParamIDs
{
    // Pro Slot (Suffix A..D)
    inline juce::String slotID (const char* base, int s) { return juce::String (base) + juce::String::charToString ((juce::juce_wchar) ('A' + s)); }
    inline juce::String level   (int s) { return slotID ("level", s); }
    inline juce::String tune    (int s) { return slotID ("tune", s); }
    inline juce::String start   (int s) { return slotID ("start", s); }
    inline juce::String end     (int s) { return slotID ("end", s); }
    inline juce::String pan     (int s) { return slotID ("pan", s); }
    inline juce::String mute    (int s) { return slotID ("mute", s); }
    inline juce::String hold    (int s) { return slotID ("hold", s); }
    inline juce::String decay   (int s) { return slotID ("decay", s); }
    inline juce::String tone    (int s) { return slotID ("tone", s); }
    inline juce::String lowCut  (int s) { return slotID ("lowCut", s); }
    inline juce::String highCut (int s) { return slotID ("highCut", s); }
    inline juce::String vel     (int s) { return slotID ("vel", s); }

    // XY-Mixer
    inline constexpr const char* xyX        = "xyX";
    inline constexpr const char* xyY        = "xyY";
    // Key Sync
    inline constexpr const char* keySync    = "keySync";
    inline constexpr const char* syncMode   = "syncMode";
    inline constexpr const char* lead       = "lead";
    inline constexpr const char* chromatic  = "chromatic";
    // Amp (global; Hold/Decay sind pro Slot)
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* ampDecay   = "ampDecay";
    inline constexpr const char* release    = "release";
    // Clipper
    inline constexpr const char* clipOn     = "clipOn";
    inline constexpr const char* clipHard   = "clipHard";
    inline constexpr const char* clipDrive  = "clipDrive";
    // Output
    inline constexpr const char* gain       = "gain";
}

inline constexpr float releaseOffValue = 5000.0f;
inline constexpr int   rootNote = 60;            // C3: Originalhoehe im Chromatic-Modus
inline constexpr float noPitch = -1000.0f;       // Slot ohne erkannten Ton
inline constexpr int   rootAuto = -1;            // Root-Override: automatisch erkennen
inline constexpr int   rootNone = -2;            // Root-Override: als nicht tonal behandeln

inline const juce::StringArray syncModes   { "Nearest", "Exact" };
inline const juce::StringArray slotNames   { "A", "B", "C", "D" };

struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    juce::String name;
};

struct WaveformPeaks
{
    std::vector<float> mins, maxs;
    int numSourceSamples = 0;
    double sampleRate = 44100.0;
    juce::String name;
    bool isEmpty() const { return mins.empty(); }
};

// Gewichte der vier Ecken des XY-Pads (A oben links, B oben rechts, C unten links, D unten rechts).
// y = 1 ist oben. Summe der Leistungen = 1 (Equal Power).
inline std::array<float, numSlots> xyWeights (float x, float y)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    y = juce::jlimit (0.0f, 1.0f, y);
    return { std::sqrt ((1.0f - x) * y), std::sqrt (x * y),
             std::sqrt ((1.0f - x) * (1.0f - y)), std::sqrt (x * (1.0f - y)) };
}

class QuadShooterProcessor : public juce::AudioProcessor,
                             public juce::ChangeBroadcaster
{
public:
    QuadShooterProcessor();
    ~QuadShooterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Samples
    bool loadSample (int slot, const juce::File& file);
    void clearSlot (int slot);
    static bool isSupportedFile (const juce::File& file);

    WaveformPeaks getPeaks (int slot) const;
    juce::File    getSlotFile (int slot) const;
    PitchResult   getDetectedPitch (int slot) const;

    // Grundton: automatisch erkannt oder manuell gesetzt
    void  setRootOverride (int slot, int midiOrMode);
    int   getRootOverride (int slot) const    { return rootOverride[(size_t) slot].load(); }
    float getEffectiveRoot (int slot) const   { return effectiveRoot[(size_t) slot].load(); }
    // Halbtoene, um die Key Sync den Slot verschiebt (0 wenn aus / nicht tonal / Lead)
    float getSyncOffset (int slot) const;

    // Fuer den Editor
    float getPlayPosition (int slot) const    { return playPos[(size_t) slot].load(); }
    float getClipReductionDb() const          { return clipReductionDb.load(); }
    float getOutputPeak() const               { return outputPeak.load(); }
    void  triggerPreview (int slot)           { previewRequest = slot; }   // -1 = alle Slots (XY-Mix)
    bool  previewFile (const juce::File& file);                           // Library: nur vorhoeren, nicht laden
    SampleLibrary& getLibrary()               { return *library; }

    std::atomic<int> activeSlot { 0 };   // Ziel fuer Library und Datei-Drops
    bool showLibrary = true;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateEffectiveRoot (int slot);

    struct Layer
    {
        bool   active = false;
        double position = 0.0, endPosition = 0.0, baseIncrement = 1.0;
        int    envStage = 0;          // 0 = Hold, 1 = Decay
        double envSamples = 0.0;
        float  envLevel = 1.0f;
        float  velGain = 1.0f;
        float  toneLp[2] {};          // Tilt
        float  hp1[2] {}, hp2[2] {};  // Low Cut (SVF)
        float  lp1[2] {}, lp2[2] {};  // High Cut (SVF)
    };

    struct Svf { bool on = false; float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 1.41421f; };
    struct SlotDsp
    {
        double pitchRatio = 1.0, holdSamples = 0.0;
        float  decayCoeff = 1.0f;
        float  toneCoeff = 0.0f, toneLow = 1.0f, toneHigh = 1.0f;
        bool   toneOn = false;
        Svf    hp, lp;
    };
    static Svf makeSvf (float fc, double sr);

    struct Voice
    {
        bool   active = false;
        int    note = -1;
        int    soloSlot = -1;          // Vorhoeren: nur dieser Slot, ohne XY-Mix
        float  noteOffset = 0.0f;
        Layer  layers[numSlots];
        // Attack / Decay (global)
        double attackSamples = 0.0;
        float  ampDecayLevel = 1.0f;
        // Gemeinsam
        bool   releasing = false;
        float  releaseGain = 1.0f, releaseCoeff = 1.0f;
        float  fadeGain = 1.0f, fadeStep = 0.0f;
        juce::uint32 age = 0;
    };

    void startVoice (int note, int midiVelocity, int soloSlot);
    void releaseVoices (int note);
    void renderVoices (juce::AudioBuffer<float>& out, int startSample, int numSamples);
    void processClipper (juce::AudioBuffer<float>& buffer);

    juce::AudioFormatManager formatManager;

    juce::SpinLock sampleLock;
    std::unique_ptr<SampleData> samples[numSlots];
    const SampleData* renderSamples[numSlots] {};   // nur im Audio-Thread gueltig

    mutable juce::CriticalSection infoLock;
    WaveformPeaks peaks[numSlots];
    juce::File files[numSlots];
    PitchResult detected[numSlots];

    std::array<std::atomic<int>, numSlots>   rootOverride;
    std::array<std::atomic<float>, numSlots> effectiveRoot;
    std::array<std::atomic<float>, numSlots> playPos;

    static constexpr int maxVoices = 8;
    Voice voices[maxVoices];
    juce::uint32 voiceCounter = 0;
    double currentSampleRate = 44100.0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::SmoothedValue<float> clipDriveSmoothed, gainSmoothed;
    juce::SmoothedValue<float> slotGainL[numSlots], slotGainR[numSlots];

    std::atomic<float> clipReductionDb { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<int>   previewRequest { -2 };   // -2 = nichts
    // Library-Vorhoeren (eigener Player, unabhaengig von den Slots)
    std::unique_ptr<SampleData> previewSample;
    std::atomic<bool> previewStart { false };
    double previewPos = 0.0;
    bool   previewActive = false;
    juce::SharedResourcePointer<SampleLibrary> library;

    // Parameter-Zeiger
    std::atomic<float>* pLevel[numSlots] {};
    std::atomic<float>* pTune[numSlots] {};
    std::atomic<float>* pStart[numSlots] {};
    std::atomic<float>* pEnd[numSlots] {};
    std::atomic<float>* pPan[numSlots] {};
    std::atomic<float>* pMute[numSlots] {};
    std::atomic<float>* pHold[numSlots] {};
    std::atomic<float>* pDecay[numSlots] {};
    std::atomic<float>* pTone[numSlots] {};
    std::atomic<float>* pLowCut[numSlots] {};
    std::atomic<float>* pHighCut[numSlots] {};
    std::atomic<float>* pVel[numSlots] {};
    std::atomic<float> *pX = nullptr, *pY = nullptr, *pKeySync = nullptr, *pSyncMode = nullptr, *pLead = nullptr,
                       *pChromatic = nullptr, *pAttack = nullptr, *pAmpDecay = nullptr,
                       *pRelease = nullptr, *pClipOn = nullptr,
                       *pClipHard = nullptr, *pClipDrive = nullptr, *pGain = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuadShooterProcessor)
};
