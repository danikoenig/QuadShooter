// Reel-Renderer: spielt einen festen Ablauf im Plugin ab, rendert Audio (WAV) und GUI-Frames (JPG, 30 fps)
// und schreibt Cursor/Layout-Daten (events.json) fuer die Komposition.
// Build: cmake -DSHOOTER_TEST_SRC=tests/ReelRender.cpp ...  Aufruf: ShooterTest <out-dir> <sample-dir>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_events/juce_events.h>
#include <cstdio>
#include <functional>

static constexpr double FPS = 30.0, SR = 48000.0, BPM = 128.0;
static constexpr double BEAT = 60.0 / BPM, BAR = 4.0 * BEAT;
static constexpr double TOTAL = 16.0 * BAR;          // 30.0 s
static constexpr double GUI_UNTIL = 13.0 * BAR;      // danach Endkarte

struct ReelAccess
{
    static void tick (QuadShooterEditor& e)              { e.timerCallback(); }
    static void changed (QuadShooterEditor& e)           { e.changeListenerCallback (nullptr); }
    static LibraryPanel& lib (QuadShooterEditor& e)      { return e.library; }
    static SlotView& slot (QuadShooterEditor& e, int i)  { return *e.slots[i]; }
    static void select (QuadShooterEditor& e, int i)     { e.selectSlot (i); }
    static void setEntries (LibraryPanel& l, std::vector<SampleLibrary::Entry> en, const juce::String& folder)
    {
        l.all = std::move (en);
        l.openFolder (folder);
    }
    static void selectRow (LibraryPanel& l, int row)
    {
        l.suppressAudition = true;
        l.list.selectRow (row);
        l.suppressAudition = false;
    }
    static juce::Rectangle<int> rowRect (QuadShooterEditor& e, int row)
    {
        auto& l = e.library;
        auto r = l.list.getRowPosition (row, true);
        return e.getLocalArea (&l.list, r);
    }
    static void hover (SlotView& s, bool h) { s.dragHover = h; s.repaint(); }
    static juce::Rectangle<int> in (QuadShooterEditor& e, juce::Component& c) { return e.getLocalArea (c.getParentComponent(), c.getBounds()); }

    static juce::var rects (QuadShooterEditor& e)
    {
        auto* o = new juce::DynamicObject();
        auto put = [o] (const char* k, juce::Rectangle<int> r)
        {
            juce::Array<juce::var> a { r.getX(), r.getY(), r.getWidth(), r.getHeight() };
            o->setProperty (k, a);
        };
        put ("gui", e.getLocalBounds());
        put ("library", in (e, e.library));
        for (int s = 0; s < numSlots; ++s)
        {
            put (("slot" + slotNames[s]).toRawUTF8(), in (e, *e.slots[s]));
            put (("pitch" + slotNames[s]).toRawUTF8(), in (e, e.slots[s]->pitchSlider));
            put (("note" + slotNames[s]).toRawUTF8(), e.slots[s]->notePill + in (e, *e.slots[s]).getPosition());
        }
        put ("xy", in (e, e.xyPad));
        put ("edit", in (e, e.editStrip));
        put ("keySync", in (e, e.keySyncButton));
        put ("chromatic", in (e, e.chromaticButton));
        put ("mAmp", e.mAmp.bounds);
        put ("mDelay", e.mDelay.bounds);
        put ("mReverb", e.mReverb.bounds);
        put ("mClip", e.mClip.bounds);
        put ("mOut", e.mOut.bounds);
        put ("dlyOn", in (e, e.dlyOn));
        put ("revOn", in (e, e.revOn));
        put ("revLeft", in (e, e.moveLeft[fxReverb]));
        put ("dlyPing", in (e, e.dlyPing));
        put ("clipOn", in (e, e.clipOn));
        put ("clipDrive", in (e, e.kClipDrive.slider));
        return juce::var (o);
    }

    static int run (int argc, char** argv);
};

struct FixedPlayHead : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (BPM);
        p.setTimeSignature (TimeSignature { 4, 4 });
        p.setIsPlaying (true);
        return p;
    }
};

static void setParam (QuadShooterProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static double ease (double x) { x = juce::jlimit (0.0, 1.0, x); return x * x * (3.0 - 2.0 * x); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    return ReelAccess::run (argc, argv);
}

int ReelAccess::run (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: ShooterTest <out-dir> <sample-dir>\n"); return 1; }
    const juce::File outDir { juce::String (argv[1]) };
    const juce::File sampleDir { juce::String (argv[2]) };
    outDir.getChildFile ("frames").createDirectory();

    auto files = sampleDir.findChildFiles (juce::File::findFiles, false, "*.wav");
    std::sort (files.begin(), files.end(), [] (auto& a, auto& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    if (files.size() < 4) { std::printf ("need 4 samples\n"); return 1; }

    QuadShooterProcessor p;
    FixedPlayHead ph;
    p.setPlayHead (&ph);
    p.setPlayConfigDetails (0, 2, SR, 2048);
    p.prepareToPlay (SR, 2048);
    p.showLibrary = true;

    // Tonerkennung der Samples
    for (int i = 0; i < 4; ++i)
    {
        QuadShooterProcessor tmp;
        tmp.loadSample (0, files[i]);
        const auto d = tmp.getDetectedPitch (0);
        std::printf ("detect %s: %s %.2f\n", files[i].getFileName().toRawUTF8(), d.tonal ? "tonal" : "no pitch", d.midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> edp (p.createEditor());
    auto& ed = *dynamic_cast<QuadShooterEditor*> (edp.get());
    ed.setSize (ed.getWidth(), ed.getHeight());

    std::vector<SampleLibrary::Entry> entries;
    for (auto& f : files)
    {
        SampleLibrary::Entry e;
        e.file = f; e.name = f.getFileNameWithoutExtension(); e.folder = sampleDir.getFileName();
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
        e.seconds = rd != nullptr ? rd->lengthInSamples / rd->sampleRate : 0.0;
        entries.push_back (e);
    }
    auto& lib = ReelAccess::lib (ed);
    ReelAccess::setEntries (lib, entries, sampleDir.getFileName());
    ed.resized();
    ReelAccess::tick (ed);

    // ---------------------------------------------------------------- Ablauf
    struct Event { double t; std::function<void()> fn; };
    std::vector<Event> ev;
    juce::MidiBuffer pendingMidi;
    auto at = [&] (double t, std::function<void()> fn) { ev.push_back ({ t, std::move (fn) }); };
    auto note = [&] (double t, int n, int vel = 110, double len = BEAT * 0.9)
    {
        at (t, [&, n, vel] { pendingMidi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) vel), 0); });
        at (t + len, [&, n] { pendingMidi.addEvent (juce::MidiMessage::noteOff (1, n), 0); });
    };
    auto b = [] (double beat) { return beat * BEAT; };

    juce::Array<juce::var> cursor, chips, clicks;
    auto kf = [&] (double t, juce::Point<float> pt, bool pressed = false)
    {
        juce::Array<juce::var> a;
        a.add (t); a.add (pt.x); a.add (pt.y); a.add (pressed);
        cursor.add (a);
    };
    auto centre = [] (juce::Rectangle<int> r) { return r.toFloat().getCentre(); };

    // Grundeinstellungen
    setParam (p, ParamIDs::gain, 0.0f);   // Titel (nur Vorhoeren) etwas lauter
    setParam (p, ParamIDs::limiter, 1.0f);
    p.activeSlot = 0;

    // Titel: Stab A aus der Library vorhoeren
    for (double beat : { 0.0, 1.0, 2.0, 3.0 })
        at (b (beat), [&] { p.previewFile (files[2]); });
    at (b (3.9), [&] { setParam (p, ParamIDs::gain, -4.0f); });

    // Drag: 4 Samples in A, B, C, D (Drops auf Beat 5.5, 7.5, 9.5, 11.5)
    const juce::Point<float> start (900.0f, 620.0f);
    kf (b (3.5), start);
    for (int i = 0; i < 4; ++i)
    {
        const double drop = b (5.5 + 2.0 * i), pick = drop - b (1.3);
        const auto rowPt = centre (ReelAccess::rowRect (ed, i + 1));   // Zeile 0 = ".."
        const auto slotPt = centre (ReelAccess::in (ed, ReelAccess::slot (ed, i)));
        kf (pick - b (0.45), rowPt);
        kf (pick, rowPt, true);
        kf (drop, slotPt, true);
        kf (drop + b (0.25), slotPt);
        juce::Array<juce::var> chip;
        chip.add (pick); chip.add (drop); chip.add (files[i].getFileNameWithoutExtension()); chip.add (i);
        chips.add (chip);
        at (pick, [&, i] { ReelAccess::selectRow (lib, i + 1); });
        at (drop - b (0.35), [&, i] { ReelAccess::hover (ReelAccess::slot (ed, i), true); });
        at (drop, [&, i]
        {
            ReelAccess::hover (ReelAccess::slot (ed, i), false);
            p.loadSample (i, files[(size_t) i]);
            ReelAccess::changed (ed);
            ReelAccess::select (ed, i);
            p.triggerPreview (i);
        });
    }

    // XY-Mix: Noten auf jedem Beat, Puck faehrt die Ecken ab (Beat 14..24)
    struct XYK { double beat; float x, y; };
    const std::vector<XYK> xyPath { { 13, 0.5f, 0.5f }, { 14, 0.5f, 0.5f }, { 16, 0.06f, 0.94f }, { 18, 0.94f, 0.94f },
                                    { 20, 0.94f, 0.06f }, { 22, 0.06f, 0.06f }, { 23.5, 0.5f, 0.5f } };
    auto xyAt = [&] (double t) -> juce::Point<float>
    {
        const double beat = t / BEAT;
        if (beat <= xyPath.front().beat) return { xyPath.front().x, xyPath.front().y };
        for (size_t k = 1; k < xyPath.size(); ++k)
            if (beat <= xyPath[k].beat)
            {
                const double u = ease ((beat - xyPath[k - 1].beat) / (xyPath[k].beat - xyPath[k - 1].beat));
                return { (float) (xyPath[k - 1].x + u * (xyPath[k].x - xyPath[k - 1].x)),
                         (float) (xyPath[k - 1].y + u * (xyPath[k].y - xyPath[k - 1].y)) };
            }
        return { xyPath.back().x, xyPath.back().y };
    };
    for (int k = 13; k < 24; ++k)
        note (b (k), 60, 112);
    {
        // Cursor auf dem Puck
        auto xyR = ReelAccess::in (ed, ed.xyPad).toFloat().reduced (16.0f).withTrimmedTop (22.0f);
        auto puck = [xyR] (juce::Point<float> v) { return juce::Point<float> (xyR.getX() + v.x * xyR.getWidth(), xyR.getBottom() - v.y * xyR.getHeight()); };
        kf (b (13.0), puck (xyAt (b (13))));
        kf (b (13.7), puck (xyAt (b (14))), true);
        for (double beat = 14.0; beat <= 23.5; beat += 0.25)
            kf (b (beat), puck (xyAt (b (beat))), true);
        kf (b (23.8), puck (xyAt (b (23.5))));
    }

    // Shape: pro Slot in der Edit-Zeile (Beat 24..34): B Pitch -3, D Decay kurz, C Low Cut
    for (int k = 24; k < 34; ++k)
        note (b (k), 60, 112);
    auto knobDrag = [&] (int slotIdx, juce::Slider& knob, const juce::String& id, double b0, double b1, float from, float to, float dy)
    {
        const auto sc = centre (ReelAccess::in (ed, ReelAccess::slot (ed, slotIdx))).translated (0.0f, -40.0f);
        kf (b (b0 - 1.0), sc); kf (b (b0 - 0.8), sc, true); kf (b (b0 - 0.65), sc);
        at (b (b0 - 0.8), [&, slotIdx] { ReelAccess::select (ed, slotIdx); });
        auto kr = ReelAccess::in (ed, knob).toFloat();
        const juce::Point<float> kc (kr.getCentreX(), kr.getCentreY() - 6.0f);
        kf (b (b0 - 0.25), kc); kf (b (b0), kc, true);
        kf (b (b1), kc.translated (0.0f, dy), true); kf (b (b1 + 0.25), kc.translated (0.0f, dy));
        auto* prm = p.apvts.getParameter (id);
        const float n0 = prm->convertTo0to1 (from), n1 = prm->convertTo0to1 (to);
        for (double beat = b0; beat <= b1 + 0.001; beat += 0.125)
        {
            const float v = prm->convertFrom0to1 (n0 + (float) ease ((beat - b0) / (b1 - b0)) * (n1 - n0));
            at (b (beat), [&, id, v] { setParam (p, id, v); });
        }
    };
    auto& es = ed.editStrip;
    knobDrag (1, es.kTune.slider,   ParamIDs::tune (1),   25.0, 26.5, 0.0f, -3.0f, 46.0f);
    knobDrag (3, es.kDecay.slider,  ParamIDs::decay (3),  28.0, 29.5, 10000.0f, 160.0f, 60.0f);
    knobDrag (2, es.kLowCut.slider, ParamIDs::lowCut (2), 31.0, 32.5, 20.0f, 380.0f, -40.0f);

    // One big clap: Clipper an, Drive hoch, Auto Gain an (Beat 34..42)
    {
        for (int k = 34; k < 42; ++k)
            note (b (k), 60, 112);
        const auto con = centre (ReelAccess::in (ed, ed.clipOn));
        kf (b (33.5), con); kf (b (34.0), con, true); kf (b (34.2), con);
        at (b (34.0), [&] { setParam (p, ParamIDs::clipOn, 1.0f); });
        auto dr = ReelAccess::in (ed, ed.kClipDrive.slider).toFloat();
        const juce::Point<float> dc (dr.getCentreX(), dr.getCentreY() - 6.0f);
        kf (b (34.6), dc); kf (b (35.0), dc, true); kf (b (39.0), dc.translated (0.0f, -70.0f), true); kf (b (39.3), dc.translated (0.0f, -70.0f));
        for (double beat = 35.0; beat <= 39.001; beat += 0.125)
        {
            const float v = (float) (ease ((beat - 35.0) / 4.0) * 10.0);
            at (b (beat), [&, v] { setParam (p, ParamIDs::clipDrive, v); });
        }
    }

    // Space: Delay (Ping-Pong 1/8.) + Reverb, dann Reihenfolge tauschen (Beat 42..52)
    {
        at (b (41.8), [&] { setParam (p, ParamIDs::dlyPing, 1.0f);
                             setParam (p, ParamIDs::dlyFb, 0.45f); setParam (p, ParamIDs::dlyMix, 0.4f);
                             setParam (p, ParamIDs::revSize, 0.7f); setParam (p, ParamIDs::revMix, 0.35f); });
        const auto dOn = centre (ReelAccess::in (ed, ed.dlyOn));
        const auto rOn = centre (ReelAccess::in (ed, ed.revOn));
        kf (b (41.3), dOn); kf (b (41.8), dOn, true); kf (b (42.0), dOn);
        at (b (41.8), [&] { setParam (p, ParamIDs::dlyOn, 1.0f); });
        kf (b (43.6), rOn); kf (b (44.0), rOn, true); kf (b (44.2), rOn);
        at (b (44.0), [&] { setParam (p, ParamIDs::revOn, 1.0f); });
        const auto rl = centre (ReelAccess::in (ed, ed.moveLeft[fxReverb]));
        kf (b (47.4), rl); kf (b (47.8), rl, true); kf (b (48.0), rl);
        at (b (47.8), [&] { ed.moveFx (fxReverb, -1); });
        at (b (47.85), [&] { ReelAccess::changed (ed); });
        for (double beat : { 42.0, 44.0, 46.0, 48.0, 50.0, 52.0 })
            note (b (beat), 60, 112, BEAT * 0.5);
    }

    std::sort (ev.begin(), ev.end(), [] (auto& x, auto& y) { return x.t < y.t; });

    // ---------------------------------------------------------------- Rendern
    const int totalSamples = (int) std::round (TOTAL * SR);
    juce::AudioBuffer<float> audio (2, totalSamples);
    audio.clear();
    juce::AudioBuffer<float> blk (2, 2048);
    size_t evIdx = 0;
    int pos = 0;
    const int numFrames = (int) std::round (TOTAL * FPS);
    juce::var rectsBefore = ReelAccess::rects (ed), rectsAfter;

    auto renderTo = [&] (int endSample)
    {
        while (pos < endSample)
        {
            // naechstes Event in diesem Abschnitt?
            int segEnd = endSample;
            if (evIdx < ev.size())
                segEnd = juce::jmin (segEnd, juce::jmax (pos, (int) std::round (ev[evIdx].t * SR)));
            if (segEnd == pos && evIdx < ev.size())
            {
                ev[evIdx++].fn();
                continue;
            }
            int n = juce::jmin (2048, segEnd - pos);
            blk.setSize (2, n, false, false, true);
            blk.clear();
            p.processBlock (blk, pendingMidi);
            pendingMidi.clear();
            for (int ch = 0; ch < 2; ++ch)
                audio.copyFrom (ch, pos, blk, ch, 0, n);
            pos += n;
        }
    };

    for (int f = 0; f < numFrames; ++f)
    {
        const double t = f / FPS;
        const auto xy = xyAt (t);
        setParam (p, ParamIDs::xyX, xy.x);
        setParam (p, ParamIDs::xyY, xy.y);
        renderTo ((int) std::round ((f + 1) / FPS * SR));

        if (t < GUI_UNTIL)
        {
            ReelAccess::tick (ed);
            auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
            auto file = outDir.getChildFile ("frames").getChildFile (juce::String (f).paddedLeft ('0', 4) + ".jpg");
            file.deleteFile();
            juce::FileOutputStream os (file);
            juce::JPEGImageFormat jpg;
            jpg.setQuality (0.93f);
            jpg.writeImageToStream (img, os);
        }
        if (f % 90 == 0)
            std::printf ("frame %d / %d\n", f, numFrames);
    }
    rectsAfter = ReelAccess::rects (ed);

    // Audio schreiben
    {
        auto wav = outDir.getChildFile ("audio.wav");
        wav.deleteFile();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (wav), SR, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }

    // Daten fuer die Komposition
    auto* root = new juce::DynamicObject();
    root->setProperty ("fps", FPS);
    root->setProperty ("beat", BEAT);
    root->setProperty ("guiUntil", GUI_UNTIL);
    root->setProperty ("total", TOTAL);
    root->setProperty ("cursor", cursor);
    root->setProperty ("chips", chips);
    root->setProperty ("clicks", clicks);
    root->setProperty ("rects", rectsBefore);
    root->setProperty ("rectsAfter", rectsAfter);
    outDir.getChildFile ("events.json").replaceWithText (juce::JSON::toString (juce::var (root)));
    std::printf ("done, peak %.3f\n", audio.getMagnitude (0, audio.getNumSamples()));
    return 0;
}
