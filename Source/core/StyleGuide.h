// Song Savant - what Song Savant knows about each genre and style (Resources/StyleGuide.json): what it is, its tempo,
// feel and keys, the sounds and instruments it uses, how its drums, bass and chords play, its arrangement, mix and
// tips, and the rules the starter MIDI parts follow. A style adds to or replaces its genre's entries.
#pragma once

#include "Music.h"
#include <map>

namespace ss
{
    // A drum groove on a grid of 16th notes: one lane per drum, one character per step.
    //   . rest   x hit   X accent   o soft   g ghost   r two quick hits (32nds)   t three quick hits (a triplet roll)
    struct Groove
    {
        int steps = 16;                                   // 16 = one bar, 32 = two bars
        int swing = 50;                                   // 50 = straight; 58 = a little swing; 66 = triplet swing
        bool crash = false;                               // a crash on the first beat of the part and halfway through
        std::map<juce::String, juce::String> lanes;       // kick, snare, clap, rim, hat, openhat, tomlow... -> pattern
        std::map<juce::String, juce::String> fill;        // the last bar's lanes (16 steps), for a fill
    };

    struct StyleGuide
    {
        juce::String id, name, genre;                     // "trap.ukdrill", "UK Drill", "Trap / Drill"
        juce::String summary, tempo, feel, keys;
        juce::StringArray sounds, drums, bass, harmony, arrangement, mix, tips;

        // for the starter parts
        std::vector<juce::StringArray> progressions;      // Roman numerals: { "i", "VI", "III", "VII" }
        Groove groove;
        juce::String bassStyle, chordStyle, melodyStyle;  // "808", "rootfifth"...; "pad", "strum"...; "bells", "hook"...
    };

    StyleGuide styleGuide (Genre, const juce::String& styleId = {});
}
