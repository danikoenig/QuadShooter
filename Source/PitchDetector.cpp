#include "PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace PitchDetector
{
juce::String noteName (int midi)
{
    return juce::MidiMessage::getMidiNoteName (midi, true, true, 3);
}

int parseNoteName (const juce::String& text)
{
    auto t = text.trim().replace (juce::String::fromUTF8 ("\xe2\x99\xaf"), "#").replace (juce::String::fromUTF8 ("\xe2\x99\xad"), "b");
    if (t.isEmpty())
        return -1;
    static const int pcs[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    const auto letter = juce::CharacterFunctions::toUpperCase (t[0]);
    if (letter < 'A' || letter > 'G')
        return -1;
    int pc = pcs[letter - 'A'];
    int i = 1;
    while (i < t.length() && (t[i] == '#' || t[i] == 'b' || t[i] == 'B'))
    {
        pc += t[i] == '#' ? 1 : -1;
        ++i;
    }
    const auto octText = t.substring (i).trim();
    if (octText.isEmpty() || ! octText.containsOnly ("-0123456789"))
        return -1;
    const int midi = (octText.getIntValue() + 2) * 12 + pc;
    return midi >= 0 && midi <= 127 ? midi : -1;
}

PitchResult detect (const juce::AudioBuffer<float>& buffer, double sr)
{
    PitchResult result;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (n < 512 || numCh == 0 || sr <= 0.0)
        return result;

    // Mono
    std::vector<float> x ((size_t) n, 0.0f);
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* d = buffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            x[(size_t) i] += d[i] / (float) numCh;
    }

    // Lange Samples auf ~22 kHz runterrechnen (reicht fuer Grundtoene bis 2 kHz, spart Rechenzeit)
    int decim = 1;
    while (sr / (decim * 2) >= 20000.0)
        decim *= 2;
    if (decim > 1)
    {
        std::vector<float> y ((size_t) (n / decim));
        for (size_t i = 0; i < y.size(); ++i)
        {
            float s = 0.0f;
            for (int k = 0; k < decim; ++k)
                s += x[i * (size_t) decim + (size_t) k];
            y[i] = s / (float) decim;
        }
        x.swap (y);
    }
    const double fs = sr / decim;
    const int len = (int) x.size();

    const int tauMin = juce::jmax (2, (int) (fs / 2000.0));
    const int tauMax = (int) (fs / 35.0);
    const int W = tauMax * 2;
    if (len < W + tauMax + 8)
        return result;

    // Transient ueberspringen: ab Peak + 25 ms
    int peakPos = 0;
    float peak = 0.0f;
    for (int i = 0; i < len; ++i)
        if (std::abs (x[(size_t) i]) > peak) { peak = std::abs (x[(size_t) i]); peakPos = i; }
    if (peak < 1e-5f)
        return result;

    int start = peakPos + (int) (0.025 * fs);
    if (start + W + tauMax >= len)
        start = juce::jmax (0, len - W - tauMax - 1);
    const int stop = juce::jmin (len - W - tauMax - 1, start + (int) (1.0 * fs));

    // Referenzpegel fuer Stille-Erkennung
    auto rmsAt = [&] (int pos)
    {
        double s = 0.0;
        for (int i = 0; i < W; ++i) s += (double) x[(size_t) (pos + i)] * x[(size_t) (pos + i)];
        return std::sqrt (s / W);
    };
    const double refRms = rmsAt (start);

    std::vector<float> d ((size_t) tauMax + 2);
    std::vector<float> midis, aperiod;
    int frames = 0;

    for (int pos = start; pos <= stop; pos += W / 2)
    {
        const double rms = rmsAt (pos);
        if (rms < refRms * 0.03 || rms < 1e-4)
            break;   // ausgeklungen
        ++frames;

        // Differenzfunktion
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            const float* a = x.data() + pos;
            const float* b = a + tau;
            for (int i = 0; i < W; ++i)
            {
                const float df = a[i] - b[i];
                s += (double) df * df;
            }
            d[(size_t) tau] = (float) s;
        }

        // Kumulative normierte Differenz (CMNDF)
        d[0] = 1.0f;
        double run = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            run += d[(size_t) tau];
            d[(size_t) tau] = run > 0.0 ? (float) (d[(size_t) tau] * tau / run) : 1.0f;
        }

        // Erstes Minimum unter der Schwelle
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (d[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
            continue;

        // Parabolische Interpolation
        const float y0 = d[(size_t) best - 1], y1 = d[(size_t) best], y2 = d[(size_t) best + 1];
        const float denom = y0 - 2.0f * y1 + y2;
        const float shift = std::abs (denom) > 1e-9f ? 0.5f * (y0 - y2) / denom : 0.0f;
        const double tau = best + juce::jlimit (-0.5f, 0.5f, shift);

        const double f0 = fs / tau;
        midis.push_back ((float) (69.0 + 12.0 * std::log2 (f0 / 440.0)));
        aperiod.push_back (y1);
    }

    if (midis.size() < 3 || (float) midis.size() < 0.5f * (float) frames)
        return result;

    // Median, danach Ausreisser (Oktavfehler) verwerfen
    auto median = [] (std::vector<float> v)
    {
        std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
        return v[v.size() / 2];
    };
    const float med = median (midis);
    std::vector<float> good;
    for (auto m : midis)
        if (std::abs (m - med) < 0.7f)
            good.push_back (m);
    if (good.size() < 3 || (float) good.size() < 0.6f * (float) midis.size())
        return result;

    double sum = 0.0;
    for (auto m : good) sum += m;

    result.tonal = true;
    result.midi = (float) (sum / (double) good.size());
    result.confidence = juce::jlimit (0.0f, 1.0f, 1.0f - median (aperiod) / 0.15f);
    return result;
}
}
