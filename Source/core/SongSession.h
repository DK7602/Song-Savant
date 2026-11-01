// Song Savant - the song you're working on: what you described, and the palettes made for it. Saved with the
// project (plug-in) or the app, and rebuilt the same way when it's opened again.
#pragma once

#include "Palette.h"
#include "StarterParts.h"
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
        PartsPlan partsPlan() const                   { return planParts (makeBrief(), form.chords, partsSeed); }

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
