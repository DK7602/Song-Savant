// Song Savant - the arrangement map: the song's shape (intro, verse, chorus...), which parts of the palette play in
// each section, and the whole song as MIDI (each section's starter parts in place, a marker for each section).
// The shape comes from the style (StyleGuide::form), or from a Key Compass song's own sections and chords.
#pragma once

#include "StarterParts.h"
#include "Palette.h"
#include "Patterns.h"

namespace ss
{
    // One row of the map: a card of the palette that plays a starter part
    struct ArrangementPart
    {
        juce::String role;                        // "Bass"
        juce::String label;                       // "Bass - Warm Analog Bass" (the track's name)
        PartType type = PartType::none;
        Kind instrument = Kind::other;
        bool extra = false;                       // an optional part: it plays where everything plays
    };
    std::vector<ArrangementPart> arrangementParts (const Palette&, const Catalog*);

    // Which key plays which drum for this palette: the drum card's own Song Savant drum map (same name as the sound),
    // or General MIDI
    DrumNotes drumNotesFor (const Palette&, const Catalog*);

    struct ArrangementSection
    {
        juce::String name;                        // "Chorus"
        int bars = 8;
        int progression = 0;                      // the style's progressions: 0 = the main one, 1 = the verse's, 2 = the bridge's
        juce::StringArray chords;                 // from a Key Compass song, one per bar (empty: the style's progression)
        std::map<juce::String, juce::String> play;// role -> "" (silent), "on"; the drums: "light", "groove", "build", "full"
        std::map<juce::String, juce::String> patterns;   // role -> the section's own pattern, when it was made different
        juce::String modeFor (const juce::String& role) const;
        juce::String patternFor (const juce::String& role) const;   // the pattern the part plays here: its own, or the section's name
    };

    // The song shapes on offer: the style's full song, a short one, and the Key Compass song's own sections
    enum class SongShape { full = 0, shortSong = 1, keyCompass = 2 };

    struct Arrangement
    {
        SongShape shape = SongShape::full;
        juce::String signature;                   // what it was made for: the style, the parts, the shape, the Key Compass song
        std::vector<ArrangementSection> sections;
        int totalBars() const;
        bool empty() const                        { return sections.empty(); }
        std::unique_ptr<juce::XmlElement> toXml() const;
        static Arrangement fromXml (const juce::XmlElement&);
    };

    // A Key Compass song's sections and their chords (empty when none is picked)
    struct KeyCompassShape
    {
        juce::String name;
        juce::StringArray sections;
        std::vector<juce::StringArray> chords;
        bool empty() const                        { return sections.isEmpty(); }
    };

    juce::StringArray songShapeNames (const KeyCompassShape&);       // "The full song", "A short song", "Key Compass: Back Roads"
    juce::String arrangementSignature (const SongBrief&, const std::vector<ArrangementPart>&, const KeyCompassShape&, SongShape);
    Arrangement makeArrangement (const PartsPlan&, const std::vector<ArrangementPart>&, const KeyCompassShape&, SongShape);

    // Clicking a box on the map: a part switches on or off; the drums step through off, light, groove, build, full
    void toggleArrangementCell (Arrangement&, int section, const ArrangementPart&);
    const juce::StringArray& drumModes();         // "", "light", "groove", "build", "full"

    // Editing a section (from its menu)
    void setSectionBars (Arrangement&, int section, int bars);
    void copySection (Arrangement&, int section); // a copy right after it
    void removeSection (Arrangement&, int section);
    void moveSection (Arrangement&, int section, int by);
    // Dragged to a new place: `gap` is where it lands, 0 (before the first section) .. the number of sections (the end)
    void moveSectionTo (Arrangement&, int section, int gap);
    // A new section at `at` (0 .. the number of sections). A name the song already has ("Verse") is a copy of the first
    // one (it plays the same pattern); a new kind ("Bridge") gets the style's own length, chords and parts for it.
    void addSection (Arrangement&, int at, const juce::String& name, const PartsPlan&, const std::vector<ArrangementPart>&);
    juce::StringArray sectionNamesInSong (const Arrangement&);                        // each name once, in order
    juce::StringArray newSectionNames (const StyleGuide&, const Arrangement&);         // kinds the song doesn't have yet

    std::vector<Chord> sectionChords (const PartsPlan&, const ArrangementSection&);

    // ---- Patterns (the piano roll). A part plays one pattern per section name ("Chorus": every chorus), unless a
    // section was made different. The patterns you edited are in a PatternBook; the rest are the suggestion.
    juce::StringArray patternNames (const Arrangement&, const juce::String& role);        // in the order they first come
    std::vector<int> sectionsUsing (const Arrangement&, const juce::String& role, const juce::String& pattern);
    juce::String makeSectionDifferent (Arrangement&, int section, const juce::String& role); // its own pattern: its name
    void setPatternBars (Arrangement&, const juce::String& role, const juce::String& pattern, int bars);  // every section using it
    int mainSection (const Arrangement&, const juce::String& role);   // what a card's starter part stands for: its first chorus
    // Song Savant's suggestion for a part in a section (beats from the section's start), whether it plays there or not
    std::vector<PatternNote> suggestedNotes (const PartsPlan&, const ArrangementSection&, const ArrangementPart&, const DrumNotes&);
    // What a part plays in a section: your pattern if you edited it, else the suggestion; nothing where it's switched off
    std::vector<PatternNote> sectionNotes (const PartsPlan&, const Arrangement&, int section, const ArrangementPart&,
                                           const DrumNotes&, const PatternBook*);
    // A card's starter part (the SONG page): your pattern for its first chorus when you edited it, else the suggestion
    juce::MidiMessageSequence starterPart (const PartsPlan&, const Arrangement&, const PatternBook*, const ArrangementPart&, const DrumNotes&);
    // How long the starter parts are: the plan's 8 bars, or longer when one of your parts is
    int starterBars (const PartsPlan&, const Arrangement&, const PatternBook*, const std::vector<ArrangementPart>&);

    // The whole song: one track per part, each section's part in place (yours where you edited it)
    std::vector<NamedPart> arrangeParts (const PartsPlan&, const Arrangement&, const std::vector<ArrangementPart>&, const DrumNotes&,
                                         const PatternBook* = nullptr);
    // One part only: in one section (`section` >= 0), or across the whole song
    std::vector<NamedPart> arrangePart (const PartsPlan&, const Arrangement&, const ArrangementPart&, const DrumNotes&,
                                        const PatternBook*, int section = -1);
    bool writeArrangement (const PartsPlan&, const Arrangement&, const std::vector<NamedPart>&, const juce::File&);
}
