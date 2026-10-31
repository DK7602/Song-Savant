// Song Savant - the catalog: every sound you own, from every library Song Savant can read, in one list.
//   Steinberg (HALion, HALion Sonic, Retrologue, Padshop, Groove Agent, Drum Machine): MediaBay's database
//   Arturia (Analog Lab): Arturia's preset database (presets you'd have to buy are marked, never suggested)
//   Cymatics: the preset files in each plug-in's folder (names and folders only, so the kind is a guess)
//   Your own .vstpreset files, when MediaBay isn't there to list them
#pragma once

#include "SoundKinds.h"
#include "SystemLocations.h"
#include <atomic>
#include <set>
#include <vector>

namespace ss
{
    struct Sound
    {
        juce::String name;            // exactly as the instrument shows it (what you search for)
        juce::String instrument;      // the plug-in that plays it: "HALion Sonic", "Retrologue", "Analog Lab V"
        juce::String library;         // "Iconica Sketch", "Retrologue 2", "Factory"
        juce::String origin;          // more about where it comes from: Arturia's synth ("Prophet V3")
        juce::String source;          // "MediaBay", "Arturia", "Cymatics", "Preset file"
        Kind kind = Kind::other;
        juce::String detail;          // a finer kind: "E. Piano", "Synth Bass", "Evolving Pad"
        juce::StringArray styles;     // Pop, Country, 80s, Ambient...
        juce::StringArray character;  // Warm, Bright, Analog, Soft...
        juce::String description;
        int rating = 0;               // 0..5
        bool owned = true;            // false: you'd have to buy it (Arturia "buy")
        bool yours = false;           // a preset you saved yourself
        bool guessed = false;         // its kind was guessed from its name
        juce::String location;        // file path or MediaBay address, for reference
        juce::String searchText;      // everything above in lower case, for searching
    };

    struct CatalogSource
    {
        juce::String name;            // "Steinberg (MediaBay)"
        juce::File file;
        int sounds = 0, notOwned = 0;
        juce::String problem;         // why it couldn't be read, if it couldn't
    };

    struct Catalog
    {
        std::vector<Sound> sounds;    // sorted by kind, then name
        std::vector<CatalogSource> sources;
        juce::Time built;
        double seconds = 0;

        juce::StringArray instruments() const;   // every instrument that has sounds, by name
        int count (Kind, bool ownedOnly = true) const;
    };

    // The libraries one at a time (also used by the tests)
    std::vector<Sound> readMediaBay (const juce::File& db, juce::String& problem);
    std::vector<Sound> readArturia (const juce::File& db, juce::String& problem);
    std::vector<Sound> readCymatics (const juce::Array<juce::File>& roots);
    std::vector<Sound> readPresetFiles (const SystemLocations&);

    Catalog buildCatalog (const SystemLocations&, const std::atomic<bool>& cancel);

    // Finding sounds: every word must appear somewhere (name, instrument, library, kind, tags, description)
    struct SoundFilter
    {
        juce::String text;
        std::set<Kind> kinds;         // empty = every kind
        juce::String instrument;      // empty = every instrument
        bool ownedOnly = true;
        bool yoursOnly = false;
    };
    std::vector<int> findSounds (const Catalog&, const SoundFilter&, int limit = 5000);   // indexes into sounds

    // What to do in Cubase (or another DAW) to load it, in plain words
    juce::String howToLoad (const Sound&);
}
