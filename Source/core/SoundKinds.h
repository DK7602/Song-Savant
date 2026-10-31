// Song Savant - the kinds of sound Song Savant thinks in (what a part of a song needs: piano, bass, pad...), and
// how each library's own categories map onto them.
#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace ss
{
    enum class Kind
    {
        piano, keys, organ, guitar, bass, strings, brass, woodwinds,
        pad, lead, synth, pluck, sequence, drums, percussion, vocal, world, fx, other
    };

    const std::vector<Kind>& allKinds();          // in display order
    juce::String kindName (Kind);                 // "Piano", "Keys", "Pad"...
    std::optional<Kind> kindFromName (const juce::String& kindNameText);   // "Pad" -> Kind::pad

    // Steinberg MediaBay: MusicalCategory ("Synth Pad") and MusicalInstrument ("Keyboard|E. Piano")
    Kind kindFromMediaBay (const juce::String& category, const juce::String& instrument);

    // Arturia: type ("Electric Piano") and sub-type ("Classic EP")
    Kind kindFromArturia (const juce::String& type, const juce::String& subtype);

    // A best guess from words in a preset's name or folder ("BASS Apocalypse", "Warm Rhodes", "Kick 808")
    Kind guessKind (const juce::String& words);
}
