// Song Savant - mapping a drum kit. Song Savant Scanner loads a kit preset you saved in your DAW, plays every key
// and records what comes out; this file listens to each recording (how low, how bright, how noisy, how long), names
// the sound (Kick, Snare, Closed Hat...) and writes a Cubase drum map so the pads have names in the drum editor.
#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace ss
{
    // What one hit sounds like (from a mono recording that starts at the note-on)
    struct HitFeatures
    {
        float peak = 0;          // loudest sample
        float lowRatio = 0;      // share of the energy below ~150 Hz (kicks, 808s)
        float highRatio = 0;     // share of the energy above ~5 kHz (hats, cymbals)
        float zcr = 0;           // zero crossings per second at the start: high = noisy or bright
        float decay = 0;         // seconds until it has died away (30 dB below its peak)
        float pitchHz = 0;       // rough pitch of the low part (kick ~50 Hz, toms 80-300 Hz)
        int bursts = 1;          // separate hits in the first 60 ms (a clap has several)
        bool silent() const      { return peak < 0.001f; }
    };

    HitFeatures analyseHit (const float* mono, int numSamples, double sampleRate);

    // "Kick", "808", "Snare", "Clap", "Closed Hat", "Open Hat", "Cymbal", "Tom", "Perc", or empty for silence
    juce::String classifyHit (const HitFeatures&);

    struct KitPad
    {
        int note = 0;            // MIDI note (36 = C1 in Cubase)
        juce::String kind;       // what it sounds like: "Kick", "Tom"...
        juce::String name;       // what the drum map calls it: "Kick 2", "Low Tom"...
        HitFeatures features;
    };

    struct KitMap
    {
        juce::String name;       // the preset's name: "Beat Box"
        juce::String plugin;     // "Analog Lab V"
        std::vector<KitPad> pads;// keys that make a sound, low to high
        bool oneSound = false;   // the same sound across the keyboard: no drum map needed
        juce::String error;      // why it couldn't be mapped
        bool ok() const          { return error.isEmpty() && ! pads.empty(); }
    };

    // Names the pads from their kinds: "Kick", "Kick 2"...; toms low to high; spots one sound played across the keys
    void nameKitPads (KitMap&);

    juce::var kitMapToJson (const KitMap&);
    KitMap kitMapFromJson (const juce::var&);

    juce::String noteName (int midiNote);          // Cubase's names: 60 = C3, 36 = C1

    // A Cubase drum map (.drm): load it in Cubase's Drum Map Setup
    juce::String makeCubaseDrumMap (const KitMap&);
    juce::File drumMapsFolder();                    // Documents/Song Savant/Drum Maps
    juce::File saveCubaseDrumMap (const KitMap&);   // writes <name>.drm there; returns the file (or nothing)
}
