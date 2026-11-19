// Lokaler Test: Tonerkennung, Key Sync, XY-Mix, Chromatic.
// Build: cmake -DSHOOTER_TEST_SRC=tests/EngineTest.cpp ... && cmake --build build --target ShooterTest
#include "../Source/PluginProcessor.h"
#include <juce_events/juce_events.h>
#include <cstdio>
#include <random>

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf ("  ok    %s\n", msg); else { std::printf ("  FAIL  %s\n", msg); ++failures; } } while (0)

static float mtof (float m) { return 440.0f * std::exp2 ((m - 69.0f) / 12.0f); }

static juce::File writeWav (const juce::String& name, const juce::AudioBuffer<float>& b, double sr)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("qs_" + name + ".wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 1, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    return f;
}

// Plucky Ton mit Obertoenen + Noise-Transient
static juce::AudioBuffer<float> tone (float midi, double sr, double secs, float glideSemis = 0.0f)
{
    juce::AudioBuffer<float> b (1, (int) (sr * secs));
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> nd (-1.0f, 1.0f);
    double ph = 0.0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        const float f = mtof (midi + glideSemis * (float) std::exp (-t / 0.012));
        ph += f / sr;
        const float env = (float) std::exp (-t / 0.35);
        float s = 0.0f;
        for (int h = 1; h <= 5; ++h)
            s += std::sin ((float) (juce::MathConstants<double>::twoPi * ph * h)) / (float) (h * h);
        const float click = t < 0.006 ? nd (rng) * (float) (1.0 - t / 0.006) : 0.0f;
        b.setSample (0, i, 0.6f * env * s + 0.5f * click);
    }
    return b;
}

static juce::AudioBuffer<float> clap (double sr)
{
    juce::AudioBuffer<float> b (1, (int) (sr * 0.4));
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> nd (-1.0f, 1.0f);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        float env = (float) std::exp (-t / 0.06);
        for (double burst : { 0.0, 0.011, 0.022 })
            if (t >= burst && t < burst + 0.01) env = juce::jmax (env, (float) std::exp (-(t - burst) / 0.003));
        b.setSample (0, i, 0.7f * env * nd (rng));
    }
    return b;
}

static juce::AudioBuffer<float> render (QuadShooterProcessor& p, int note, double secs, double sr)
{
    const int block = 512;
    const int total = (int) (secs * sr);
    juce::AudioBuffer<float> out (2, total), buf (2, block);
    for (int pos = 0; pos < total; pos += block)
    {
        juce::MidiBuffer midi;
        if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    return out;
}

static void setParam (QuadShooterProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static float renderedPitch (QuadShooterProcessor& p, int note, double sr)
{
    auto out = render (p, note, 1.0, sr);
    juce::AudioBuffer<float> mono (1, out.getNumSamples());
    mono.copyFrom (0, 0, out, 0, 0, out.getNumSamples());
    auto r = PitchDetector::detect (mono, sr);
    return r.tonal ? r.midi : -1.0f;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;

    std::printf ("Tonerkennung\n");
    struct Case { const char* name; float midi; float glide; };
    for (auto c : { Case { "C3", 60.0f, 0.0f }, Case { "B1", 47.0f, 0.0f }, Case { "A1_808", 45.0f, 24.0f },
                    Case { "F#4", 78.0f, 0.0f }, Case { "E0_sub", 28.0f, 0.0f } })
    {
        auto b = tone (c.midi, sr, 1.2, c.glide);
        auto r = PitchDetector::detect (b, sr);
        char msg[128];
        std::snprintf (msg, sizeof msg, "%-7s erwartet %.0f, erkannt %s %.2f (conf %.2f)", c.name, c.midi,
                       r.tonal ? "tonal" : "NICHT tonal", r.midi, r.confidence);
        CHECK (r.tonal && std::abs (r.midi - c.midi) < 0.3f, msg);
    }
    {
        auto r = PitchDetector::detect (clap (sr), sr);
        CHECK (! r.tonal, "Clap gilt als nicht tonal");
    }

    std::printf ("Engine\n");
    QuadShooterProcessor p;
    p.setPlayConfigDetails (0, 2, sr, 512);
    p.prepareToPlay (sr, 8192);

    CHECK (p.loadSample (0, writeWav ("C3", tone (60.0f, sr, 1.2), sr)), "Slot A laedt C3");
    CHECK (p.loadSample (1, writeWav ("B1", tone (47.0f, sr, 1.2), sr)), "Slot B laedt B1");
    CHECK (p.loadSample (2, writeWav ("clap", clap (sr), sr)), "Slot C laedt Clap");
    CHECK (p.loadSample (3, writeWav ("A1", tone (45.0f, sr, 1.2, 24.0f), sr)), "Slot D laedt 808 A1");

    setParam (p, ParamIDs::keySync, 1.0f);
    setParam (p, ParamIDs::lead, 0.0f);
    {
        char msg[128];
        std::snprintf (msg, sizeof msg, "Nearest: B %+.2f st (erw. +1), C %+.2f (erw. 0), D %+.2f (erw. +3)",
                       p.getSyncOffset (1), p.getSyncOffset (2), p.getSyncOffset (3));
        CHECK (std::abs (p.getSyncOffset (1) - 1.0f) < 0.3f && std::abs (p.getSyncOffset (2)) < 0.001f
               && std::abs (p.getSyncOffset (3) - 3.0f) < 0.3f, msg);
    }
    setParam (p, ParamIDs::syncMode, 1.0f);
    {
        char msg[96];
        std::snprintf (msg, sizeof msg, "Exact: B %+.2f st (erw. +13)", p.getSyncOffset (1));
        CHECK (std::abs (p.getSyncOffset (1) - 13.0f) < 0.3f, msg);
    }
    setParam (p, ParamIDs::syncMode, 0.0f);

    // Nur B hoerbar (Ecke oben rechts), Key Sync an -> B klingt als C2 (48)
    setParam (p, ParamIDs::xyX, 1.0f);
    setParam (p, ParamIDs::xyY, 1.0f);
    for (int i = 0; i < 3; ++i) render (p, 60, 0.05, sr);   // Gains einschwingen lassen
    {
        const float m = renderedPitch (p, 60, sr);
        char msg[96];
        std::snprintf (msg, sizeof msg, "Slot B gesynct klingt als %.2f (erw. 48 = C2)", m);
        CHECK (std::abs (m - 48.0f) < 0.3f, msg);
    }

    // Chromatic: G3 (67) -> B klingt 7 st hoeher (55)
    setParam (p, ParamIDs::chromatic, 1.0f);
    {
        const float m = renderedPitch (p, 67, sr);
        char msg[96];
        std::snprintf (msg, sizeof msg, "Chromatic G3: Slot B klingt als %.2f (erw. 55)", m);
        CHECK (std::abs (m - 55.0f) < 0.3f, msg);
    }
    setParam (p, ParamIDs::chromatic, 0.0f);

    // XY-Ecken: Energie pro Slot pruefen (je Ecke nur ein Slot)
    {
        auto energy = [&] (float x, float y, int muteMask)
        {
            for (int s = 0; s < numSlots; ++s) setParam (p, ParamIDs::mute (s), (muteMask >> s) & 1 ? 1.0f : 0.0f);
            setParam (p, ParamIDs::xyX, x);
            setParam (p, ParamIDs::xyY, y);
            for (int i = 0; i < 3; ++i) render (p, 60, 0.05, sr);
            auto out = render (p, 60, 0.3, sr);
            return out.getRMSLevel (0, 0, out.getNumSamples());
        };
        const float aOnly = energy (0.0f, 1.0f, 0);
        const float aMutedAtA = energy (0.0f, 1.0f, 1);
        const float centre = energy (0.5f, 0.5f, 0);
        char msg[128];
        std::snprintf (msg, sizeof msg, "Ecke A: RMS %.3f, A gemutet: %.5f, Mitte: %.3f", aOnly, aMutedAtA, centre);
        CHECK (aOnly > 0.01f && aMutedAtA < 1e-4f && centre > 0.01f, msg);
        energy (0.5f, 0.5f, 0);
    }

    // Edit-Zeile pro Slot: Decay, End, Low Cut, VelSens (nur Slot A hoerbar)
    {
        for (int s = 0; s < numSlots; ++s) setParam (p, ParamIDs::mute (s), s == 0 ? 0.0f : 1.0f);
        setParam (p, ParamIDs::xyX, 0.0f); setParam (p, ParamIDs::xyY, 1.0f);
        for (int i = 0; i < 3; ++i) render (p, 60, 0.05, sr);
        auto tailRms = [&] { auto o = render (p, 60, 0.8, sr); return o.getRMSLevel (0, (int) (0.4 * sr), (int) (0.4 * sr)); };
        auto fullRms = [&] { auto o = render (p, 60, 0.5, sr); return o.getRMSLevel (0, 0, o.getNumSamples()); };

        const float tail0 = tailRms();
        setParam (p, ParamIDs::decay (0), 80.0f);
        const float tailShort = tailRms();
        setParam (p, ParamIDs::decay (0), 10000.0f);
        setParam (p, ParamIDs::end (0), 0.2f);
        const float tailEnd = tailRms();
        setParam (p, ParamIDs::end (0), 1.0f);
        char msg[128];
        std::snprintf (msg, sizeof msg, "Decay 80 ms / End 20 %%: Ausklang %.4f -> %.5f / %.5f", tail0, tailShort, tailEnd);
        CHECK (tailShort < tail0 * 0.05f && tailEnd < tail0 * 0.05f, msg);

        const float open = fullRms();
        setParam (p, ParamIDs::lowCut (0), 2000.0f);
        const float cut = fullRms();
        setParam (p, ParamIDs::lowCut (0), 20.0f);
        std::snprintf (msg, sizeof msg, "Low Cut 2 kHz: RMS %.3f -> %.3f", open, cut);
        CHECK (cut < open * 0.5f, msg);

        setParam (p, ParamIDs::tone (0), -1.0f);
        const float dark = fullRms();
        setParam (p, ParamIDs::tone (0), 0.0f);
        CHECK (std::isfinite (dark) && dark > 0.0f, "Tone stabil");

        // VelSens: leise Note bei 100 % deutlich leiser als bei 0 %
        auto velRms = [&] (float sens)
        {
            setParam (p, ParamIDs::vel (0), sens);
            juce::AudioBuffer<float> buf (2, 4800);
            juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 30), 0);
            p.processBlock (buf, m);
            return buf.getRMSLevel (0, 0, buf.getNumSamples());
        };
        const float v0 = velRms (0.0f), v1 = velRms (1.0f);
        setParam (p, ParamIDs::vel (0), 0.5f);
        std::snprintf (msg, sizeof msg, "VelSens 0 %% vs 100 %% bei Velocity 30: %.3f vs %.3f", v0, v1);
        CHECK (v1 < v0 * 0.3f, msg);

        for (int s = 0; s < numSlots; ++s) setParam (p, ParamIDs::mute (s), 0.0f);
        setParam (p, ParamIDs::xyX, 0.5f); setParam (p, ParamIDs::xyY, 0.5f);
    }

    // Library-Vorhoeren laedt nichts in die Slots
    {
        const auto before = p.getSlotFile (0);
        CHECK (p.previewFile (writeWav ("prev", clap (sr), sr)), "Vorhoeren: Datei geladen");
        juce::AudioBuffer<float> buf (2, 4800);
        juce::MidiBuffer m;
        p.processBlock (buf, m);
        CHECK (buf.getMagnitude (0, 0, buf.getNumSamples()) > 0.01f && p.getSlotFile (0) == before,
               "Vorhoeren klingt und laesst Slot A unveraendert");
    }

    // State speichern / laden
    {
        p.setRootOverride (2, 50);
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        QuadShooterProcessor q;
        q.setPlayConfigDetails (0, 2, sr, 512);
        q.prepareToPlay (sr, 8192);
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (q.getSlotFile (3) == p.getSlotFile (3) && q.getRootOverride (2) == 50
               && std::abs (q.getSyncOffset (1) - 1.0f) < 0.3f, "State: Slots, Root-Override und Key Sync wiederhergestellt");
    }

    // Screenshot der Oberflaeche (nur wenn QS_SNAPSHOT gesetzt ist)
    if (auto path = juce::SystemStats::getEnvironmentVariable ("QS_SNAPSHOT", {}); path.isNotEmpty())
    {
        p.setRootOverride (2, rootAuto);
        setParam (p, ParamIDs::xyX, 0.32f);
        setParam (p, ParamIDs::xyY, 0.7f);
        setParam (p, ParamIDs::clipOn, 1.0f);
        setParam (p, ParamIDs::clipDrive, 6.0f);
        p.activeSlot = 1;
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (ed->getWidth(), ed->getHeight());
        ed->resized();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out (path);
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("Snapshot: %s\n", path.toRawUTF8());
    }

    std::printf (failures == 0 ? "\nAlle Tests bestanden\n" : "\n%d Tests fehlgeschlagen\n", failures);
    return failures == 0 ? 0 : 1;
}
