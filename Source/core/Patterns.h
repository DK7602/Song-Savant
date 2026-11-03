// Song Savant - your own parts: the patterns you drew or changed in the piano roll.
//
// A part plays one pattern per section name ("Chorus": every chorus plays it) unless a section was made different
// (it then has a pattern of its own, "Chorus 3"). Only the patterns you edited are stored: the rest are Song Savant's
// suggestion, made fresh each time, so NEW IDEA still changes them. A stored pattern remembers the song's key it was
// written in and moves with the song's key; its notes are in beats, so the tempo can change freely. A section longer
// than its pattern repeats it; a shorter one stops early (the notes after the end are kept).
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <map>
#include <vector>

namespace ss
{
    struct PatternNote
    {
        double start = 0.0, length = 1.0;         // in beats (quarter notes) from the pattern's start
        int note = 60, velocity = 100;
        bool operator== (const PatternNote&) const = default;
    };

    struct Pattern
    {
        int bars = 8;
        int key = 0;                              // the song's key when it was written: 0..11 major, 12..23 minor
        std::vector<PatternNote> notes;           // in order of start
        double lengthBeats() const                { return bars * 4.0; }
        bool operator== (const Pattern&) const = default;
    };

    void sortNotes (std::vector<PatternNote>&);

    // The patterns you edited, by part (the card's role, "Bass") and pattern name ("Chorus", "Chorus 3")
    class PatternBook
    {
    public:
        const Pattern* find (const juce::String& role, const juce::String& name) const;
        void set (const juce::String& role, const juce::String& name, Pattern);
        void remove (const juce::String& role, const juce::String& name);
        bool hasAny (const juce::String& role) const;            // the part has a pattern of yours somewhere
        bool empty() const                                       { return patterns.empty(); }
        int size() const                                         { return (int) patterns.size(); }
        void clear()                                             { patterns.clear(); ++changes; }
        int changeCount() const                                  { return changes; }   // goes up with every change

        std::unique_ptr<juce::XmlElement> toXml() const;
        static PatternBook fromXml (const juce::XmlElement&);
        bool operator== (const PatternBook& o) const             { return patterns == o.patterns; }

    private:
        static juce::String keyFor (const juce::String& role, const juce::String& name)   { return role + "|" + name; }
        std::map<juce::String, Pattern> patterns;
        int changes = 0;
    };

    // MIDI (ticks, 480 to the beat) <-> notes (beats)
    std::vector<PatternNote> notesFromSequence (const juce::MidiMessageSequence&);
    juce::MidiMessageSequence sequenceFromNotes (const std::vector<PatternNote>&, double offsetBeats = 0.0, int channel = 1);

    // The nearest way from one key's home note to another's: -6..+5 semitones
    int keyShift (int fromKey, int toKey);
    // Notes written in one key, moved to another: by the distance between the keys' home notes, and from major to
    // minor (or back) the 3rd, 6th and 7th of the scale move with it
    std::vector<PatternNote> moveToKey (std::vector<PatternNote>, int fromKey, int toKey);

    // A pattern played for `bars` bars in `key`: repeated when the section is longer, cut when it's shorter
    std::vector<PatternNote> playPattern (const Pattern&, int bars, int key);
    // The pattern made at least `bars` long, its notes repeated to fill the new bars (before you edit them)
    Pattern unrolled (Pattern, int bars);
}
