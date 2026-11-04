// Song Savant - FAVORITES: the sounds you hearted, and the songs you saved, to use and change later.
//
// Favorite sounds: the heart on a SONG card or a LIBRARY sound. Kept with what Song Savant knew about each sound (its
// name, instrument, library, kind), so the list still reads right when a library isn't there. They only sit in your
// list until you pick one (USE IT puts it on a card): palettes don't favour them. One file, Documents/Song Savant/
// Favorite sounds.xml, read again when it changed (the app and the plug-in can both be open).
//
// Saved songs: SAVE SONG keeps everything about a song (description, key, tempo, palettes with your locks and
// choices, the arrangement, your own notes and chords, the blank song) as one file in Documents/Song Savant/My Songs
// (<name>.songsavant). OPEN brings it all back. Before a song is saved over or deleted, a copy goes to My Songs/Backups
// (the newest 200 are kept), and a backup comes back as a song of its own.
#pragma once

#include "Catalog.h"

namespace ss
{
    class SongSession;

    namespace favorites
    {
        struct FavSound
        {
            juce::String key;                         // soundKey: name|instrument|library
            juce::String name, instrument, library, source;
            Kind kind = Kind::other;
            juce::Time added;
        };

        std::vector<FavSound> sounds();               // by kind, then name
        bool isFavorite (const Sound&);
        bool isFavorite (const juce::String& key);
        void add (const Sound&);
        void remove (const juce::String& key);
        bool toggle (const Sound&);                   // true: it's a favorite now
        juce::File file();                            // Documents/Song Savant/Favorite sounds.xml
        void reloadForTests();                        // forget what's in memory and read the file again
    }

    namespace mysongs
    {
        struct SavedSong
        {
            juce::File file;
            juce::String name;
            juce::String style;                       // "Trap / Drill", "Modern Country"
            juce::String description;
            int key = -1;                             // 0..23
            double bpm = 0.0;
            int sections = 0, bars = 0;
            bool blank = false;                       // a blank song (your own sections and notes)
            juce::Time saved;
        };

        juce::File folder();                          // Documents/Song Savant/My Songs
        juce::File backupFolder();                    // My Songs/Backups
        std::vector<SavedSong> list();                // newest first
        std::optional<SavedSong> find (const juce::String& name);
        juce::String freeName (const juce::String& wanted);       // "Back Roads" or "Back Roads 2"...
        juce::String suggestedName (const SongSession&);          // the Key Compass song's name, or the description's first words

        // Saves the song under `name` (a copy of the one it replaces goes to Backups first). Returns the file, or none.
        juce::File save (const SongSession&, const juce::String& name);
        std::unique_ptr<juce::XmlElement> load (const juce::File&);   // the song (a "Song" element), or null
        juce::File rename (const juce::File&, const juce::String& newName);   // the renamed file (none if the name is taken)
        bool remove (const juce::File&);                              // (a copy goes to Backups)

        struct Backup { juce::File file; juce::String name, why; juce::Time when; };
        std::vector<Backup> backups();                // newest first
        juce::File restore (const Backup&);           // a song of its own again ("Name (restored)"); the file
    }
}
