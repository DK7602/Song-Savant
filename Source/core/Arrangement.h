// Song Savant - the arrangement map: the song's shape (intro, verse, chorus...), which parts of the palette play in
// each section, and the whole song as MIDI (each section's starter parts in place, a marker for each section).
// The shape comes from the style (StyleGuide::form), or from a Key Compass song's own sections and chords.
#pragma once

#include "StarterParts.h"
#include "Palette.h"

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
        juce::String modeFor (const juce::String& role) const;
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

    std::vector<Chord> sectionChords (const PartsPlan&, const ArrangementSection&);

    // The whole song: one track per part, each section's starter parts in place
    std::vector<NamedPart> arrangeParts (const PartsPlan&, const Arrangement&, const std::vector<ArrangementPart>&, const DrumNotes&);
    bool writeArrangement (const PartsPlan&, const Arrangement&, const std::vector<NamedPart>&, const juce::File&);
}
