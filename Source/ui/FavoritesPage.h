// Song Savant - the FAVORITES page: your saved songs, and the sounds you hearted.
//
// MY SONGS (left): SAVE THIS SONG (over the saved song it came from, or a new one) and SAVE AS NEW; each song with its
// style, key, tempo and size; OPEN brings it all back (it asks first when the song you have now isn't saved), RENAME,
// DELETE (click twice; a copy stays in Backups), BACKUPS (bring one back). MY SOUNDS (right): the sounds you hearted on
// a SONG card or on the LIBRARY page, by kind; USE IT puts one on a card of your palette (locked), COPY NAME, how to
// load it, REMOVE. Favorites never steer the palettes: they wait here until you pick one.
#pragma once

#include "Widgets.h"
#include "core/Favorites.h"

class SongSavantProcessor;

namespace ss::ui
{
    class FavoritesPage : public juce::Component, private juce::Timer
    {
    public:
        explicit FavoritesPage (SongSavantProcessor&);
        ~FavoritesPage() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void visibilityChanged() override;
        void themeChanged();

        // SAVE THIS SONG (asNew false: over the saved song it was opened from or saved as) or SAVE AS NEW. The name it
        // was saved under ("" when it couldn't be saved); `savedOver` says whether an older version was replaced.
        juce::String saveSong (bool asNew, bool* savedOver = nullptr);
        // OPEN: the saved song comes back as it was. When the song you have now isn't saved it asks first (force: don't).
        bool openSong (const juce::File&, bool force = false);
        bool currentIsSaved() const;                    // nothing to lose by opening another
        std::function<void()> onOpened;                 // another song now: the other pages show it
        std::function<void (const juce::String& role)> onUsedSound;   // USE IT put a favorite on that card

        // (the tests)
        void refreshLists();
        int numSongs() const                            { return (int) songs.size(); }
        int numSounds() const                           { return (int) sounds.size(); }
        void selectSong (int row);
        void selectSound (int row);
        bool useSelectedSound (const juce::String& role);
        bool renameSelected (const juce::String& name);
        bool deleteSelected();
        const juce::String& getStatus() const           { return status; }

    private:
        class SongRows;
        class SoundRows;
        void timerCallback() override;
        void updateButtons();
        void showUseMenu();
        void showBackups();
        void startRename();
        void finishRename (bool keep);
        const mysongs::SavedSong* selectedSong() const;
        const favorites::FavSound* selectedFav() const;
        const Sound* catalogSound (const favorites::FavSound&) const;   // the favorite in today's library (null: not found)
        juce::String signature() const;                 // what's in the folders (to notice another window's changes)

        SongSavantProcessor& proc;
        std::vector<mysongs::SavedSong> songs;
        std::vector<favorites::FavSound> sounds;
        juce::String seen, status;
        std::unique_ptr<SongRows> songRows;
        std::unique_ptr<SoundRows> soundRows;
        juce::ListBox songList, soundList;
        juce::TextEditor nameEdit;
        bool renaming = false, deleteArmed = false;
        PillButton saveBtn { "SAVE THIS SONG", PillButton::Style::gold },
                   saveNewBtn { "SAVE AS NEW", PillButton::Style::outline },
                   backupsBtn { "BACKUPS", PillButton::Style::subtle },
                   openBtn { "OPEN", PillButton::Style::gold },
                   renameBtn { "RENAME", PillButton::Style::outline },
                   deleteBtn { "DELETE", PillButton::Style::outline },
                   useBtn { "USE IT", PillButton::Style::gold },
                   copyBtn { "COPY NAME", PillButton::Style::outline },
                   removeBtn { "REMOVE", PillButton::Style::subtle };
        juce::Rectangle<float> songsPanel, soundsPanel, songsTitle, soundsTitle, songsSub, soundsSub, songsEmpty, soundsEmpty, howArea, statusArea;
    };
}
