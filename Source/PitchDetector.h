#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Tonerkennung fuer One-Shots (YIN). Ueberspringt den Transienten und
// sucht im Ausklang einen stabilen Grundton. Claps, Hats usw. gelten als "nicht tonal".
struct PitchResult
{
    bool  tonal = false;
    float midi = 0.0f;        // MIDI-Note als Kommazahl (60 = C3), inkl. Cents
    float confidence = 0.0f;  // 0..1
    float hz() const { return 440.0f * std::exp2 ((midi - 69.0f) / 12.0f); }
};

namespace PitchDetector
{
    PitchResult detect (const juce::AudioBuffer<float>& buffer, double sampleRate);

    // "C3", "F#1" usw. (C3 = 60, wie Bitwig/Ableton)
    juce::String noteName (int midi);

    // "F#2", "Eb1", "c3" -> MIDI (C3 = 60). -1 wenn ungueltig.
    int parseNoteName (const juce::String& text);
}
