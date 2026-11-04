// Song Savant - palettes: for each part of the song, the sound from your library that fits best, plus alternatives.
// A sound fits a part when it's the right kind of sound, its tags and name say what the part wants (your words count
// most), nothing it says counts against it, and it's tagged with the song's genre. Several palettes are made, each a
// different take; locked parts are kept when you ask for new ones.
#pragma once

#include "Catalog.h"
#include "Parts.h"

namespace ss
{
    struct Pick
    {
        int sound = -1;                    // index into the catalog's sounds
        float score = 0;
        juce::StringArray reasons;         // why it fits: "warm", "grand", "tagged Pop"
        bool isValid() const               { return sound >= 0; }
    };

    struct PaletteSlot
    {
        PartSpec part;
        Pick pick;
        std::vector<Pick> alternatives;    // the next best, in order
        bool locked = false;
        juce::String keepKey;              // a locked sound's identity (survives a rebuilt catalog)
        int choice = 0;                    // 0 = the pick, 1.. = an alternative you switched to

        int numOptions() const             { return pick.isValid() ? 1 + (int) alternatives.size() : 0; }
        const Pick& current() const        { return choice > 0 && choice <= (int) alternatives.size() ? alternatives[(size_t) choice - 1] : pick; }
        void cycle (int step)              { if (numOptions() > 1) choice = (choice + step + numOptions()) % numOptions(); }
    };

    struct Palette
    {
        juce::String name;                 // "Palette 1"
        std::vector<PaletteSlot> slots;
    };

    // What a drum sound is: a whole kit you play yourself (Groove Agent, Drum Machine, "Drumset"), a ready-made loop
    // that plays its own beat (HALion Sonic "Beats"), or one drum (a kick, a snare). A drum part wants a kit.
    enum class DrumForm { kit, loop, single, unknown };
    DrumForm drumForm (const Sound&);
    // Cubase shows this kit's pad names by itself (Groove Agent kits, Groove Grabber kits): no drum map needed. Kits
    // from other instruments (Analog Lab, HALion...) show note numbers until you make a drum map (MAKE A DRUM MAP).
    bool namesItsPads (const Sound&);

    struct PaletteOptions
    {
        int palettes = 3;
        int alternatives = 6;
        juce::uint32 seed = 1;             // a new seed = new takes on the same song
    };

    // A sound's identity that survives rebuilding the catalog: "name|instrument|library"
    juce::String soundKey (const Sound&);
    int findSoundByKey (const Catalog&, const juce::String& key);

    // How well a sound fits a part (negative = not at all); reasons says why
    float scoreSound (const Sound&, const PartSpec&, const SongBrief&, juce::StringArray* reasons = nullptr);

    // New palettes for the song. Slots locked in `keep` (by role) keep their sound in every palette.
    std::vector<Palette> makePalettes (const Catalog&, const SongBrief&, const PaletteOptions& = {},
                                       const std::vector<PaletteSlot>& keep = {});

    // "Warm, grand piano for the Keys part. Tagged Pop."
    juce::String whyItWorks (const Catalog&, const PaletteSlot&);
    juce::StringArray whyItWorksPoints (const Catalog&, const PaletteSlot&);   // the same, as a short list of reasons
}
