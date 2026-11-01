// Song Savant - which parts a song needs, and what each one should sound like. Each genre has its band; a style,
// a decade and your own describing words ("warm", "airy", "80s") then shape what each part looks for.
#pragma once

#include "Music.h"
#include "SoundKinds.h"

namespace ss
{
    struct PartSpec
    {
        juce::String role;            // "Drums", "Bass", "Rhythm 1", "Lead", "Atmosphere"...
        juce::String hint;            // "Acoustic guitar strums"
        std::vector<Kind> kinds;      // kinds of sound that fit, best first
        juce::StringArray want;       // words that make a sound fit better ("acoustic", "warm", "e. bass")
        juce::StringArray avoid;      // words that make it fit worse ("distorted", "808")
        bool optional = false;        // an extra, not part of the core band
        juce::String note;            // why this kind of sound, for "why it works" ("Leaves the low end to the 808...")
        juce::StringArray alsoOffer;  // another kind of sound to keep among the alternatives (up to 2), e.g. deep subs
        juce::String alsoOfferNote;   // ...and why you might pick one of those instead
    };

    std::vector<PartSpec> partsFor (const SongBrief&);

    // Opposites of describing words: asking for "warm" counts against "cold" and "harsh"
    juce::StringArray oppositesOf (const juce::String& word);
}
