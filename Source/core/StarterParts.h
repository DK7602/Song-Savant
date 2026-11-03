// Song Savant - starter MIDI parts: short parts (8 bars) for the palette, in the song's key and tempo, played the
// way the style plays (StyleGuide): drums from its groove, a bass line, chords, pads and a hook idea. Dragged into
// the DAW as MIDI files.
#pragma once

#include "StyleGuide.h"
#include "Parts.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <optional>

namespace ss
{
    struct Chord
    {
        int root = 0;                       // pitch class, 0 = C
        std::vector<int> intervals;         // semitones above the root: { 0, 3, 7, 10 } for a minor 7th
        juce::String name;                  // "Am7"
        std::vector<int> pitchClasses() const;
    };

    // "Am7", "F#", "Bbmaj7", "Csus2", "G/B" (the bass note after the slash is left out)
    std::optional<Chord> chordFromName (const juce::String&);

    // A Roman numeral in a key: "VI", "i", "bVII", "ii7", "IVmaj7", "i9", "vii°". Upper case is major, lower case minor;
    // the degree counts in the key's own scale (so in A minor, VI is F).
    std::optional<Chord> chordFromRoman (const juce::String& numeral, int tonic, bool minorMode, bool flats);

    juce::String pitchName (int pitchClass, bool flats);
    bool keyUsesFlats (int key);            // key: 0..11 major, 12..23 minor

    struct PartsPlan
    {
        int key = 21;                       // 0..11 = C..B major, 12..23 = C..B minor
        bool keyGiven = false;              // false when Song Savant picked the key
        double bpm = 120;
        int bars = 8;
        std::vector<Chord> chords;          // one per bar
        juce::String chordSource;           // "Key Compass: Back Roads", or the style's progression
        StyleGuide guide;
        juce::uint32 seed = 1;              // a new seed is a new idea

        int tonic() const                   { return key % 12; }
        bool minor() const                  { return key >= 12; }
        juce::String chordLine() const;     // "Am - F - C - G"
    };

    // A progression's chords in a key: { "i", "VI", "III", "VII" } in A minor = "Am - F - C - G" (a minor progression
    // in a major key is played from the relative minor, and the other way round)
    juce::String progressionInKey (const juce::StringArray& numerals, int key);

    // The chords for one of the style's progressions (0 = the plan's main one, 1 = the next...), one per bar
    std::vector<Chord> progressionChords (const PartsPlan&, int index, int bars);

    // The plan for the song: its key and tempo (or the style's), and its chords (the Key Compass song's when given)
    PartsPlan planParts (const SongBrief&, const juce::StringArray& songChords = {}, juce::uint32 seed = 1);

    enum class PartType { drums, bass, chords, pad, hook, none };
    PartType partTypeFor (const PartSpec&);
    juce::String partTypeName (PartType);

    // Which key plays which drum: General MIDI, or the pads' names in a Cubase drum map (a Song Savant one, say)
    struct DrumNotes
    {
        std::map<juce::String, int> notes;  // "kick" -> 36, "snare" -> 38...
        juce::String source = "General MIDI";
        int note (const juce::String& lane) const;
    };
    DrumNotes generalMidiDrums();
    std::optional<DrumNotes> drumNotesFromMap (const juce::File& cubaseDrumMap);

    static constexpr int ticksPerQuarter = 480;
    // `instrument`: the card's kind of sound, so a guitar strums or picks and a piano doesn't strum like a guitar
    juce::MidiMessageSequence makePart (const PartsPlan&, PartType, const DrumNotes& = generalMidiDrums(), Kind instrument = Kind::other);
    juce::String describePart (const PartsPlan&, PartType, const DrumNotes& = generalMidiDrums(), Kind instrument = Kind::other);
    juce::String chordStyleFor (const PartsPlan&, Kind instrument);

    struct NamedPart
    {
        juce::String name;
        juce::MidiMessageSequence notes;
        juce::String text;                  // a text event at its start (a drum track: "drums: <its drum map>")
    };
    // A MIDI file with the tempo, time and key signature, and one track per part. Markers (in ticks) name places in
    // the song (sections); `bars` is its length when it isn't the plan's.
    bool writeMidiFile (const PartsPlan&, const std::vector<NamedPart>&, const juce::File&,
                        const std::vector<std::pair<double, juce::String>>& markers = {}, int bars = 0);
}
