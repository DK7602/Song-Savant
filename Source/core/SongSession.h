// Song Savant - the song you're working on: what you described, and the palettes made for it. Saved with the
// project (plug-in) or the app, and rebuilt the same way when it's opened again.
#pragma once

#include "Palette.h"
#include "StarterParts.h"
#include "Arrangement.h"
#include <juce_core/juce_core.h>

namespace ss
{
    struct SongForm
    {
        juce::String description;     // "modern country like Morgan Wallen, warm"
        int genre = -1;               // -1 = from the description
        juce::String styleId;         // empty = any style
        int key = -1;                 // -1 = from the description (or none)
        double bpm = 0;               // 0 = from the description / the style
        juce::String fromSong;        // the Key Compass song it came from, if any
        juce::StringArray chords;     // that song's chords, in order (the starter parts follow them)
        juce::StringArray sections;   // that song's sections ("Verse", "Chorus"...)
        std::vector<juce::StringArray> sectionChords;   // and each section's chords (the arrangement follows them)
    };

    class SongSession
    {
    public:
        SongForm form;
        SongBrief brief;
        std::vector<Palette> palettes;
        int current = 0;
        juce::uint32 seed = 1;
        juce::uint32 partsSeed = 1;                   // starter parts: each NEW IDEA is the next one
        double hostBpm = 0.0;                         // inside a DAW, its tempo: the song follows it (not saved)
        PartsPlan partsPlan() const                   { return planParts (makeBrief(), form.chords, partsSeed); }

        // The arrangement map: made from the style's song shape (or the Key Compass song's sections) and then edited
        Arrangement arrangement;
        SongShape songShape = SongShape::full;
        KeyCompassShape keyCompassShape() const       { return { form.fromSong, form.sections, form.sectionChords }; }
        void updateArrangement (const std::vector<ArrangementPart>&);   // made again when the style, the parts or the shape changed
        void resetArrangement (const std::vector<ArrangementPart>&);    // back to the shape as it comes
        // A new shape. A blank song keeps its own notes: going into one puts your other song's patterns aside, and
        // coming back out brings them back (and the blank song's are put aside for next time).
        void setSongShape (SongShape);

        // Your own parts, made in the piano roll: one pattern per part and section name (saved with the song). They stay
        // until you reset them (NEW IDEA and new palettes leave them alone) and move with the song's key.
        PatternBook patterns;
        PatternBook patternsAside;                    // the other song's (the blank song's, or the full song's while blank)
        Arrangement arrangementAside;                 // ... and its map
        bool isMine (int section, const juce::String& role) const;
        // What the piano roll opens: your pattern (in the song's key) or the suggestion (the section's length)
        Pattern patternAt (int section, const ArrangementPart&, const DrumNotes&) const;
        // Yours from now on, in every section that plays this pattern (switched on where the part was off)
        void storePattern (int section, const ArrangementPart&, Pattern);
        void forgetPattern (int section, const juce::String& role);     // back to Song Savant's suggestion
        // The section gets a pattern of its own (a copy of what it played): its name ("Chorus 3")
        juce::String makeDifferent (int section, const ArrangementPart&, const DrumNotes&);

        // A sound you said isn't in your library: out of every palette (the next choice takes its place, unlocked)
        void forgetSound (const Catalog&, const juce::String& key);

        SongBrief makeBrief() const;                  // the description, with the form's choices on top
        void find (const Catalog&);                   // fresh palettes for the form
        void newPalettes (const Catalog&);            // new takes; locked parts of the current palette stay
        Palette* currentPalette()                     { return current >= 0 && current < (int) palettes.size() ? &palettes[(size_t) current] : nullptr; }

        std::unique_ptr<juce::XmlElement> toXml() const;
        void fromXml (const juce::XmlElement&);       // the palettes come back when restore() gets a catalog
        bool needsRestore() const                     { return pending != nullptr; }
        void restore (const Catalog&);
        void moveTo (const Catalog&);                 // the library was read again: find the same sounds in the new one

    private:
        std::unique_ptr<juce::XmlElement> pending;
    };
}
