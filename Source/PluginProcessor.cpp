#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    constexpr int   numPeakBins  = 1024;
    constexpr float chokeFadeMs  = 4.0f;
    constexpr float envFloor     = 0.001f;   // -60 dB

    inline float hermite (float xm1, float x0, float x1, float x2, float t)
    {
        const float c0 = x0;
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    inline float decayCoeff (double timeMs, double sampleRate)
    {
        const double n = juce::jmax (1.0, timeMs * 0.001 * sampleRate);
        return (float) std::exp (std::log ((double) envFloor) / n);
    }

    // Soft-Clip mit Knie ab 0.5, naehert sich 1.0 an (wie One Shooter)
    inline float softClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.5f)
            return x;
        const float y = 0.5f + 0.5f * std::tanh ((a - 0.5f) * 2.0f);
        return std::copysign (y, x);
    }

    inline float hardClip (float x) { return juce::jlimit (-1.0f, 1.0f, x); }

    inline float levelToGain (float db) { return db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (db); }
}

QuadShooterProcessor::QuadShooterProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();

    for (int s = 0; s < numSlots; ++s)
    {
        pLevel[s] = apvts.getRawParameterValue (ParamIDs::level (s));
        pTune[s]  = apvts.getRawParameterValue (ParamIDs::tune (s));
        pStart[s] = apvts.getRawParameterValue (ParamIDs::start (s));
        pEnd[s]   = apvts.getRawParameterValue (ParamIDs::end (s));
        pPan[s]   = apvts.getRawParameterValue (ParamIDs::pan (s));
        pMute[s]  = apvts.getRawParameterValue (ParamIDs::mute (s));
        pHold[s]  = apvts.getRawParameterValue (ParamIDs::hold (s));
        pDecay[s] = apvts.getRawParameterValue (ParamIDs::decay (s));
        pTone[s]  = apvts.getRawParameterValue (ParamIDs::tone (s));
        pLowCut[s]  = apvts.getRawParameterValue (ParamIDs::lowCut (s));
        pHighCut[s] = apvts.getRawParameterValue (ParamIDs::highCut (s));
        pVel[s]   = apvts.getRawParameterValue (ParamIDs::vel (s));
        rootOverride[(size_t) s] = rootAuto;
        effectiveRoot[(size_t) s] = noPitch;
        playPos[(size_t) s] = -1.0f;
    }

    auto get = [this] (const char* id) { auto* v = apvts.getRawParameterValue (id); jassert (v != nullptr); return v; };
    pX = get (ParamIDs::xyX);              pY = get (ParamIDs::xyY);
    pKeySync = get (ParamIDs::keySync);    pSyncMode = get (ParamIDs::syncMode);
    pLead = get (ParamIDs::lead);          pChromatic = get (ParamIDs::chromatic);
    pAttack = get (ParamIDs::attack);      pRelease = get (ParamIDs::release);
    pAmpDecay = get (ParamIDs::ampDecay);
    pClipOn = get (ParamIDs::clipOn);      pClipHard = get (ParamIDs::clipHard);
    pClipDrive = get (ParamIDs::clipDrive);
    pGain = get (ParamIDs::gain);
}

juce::AudioProcessorValueTreeState::ParameterLayout QuadShooterProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (v * 100.0f, 1) + " %"; };
    auto ms  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : (v < 10.0f ? String (v, 1) : String (roundToInt (v))) + " ms"; };
    auto db  = [] (float v, int) { return String (v, 1) + " dB"; };

    auto addFloat = [&] (const String& id, const String& name, NormalisableRange<float> r, float def,
                         std::function<String (float, int)> fn)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                        AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fn))));
    };

    for (int s = 0; s < numSlots; ++s)
    {
        const auto L = slotNames[s];
        addFloat (ParamIDs::level (s), L + " Level", { -60.0f, 6.0f, 0.1f, 2.0f }, 0.0f,
                  [] (float v, int) { return v <= -59.9f ? String ("-inf") : String (v, 1) + " dB"; });
        addFloat (ParamIDs::tune (s),  L + " Pitch", { -12.0f, 12.0f, 0.001f }, 0.0f,
                  [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 2) + " st"; });
        addFloat (ParamIDs::start (s), L + " Start", { 0.0f, 1.0f }, 0.0f, pct);
        addFloat (ParamIDs::end (s),   L + " End",   { 0.0f, 1.0f }, 1.0f, pct);
        addFloat (ParamIDs::pan (s),   L + " Pan",   { -1.0f, 1.0f }, 0.0f,
                  [] (float v, int) { return std::abs (v) < 0.005f ? String ("C") : (v < 0 ? "L" : "R") + String (roundToInt (std::abs (v) * 100.0f)); });
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::mute (s), 1 }, L + " Mute", false));
        addFloat (ParamIDs::hold (s),  L + " Hold",  { 0.0f, 2000.0f, 0.0f, 0.35f }, 50.0f, ms);
        addFloat (ParamIDs::decay (s), L + " Decay", { 5.0f, 10000.0f, 0.0f, 0.3f }, 10000.0f,
                  [] (float v, int) { return v >= 9999.0f ? String ("Max") : (v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"); });
        addFloat (ParamIDs::tone (s),  L + " Tone",  { -1.0f, 1.0f }, 0.0f,
                  [] (float v, int) { return std::abs (v) < 0.005f ? String ("0") : (v > 0 ? "+" : "") + String (roundToInt (v * 100.0f)); });
        auto hz = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; };
        addFloat (ParamIDs::lowCut (s),  L + " Low Cut",  { 20.0f, 20000.0f, 0.0f, 0.25f }, 20.0f,
                  [hz] (float v, int i) { return v <= 20.5f ? String ("Off") : hz (v, i); });
        addFloat (ParamIDs::highCut (s), L + " High Cut", { 20.0f, 20000.0f, 0.0f, 0.25f }, 20000.0f,
                  [hz] (float v, int i) { return v >= 19999.0f ? String ("Off") : hz (v, i); });
        addFloat (ParamIDs::vel (s),   L + " Vel Sens", { 0.0f, 1.0f }, 0.5f, pct);
    }

    addFloat (ParamIDs::xyX, "Mix X", { 0.0f, 1.0f }, 0.5f, pct);
    addFloat (ParamIDs::xyY, "Mix Y", { 0.0f, 1.0f }, 0.5f, pct);

    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::keySync, 1 },  "Key Sync", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::syncMode, 1 }, "Sync Mode", syncModes, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::lead, 1 },     "Lead Slot", slotNames, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::chromatic, 1 }, "Chromatic", false));

    addFloat (ParamIDs::attack,  "Attack",  { 0.0f, 200.0f, 0.0f, 0.35f }, 0.0f, ms);
    addFloat (ParamIDs::ampDecay, "Decay",  { 5.0f, 10000.0f, 0.0f, 0.3f }, 10000.0f,
              [] (float v, int) { return v >= 9999.0f ? String ("Max") : (v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"); });
    addFloat (ParamIDs::release, "Release", { 5.0f, releaseOffValue, 0.0f, 0.35f }, releaseOffValue,
              [] (float v, int) { return v >= releaseOffValue - 0.5f ? String ("Off") : (v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"); });

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::clipOn, 1 },   "Clip On",   false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::clipHard, 1 }, "Clip Hard", false));
    addFloat (ParamIDs::clipDrive, "Clip Drive", { 0.0f, 24.0f, 0.1f }, 0.0f,
              [] (float v, int) { return "+" + String (v, 1) + " dB"; });

    addFloat (ParamIDs::gain,     "Gain",     { -36.0f, 12.0f, 0.1f }, -3.0f, db);

    return layout;
}

bool QuadShooterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void QuadShooterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    gainSmoothed.reset (sampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pGain->load()));

    for (int s = 0; s < numSlots; ++s)
    {
        slotGainL[s].reset (sampleRate, 0.015);
        slotGainR[s].reset (sampleRate, 0.015);
        slotGainL[s].setCurrentAndTargetValue (0.0f);
        slotGainR[s].setCurrentAndTargetValue (0.0f);
    }

    // 4x Oversampling fuer den Clipper
    const auto numCh = (size_t) juce::jmax (1, getTotalNumOutputChannels());
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        numCh, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples ((int) std::round (oversampler->getLatencyInSamples()));

    clipDriveSmoothed.reset (sampleRate * 4.0, 0.02);
    clipDriveSmoothed.setCurrentAndTargetValue (1.0f);

    for (auto& v : voices)
        v.active = false;
}

//==============================================================================
bool QuadShooterProcessor::isSupportedFile (const juce::File& file)
{
    return file.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a;caf");
}

bool QuadShooterProcessor::loadSample (int slot, const juce::File& file)
{
    if (slot < 0 || slot >= numSlots)
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    const auto maxLen = (juce::int64) (reader->sampleRate * 30.0);
    const int numSamples = (int) juce::jmin (reader->lengthInSamples, maxLen);
    const int numChannels = (int) juce::jlimit (1u, 2u, reader->numChannels);

    auto data = std::make_unique<SampleData>();
    data->buffer.setSize (numChannels, numSamples);
    reader->read (&data->buffer, 0, numSamples, 0, true, numChannels > 1);
    data->sampleRate = reader->sampleRate;
    data->name = file.getFileNameWithoutExtension();

    WaveformPeaks newPeaks;
    newPeaks.mins.resize (numPeakBins);
    newPeaks.maxs.resize (numPeakBins);
    newPeaks.numSourceSamples = numSamples;
    newPeaks.sampleRate = reader->sampleRate;
    newPeaks.name = data->name;
    for (int b = 0; b < numPeakBins; ++b)
    {
        const int s0 = (int) ((juce::int64) b * numSamples / numPeakBins);
        const int s1 = juce::jmin (numSamples, juce::jmax (s0 + 1, (int) ((juce::int64) (b + 1) * numSamples / numPeakBins)));
        float mn = 0.0f, mx = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto r = data->buffer.findMinMax (ch, s0, s1 - s0);
            mn = juce::jmin (mn, r.getStart());
            mx = juce::jmax (mx, r.getEnd());
        }
        newPeaks.mins[(size_t) b] = mn;
        newPeaks.maxs[(size_t) b] = mx;
    }

    const auto pitch = PitchDetector::detect (data->buffer, data->sampleRate);

    std::unique_ptr<SampleData> old;
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        old = std::move (samples[slot]);
        samples[slot] = std::move (data);
        for (auto& v : voices)
            v.layers[slot].active = false;
    }
    old.reset();

    {
        const juce::ScopedLock sl (infoLock);
        peaks[slot] = std::move (newPeaks);
        files[slot] = file;
        detected[slot] = pitch;
    }
    updateEffectiveRoot (slot);

    sendChangeMessage();
    return true;
}

bool QuadShooterProcessor::previewFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;
    const int numSamples = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 10.0));
    const int numChannels = (int) juce::jlimit (1u, 2u, reader->numChannels);
    auto data = std::make_unique<SampleData>();
    data->buffer.setSize (numChannels, numSamples);
    reader->read (&data->buffer, 0, numSamples, 0, true, numChannels > 1);
    data->sampleRate = reader->sampleRate;

    std::unique_ptr<SampleData> old;
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        old = std::move (previewSample);
        previewSample = std::move (data);
        previewActive = false;
    }
    previewStart = true;
    return true;
}

void QuadShooterProcessor::clearSlot (int slot)
{
    if (slot < 0 || slot >= numSlots)
        return;
    std::unique_ptr<SampleData> old;
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        old = std::move (samples[slot]);
        for (auto& v : voices)
            v.layers[slot].active = false;
    }
    {
        const juce::ScopedLock sl (infoLock);
        peaks[slot] = {};
        files[slot] = juce::File();
        detected[slot] = {};
    }
    updateEffectiveRoot (slot);
    sendChangeMessage();
}

WaveformPeaks QuadShooterProcessor::getPeaks (int slot) const
{
    const juce::ScopedLock sl (infoLock);
    return peaks[slot];
}

juce::File QuadShooterProcessor::getSlotFile (int slot) const
{
    const juce::ScopedLock sl (infoLock);
    return files[slot];
}

PitchResult QuadShooterProcessor::getDetectedPitch (int slot) const
{
    const juce::ScopedLock sl (infoLock);
    return detected[slot];
}

void QuadShooterProcessor::setRootOverride (int slot, int midiOrMode)
{
    rootOverride[(size_t) slot] = midiOrMode;
    updateEffectiveRoot (slot);
    sendChangeMessage();
}

void QuadShooterProcessor::updateEffectiveRoot (int slot)
{
    const int ov = rootOverride[(size_t) slot].load();
    float root = noPitch;
    if (ov >= 0)
        root = (float) ov;
    else if (ov == rootAuto)
    {
        const auto p = getDetectedPitch (slot);
        if (p.tonal && ! getSlotFile (slot).getFullPathName().isEmpty())
            root = p.midi;
    }
    effectiveRoot[(size_t) slot] = root;
}

float QuadShooterProcessor::getSyncOffset (int slot) const
{
    if (pKeySync->load() < 0.5f)
        return 0.0f;
    const int lead = juce::jlimit (0, numSlots - 1, (int) std::round (pLead->load()));
    if (slot == lead)
        return 0.0f;
    const float rLead = effectiveRoot[(size_t) lead].load();
    const float rSlot = effectiveRoot[(size_t) slot].load();
    if (rLead <= noPitch + 1.0f || rSlot <= noPitch + 1.0f)
        return 0.0f;

    float diff = rLead - rSlot;
    if ((int) std::round (pSyncMode->load()) == 0)   // Nearest: auf naechste Oktave falten
        diff -= 12.0f * std::round (diff / 12.0f);
    return diff;
}

//==============================================================================
void QuadShooterProcessor::startVoice (int note, int midiVelocity, int soloSlot)
{
    const float chokeStep = 1.0f / (float) juce::jmax (1.0, currentSampleRate * chokeFadeMs / 1000.0);
    const bool chromatic = pChromatic->load() > 0.5f;

    // Ohne Chromatic: monofon wie One Shooter (neuer Hit choked den alten).
    // Mit Chromatic: polyfon, nur dieselbe Taste wird gechoked.
    Voice* target = nullptr;
    for (auto& v : voices)
    {
        if (v.active && (! chromatic || v.note == note || soloSlot >= 0 || v.soloSlot >= 0))
            v.fadeStep = juce::jmax (v.fadeStep, chokeStep);
        else if (! v.active && target == nullptr)
            target = &v;
    }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.age < target->age)
                target = &v;
    }

    const float vel = (float) midiVelocity / 127.0f;

    auto& v = *target;
    v = Voice();
    v.active = true;
    v.note = note;
    v.soloSlot = soloSlot;
    v.age = ++voiceCounter;
    v.noteOffset = (chromatic && note >= 0) ? (float) (note - rootNote) : 0.0f;

    bool any = false;
    for (int s = 0; s < numSlots; ++s)
    {
        const auto* d = renderSamples[s];
        if (d == nullptr || (soloSlot >= 0 && s != soloSlot))
            continue;
        const int n = d->buffer.getNumSamples();
        const double st = (double) pStart[s]->load() * (n - 1);
        const double en = juce::jmax (st + 16.0, (double) pEnd[s]->load() * (n - 1));
        auto& L = v.layers[s];
        L = Layer();
        L.active = true;
        L.position = st;
        L.endPosition = juce::jmin ((double) n - 1.0, en);
        L.baseIncrement = d->sampleRate / currentSampleRate;
        const float sens = pVel[s]->load();
        L.velGain = (1.0f - sens) + sens * vel * vel;
        any = true;
    }
    if (! any)
        v.active = false;
}

void QuadShooterProcessor::releaseVoices (int note)
{
    const float rel = pRelease->load();
    if (rel >= releaseOffValue - 0.5f)
        return;   // One-Shot

    const float coeff = decayCoeff (rel, currentSampleRate);
    for (auto& v : voices)
        if (v.active && v.note == note && ! v.releasing)
        {
            v.releasing = true;
            v.releaseCoeff = coeff;
        }
}

QuadShooterProcessor::Svf QuadShooterProcessor::makeSvf (float fc, double sr)
{
    Svf f;
    f.on = true;
    const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (10.0f, (float) (sr * 0.45), fc) / (float) sr);
    f.k = 1.41421f;   // Q 0.707
    f.a1 = 1.0f / (1.0f + g * (g + f.k));
    f.a2 = g * f.a1;
    f.a3 = g * f.a2;
    return f;
}

void QuadShooterProcessor::renderVoices (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    const int outChannels = out.getNumChannels();
    const double sr = currentSampleRate;
    const double attackSamples = pAttack->load() * 0.001 * sr;
    const float  ampDecayCoeff = pAmpDecay->load() >= 9999.0f ? 1.0f : decayCoeff (pAmpDecay->load(), sr);
    const double endFade = sr * 0.002;

    // Pro Slot einmal pro Block
    SlotDsp dsp[numSlots];
    for (int s = 0; s < numSlots; ++s)
    {
        auto& D = dsp[s];
        D.pitchRatio = std::exp2 ((pTune[s]->load() + getSyncOffset (s)) / 12.0);
        D.holdSamples = pHold[s]->load() * 0.001 * sr;
        D.decayCoeff = pDecay[s]->load() >= 9999.0f ? 1.0f : decayCoeff (pDecay[s]->load(), sr);
        const float tone = pTone[s]->load();
        D.toneOn = std::abs (tone) > 0.005f;
        D.toneCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 900.0f / (float) sr);
        D.toneLow  = 1.0f - 0.8f * tone;
        D.toneHigh = 1.0f + 0.8f * tone;
        if (pLowCut[s]->load() > 20.5f)     D.hp = makeSvf (pLowCut[s]->load(), sr);
        if (pHighCut[s]->load() < 19999.0f) D.lp = makeSvf (pHighCut[s]->load(), sr);
    }

    auto svf = [] (const Svf& f, float x, float& ic1, float& ic2, bool high)
    {
        const float v3 = x - ic2;
        const float v1 = f.a1 * ic1 + f.a2 * v3;
        const float v2 = ic2 + f.a2 * ic1 + f.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return high ? x - f.k * v1 - v2 : v2;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        float gl[numSlots], gr[numSlots];
        for (int s = 0; s < numSlots; ++s)
        {
            gl[s] = slotGainL[s].getNextValue();
            gr[s] = slotGainR[s].getNextValue();
        }

        float outL = 0.0f, outR = 0.0f;

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            const bool inAttack = attackSamples >= 1.0 && v.attackSamples < attackSamples;
            const float att = inAttack ? (float) (v.attackSamples / attackSamples) : 1.0f;
            v.attackSamples += 1.0;
            if (! inAttack)
                v.ampDecayLevel *= ampDecayCoeff;

            float vl = 0.0f, vr = 0.0f;
            bool anyLayer = false;
            const double noteRatio = std::abs (v.noteOffset) > 0.0001f ? std::exp2 ((double) v.noteOffset / 12.0) : 1.0;

            for (int s = 0; s < numSlots; ++s)
            {
                auto& L = v.layers[s];
                if (! L.active)
                    continue;
                const auto* d = renderSamples[s];
                if (d == nullptr) { L.active = false; continue; }
                const auto& D = dsp[s];

                // Hold / Decay pro Slot
                if (L.envStage == 0)
                {
                    if (L.envSamples >= D.holdSamples) L.envStage = 1;
                }
                else
                    L.envLevel *= D.decayCoeff;
                L.envSamples += 1.0;

                const int n = d->buffer.getNumSamples();
                const int idx = (int) L.position;
                const float frac = (float) (L.position - idx);
                const float* c0 = d->buffer.getReadPointer (0);
                const float* c1 = d->buffer.getReadPointer (d->buffer.getNumChannels() > 1 ? 1 : 0);
                auto at = [n] (const float* c, int k) { return c[juce::jlimit (0, n - 1, k)]; };

                const double inc = L.baseIncrement * D.pitchRatio * noteRatio;
                const float endGain = (float) juce::jlimit (0.0, 1.0, ((L.endPosition - L.position) / inc) / endFade);
                const float g = endGain * L.envLevel * L.velGain;

                float x[2];
                x[0] = hermite (at (c0, idx - 1), at (c0, idx), at (c0, idx + 1), at (c0, idx + 2), frac) * g;
                x[1] = c1 == c0 ? x[0]
                     : hermite (at (c1, idx - 1), at (c1, idx), at (c1, idx + 1), at (c1, idx + 2), frac) * g;

                for (int ch = 0; ch < 2; ++ch)
                {
                    if (D.toneOn)
                    {
                        L.toneLp[ch] += D.toneCoeff * (x[ch] - L.toneLp[ch]);
                        x[ch] = L.toneLp[ch] * D.toneLow + (x[ch] - L.toneLp[ch]) * D.toneHigh;
                    }
                    if (D.hp.on) x[ch] = svf (D.hp, x[ch], L.hp1[ch], L.hp2[ch], true);
                    if (D.lp.on) x[ch] = svf (D.lp, x[ch], L.lp1[ch], L.lp2[ch], false);
                }

                if (v.soloSlot >= 0) { vl += x[0]; vr += x[1]; }
                else                 { vl += x[0] * gl[s]; vr += x[1] * gr[s]; }

                L.position += inc;
                if (L.position >= L.endPosition || (L.envStage == 1 && L.envLevel <= envFloor))
                    L.active = false;
                else
                    anyLayer = true;
            }

            const float amp = att * v.ampDecayLevel * v.fadeGain * v.releaseGain;
            outL += vl * amp;
            outR += vr * amp;

            if (v.releasing)       v.releaseGain *= v.releaseCoeff;
            if (v.fadeStep > 0.0f) v.fadeGain -= v.fadeStep;

            if (! anyLayer || v.fadeGain <= 0.0f || v.releaseGain <= envFloor || v.ampDecayLevel <= envFloor)
                v.active = false;
        }

        if (outChannels == 1)
            out.addSample (0, startSample + i, 0.5f * (outL + outR));
        else
        {
            out.addSample (0, startSample + i, outL);
            out.addSample (1, startSample + i, outR);
        }
    }
}

void QuadShooterProcessor::processClipper (juce::AudioBuffer<float>& buffer)
{
    const bool on = pClipOn->load() > 0.5f;
    const bool hard = pClipHard->load() > 0.5f;
    clipDriveSmoothed.setTargetValue (on ? juce::Decibels::decibelsToGain (pClipDrive->load()) : 1.0f);

    juce::dsp::AudioBlock<float> block (buffer);
    auto up = oversampler->processSamplesUp (block);

    float maxReduction = 0.0f;
    const auto numCh = up.getNumChannels();
    const auto numS  = up.getNumSamples();

    for (size_t i = 0; i < numS; ++i)
    {
        const float drive = clipDriveSmoothed.getNextValue();
        for (size_t ch = 0; ch < numCh; ++ch)
        {
            const float x = up.getSample ((int) ch, (int) i) * drive;
            float y = x;
            if (on)
            {
                y = hard ? hardClip (x) : softClip (x);
                const float ax = std::abs (x), ay = std::abs (y);
                if (ax > 1e-6f && ay < ax)
                    maxReduction = juce::jmax (maxReduction, juce::Decibels::gainToDecibels (ax / juce::jmax (1e-6f, ay)));
            }
            up.setSample ((int) ch, (int) i, y);
        }
    }

    oversampler->processSamplesDown (block);

    const float prev = clipReductionDb.load();
    clipReductionDb = maxReduction > prev ? maxReduction : prev * 0.85f;
}

void QuadShooterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    // Ziel-Gains pro Slot: XY-Gewicht * Level * Mute * Pan
    const auto w = xyWeights (pX->load(), pY->load());
    for (int s = 0; s < numSlots; ++s)
    {
        const float g = w[(size_t) s] * levelToGain (pLevel[s]->load()) * (pMute[s]->load() > 0.5f ? 0.0f : 1.0f);
        const float angle = (pPan[s]->load() + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        slotGainL[s].setTargetValue (g * std::cos (angle) * juce::MathConstants<float>::sqrt2);
        slotGainR[s].setTargetValue (g * std::sin (angle) * juce::MathConstants<float>::sqrt2);
    }

    {
        const juce::SpinLock::ScopedTryLockType tl (sampleLock);
        if (tl.isLocked())
        {
            for (int s = 0; s < numSlots; ++s)
                renderSamples[s] = samples[s].get();

            const int preview = previewRequest.exchange (-2);
            if (preview >= -1)
                startVoice (-1, 100, preview);

            int pos = 0;
            for (const auto meta : midi)
            {
                const auto msg = meta.getMessage();
                const int when = juce::jlimit (0, numSamples, meta.samplePosition);
                if (when > pos)
                {
                    renderVoices (buffer, pos, when - pos);
                    pos = when;
                }
                if (msg.isNoteOn())
                    startVoice (msg.getNoteNumber(), msg.getVelocity(), -1);
                else if (msg.isNoteOff())
                    releaseVoices (msg.getNoteNumber());
                else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                    for (auto& v : voices)
                        if (v.active)
                            v.fadeStep = 1.0f / (float) (currentSampleRate * chokeFadeMs / 1000.0);
            }
            if (pos < numSamples)
                renderVoices (buffer, pos, numSamples - pos);

            // Library-Vorhoeren
            if (previewStart.exchange (false))
            {
                previewPos = 0.0;
                previewActive = previewSample != nullptr;
            }
            if (previewActive && previewSample != nullptr)
            {
                const auto& pb = previewSample->buffer;
                const int n = pb.getNumSamples();
                const double inc = previewSample->sampleRate / currentSampleRate;
                const int chs = pb.getNumChannels();
                for (int i = 0; i < numSamples && previewActive; ++i)
                {
                    const int k = (int) previewPos;
                    if (k + 1 >= n) { previewActive = false; break; }
                    const float t = (float) (previewPos - k);
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    {
                        const int sc = juce::jmin (ch, chs - 1);
                        const float a = pb.getSample (sc, k), b2 = pb.getSample (sc, k + 1);
                        buffer.addSample (ch, i, 0.8f * (a + t * (b2 - a)));
                    }
                    previewPos += inc;
                }
            }

            // Playhead pro Slot (juengste Stimme)
            for (int s = 0; s < numSlots; ++s)
            {
                float newest = -1.0f;
                juce::uint32 newestAge = 0;
                if (renderSamples[s] != nullptr)
                    for (auto& v : voices)
                        if (v.active && v.layers[s].active && v.fadeStep <= 0.0f && v.age > newestAge)
                        {
                            newestAge = v.age;
                            newest = (float) (v.layers[s].position / renderSamples[s]->buffer.getNumSamples());
                        }
                playPos[(size_t) s] = newest;
            }

            for (auto& r : renderSamples)
                r = nullptr;
        }
    }

    if (oversampler != nullptr && buffer.getNumChannels() <= (int) oversampler->numChannels)
        processClipper (buffer);

    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (pGain->load()));
    if (gainSmoothed.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = gainSmoothed.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * g);
        }
    }
    else
        buffer.applyGain (gainSmoothed.getTargetValue());

    const float peak = buffer.getMagnitude (0, numSamples);
    const float prevPeak = outputPeak.load();
    outputPeak = peak > prevPeak ? peak : prevPeak * 0.9f;
}

//==============================================================================
void QuadShooterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int s = 0; s < numSlots; ++s)
    {
        state.setProperty ("path" + slotNames[s], getSlotFile (s).getFullPathName(), nullptr);
        state.setProperty ("root" + slotNames[s], rootOverride[(size_t) s].load(), nullptr);
    }
    state.setProperty ("activeSlot", activeSlot.load(), nullptr);
    state.setProperty ("showLibrary", showLibrary, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void QuadShooterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid())
        return;

    showLibrary = (bool) state.getProperty ("showLibrary", true);
    activeSlot = juce::jlimit (0, numSlots - 1, (int) state.getProperty ("activeSlot", 0));
    apvts.replaceState (state);

    for (int s = 0; s < numSlots; ++s)
    {
        rootOverride[(size_t) s] = (int) state.getProperty ("root" + slotNames[s], rootAuto);
        const juce::File f (state.getProperty ("path" + slotNames[s]).toString());
        if (f.getFullPathName().isNotEmpty() && f.existsAsFile())
            loadSample (s, f);
        else
            clearSlot (s);
    }
}

juce::AudioProcessorEditor* QuadShooterProcessor::createEditor()
{
    return new QuadShooterEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuadShooterProcessor();
}
