#include "FavoritesPage.h"
#include "PluginProcessor.h"
#include "core/Palette.h"
#include "core/Arrangement.h"

namespace ss::ui
{
    //==============================================================================
    // the saved songs, newest first
    class FavoritesPage::SongRows : public juce::ListBoxModel
    {
    public:
        explicit SongRows (FavoritesPage& p) : page (p) {}
        int getNumRows() override   { return (int) page.songs.size(); }
        void selectedRowsChanged (int) override
        {
            page.deleteArmed = false;
            if (page.renaming) page.finishRename (false);
            page.updateButtons();
            page.repaint();
        }
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
        {
            if (juce::isPositiveAndBelow (row, (int) page.songs.size())) page.openSong (page.songs[(size_t) row].file);
        }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (! juce::isPositiveAndBelow (row, (int) page.songs.size())) return;
            const auto& p = pal();
            const auto& s = page.songs[(size_t) row];
            auto r = juce::Rectangle<float> ((float) w, (float) h).reduced (4.0f, 3.0f);
            if (selected)
            {
                g.setColour (p.panelHi);
                g.fillRoundedRectangle (r, 10.0f);
                g.setColour (p.gold.withAlpha (0.8f));
                g.drawRoundedRectangle (r, 10.0f, 1.2f);
            }
            else if (row % 2 == 0)
            {
                g.setColour (p.panelHi.withAlpha (0.3f));
                g.fillRoundedRectangle (r, 10.0f);
            }
            auto in = r.reduced (16.0f, 8.0f);
            auto right = in.removeFromRight (170.0f);
            g.setColour (p.text);
            g.setFont (serif (20.0f));
            g.drawText (s.name, in.removeFromTop (in.getHeight() * 0.55f), juce::Justification::bottomLeft, true);
            juce::StringArray bits;
            if (s.style.isNotEmpty()) bits.add (s.style);
            if (s.key >= 0) bits.add (keyName (s.key));
            if (s.bpm > 0.0) bits.add (juce::String (juce::roundToInt (s.bpm)) + " BPM");
            if (s.sections > 0) bits.add (juce::String (s.sections) + (s.sections == 1 ? " section, " : " sections, ") + juce::String (s.bars) + " bars");
            g.setColour (p.muted);
            g.setFont (sans (13.0f));
            g.drawText (bits.joinIntoString ("  |  "), in, juce::Justification::topLeft, true);

            g.setColour (p.dim);
            g.setFont (sans (12.5f));
            g.drawText (s.saved.formatted ("%b %d, %Y  %H:%M"), right.removeFromTop (right.getHeight() * 0.5f), juce::Justification::bottomRight, true);
            // tags: the song open now, a blank song
            auto tags = right.removeFromTop (20.0f).withTrimmedTop (3.0f);
            auto tag = [&g, &tags] (const juce::String& text, juce::Colour c)
            {
                const auto f = sans (10.5f, true);
                const float tw = juce::GlyphArrangement::getStringWidth (f, text) + 16.0f;
                auto t = tags.removeFromRight (tw);
                tags.removeFromRight (6.0f);
                g.setColour (c.withAlpha (0.18f));
                g.fillRoundedRectangle (t, 8.0f);
                g.setColour (c);
                g.drawRoundedRectangle (t.reduced (0.5f), 8.0f, 1.0f);
                g.setFont (f);
                g.drawText (text, t, juce::Justification::centred, false);
            };
            if (s.name.equalsIgnoreCase (page.proc.song.savedAs)) tag ("OPEN NOW", p.good);
            if (s.blank) tag ("BLANK SONG", p.purple);
        }

    private:
        FavoritesPage& page;
    };

    //==============================================================================
    // the favorite sounds, by kind
    class FavoritesPage::SoundRows : public juce::ListBoxModel
    {
    public:
        explicit SoundRows (FavoritesPage& p) : page (p) {}
        int getNumRows() override   { return (int) page.sounds.size(); }
        void selectedRowsChanged (int) override   { page.updateButtons(); page.repaint(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (! juce::isPositiveAndBelow (row, (int) page.sounds.size())) return;
            const auto& p = pal();
            const auto& s = page.sounds[(size_t) row];
            const bool found = page.catalogSound (s) != nullptr;
            auto r = juce::Rectangle<float> ((float) w, (float) h).reduced (4.0f, 2.0f);
            if (selected)
            {
                g.setColour (p.panelHi);
                g.fillRoundedRectangle (r, 9.0f);
                g.setColour (p.gold.withAlpha (0.8f));
                g.drawRoundedRectangle (r, 9.0f, 1.2f);
            }
            else if (row % 2 == 0)
            {
                g.setColour (p.panelHi.withAlpha (0.3f));
                g.fillRoundedRectangle (r, 9.0f);
            }
            g.setColour (kindColour (s.kind));
            g.fillRoundedRectangle (r.getX() + 8.0f, r.getY() + 9.0f, 4.0f, r.getHeight() - 18.0f, 2.0f);
            auto text = r.withTrimmedLeft (22.0f).withTrimmedRight (10.0f);
            auto right = text.removeFromRight (110.0f);
            g.setColour (found ? p.text : p.dim);
            g.setFont (sans (15.0f, true));
            g.drawText (s.name, text.withTrimmedBottom (text.getHeight() * 0.48f), juce::Justification::bottomLeft, true);
            juce::String sub = s.source == "Plug-in" ? juce::String ("Plug-in") : s.instrument;
            if (s.library.isNotEmpty() && s.library != s.instrument && s.source != "Plug-in") sub << "  |  " << s.library;
            g.setColour (p.muted);
            g.setFont (sans (12.5f));
            g.drawText (sub, text.withTrimmedTop (text.getHeight() * 0.55f), juce::Justification::topLeft, true);
            g.setColour (kindColour (s.kind).withAlpha (0.9f));
            g.setFont (sans (12.0f, true));
            g.drawText (kindName (s.kind).toUpperCase(), right.withTrimmedBottom (right.getHeight() * 0.48f), juce::Justification::bottomRight, true);
            if (! found)
            {
                g.setColour (p.warn);
                g.setFont (sans (11.0f, true));
                g.drawText ("NOT FOUND", right.withTrimmedTop (right.getHeight() * 0.55f), juce::Justification::topRight, true);
            }
        }

    private:
        FavoritesPage& page;
    };

    //==============================================================================
    FavoritesPage::FavoritesPage (SongSavantProcessor& p) : proc (p)
    {
        songRows = std::make_unique<SongRows> (*this);
        soundRows = std::make_unique<SoundRows> (*this);
        songList.setModel (songRows.get());
        soundList.setModel (soundRows.get());
        songList.setRowHeight (64);
        soundList.setRowHeight (48);
        for (auto* l : { &songList, &soundList })
        {
            l->setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
            l->getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, pal().purple);
            addAndMakeVisible (l);
        }

        saveBtn.setTooltip ("Save everything about the song you have now: its sounds, arrangement, chords and notes (Ctrl+S)");
        saveBtn.onClick = [this] { saveSong (false); };
        saveNewBtn.setTooltip ("Save it as a new song, and keep the one it came from as it was");
        saveNewBtn.onClick = [this] { saveSong (true); };
        backupsBtn.setTooltip ("Copies kept before a song was saved over or deleted: bring one back");
        backupsBtn.onClick = [this] { showBackups(); };
        openBtn.setTooltip ("Bring this song back, as it was saved, to keep working on it (or double-click it)");
        openBtn.onClick = [this] { if (auto* s = selectedSong()) openSong (s->file); };
        renameBtn.onClick = [this] { startRename(); };
        deleteBtn.onClick = [this]
        {
            if (! deleteArmed) { deleteArmed = true; updateButtons(); return; }
            deleteSelected();
        };
        useBtn.setTooltip ("Put this sound on a card of your palette (locked there)");
        useBtn.onClick = [this] { showUseMenu(); };
        copyBtn.onClick = [this]
        {
            if (auto* f = selectedFav())
            {
                const auto* s = catalogSound (*f);
                const auto name = s != nullptr ? nameToCopy (*s) : f->name;
                juce::SystemClipboard::copyTextToClipboard (name);
                status = "Copied \"" + name + "\"." + (s != nullptr ? " " + whereToPaste (*s) : juce::String());
                repaint();
            }
        };
        removeBtn.setTooltip ("Take it out of your favorites (the sound stays in your library)");
        removeBtn.onClick = [this]
        {
            if (auto* f = selectedFav())
            {
                status = "\"" + f->name + "\" is out of your favorites.";
                favorites::remove (f->key);
                refreshLists();
            }
        };
        for (auto* b : { &saveBtn, &saveNewBtn, &backupsBtn, &openBtn, &renameBtn, &deleteBtn, &useBtn, &copyBtn, &removeBtn })
        {
            b->setWantsKeyboardFocus (false);
            addAndMakeVisible (b);
        }

        nameEdit.setFont (serif (19.0f));
        nameEdit.setIndents (12, 8);
        nameEdit.onReturnKey = [this] { finishRename (true); };
        nameEdit.onEscapeKey = [this] { finishRename (false); };
        nameEdit.onFocusLost = [this] { if (renaming) finishRename (true); };
        addChildComponent (nameEdit);

        refreshLists();
        startTimer (1500);
    }

    FavoritesPage::~FavoritesPage()
    {
        stopTimer();
        songList.setModel (nullptr);
        soundList.setModel (nullptr);
    }

    void FavoritesPage::visibilityChanged()
    {
        if (isVisible()) refreshLists();
    }

    void FavoritesPage::themeChanged()
    {
        for (auto* l : { &songList, &soundList }) l->getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, pal().purple);
        nameEdit.applyColourToAllText (pal().text, true);
        repaint();
    }

    void FavoritesPage::timerCallback()
    {
        // another window (the app and the plug-in) may have saved a song or hearted a sound
        if (isVisible() && ! renaming && signature() != seen) refreshLists();
    }

    juce::String FavoritesPage::signature() const
    {
        juce::String sig;
        for (auto& f : mysongs::folder().findChildFiles (juce::File::findFiles, false, "*.songsavant"))
            sig << f.getFileName() << ":" << juce::String (f.getLastModificationTime().toMilliseconds()) << ";";
        sig << "|" << juce::String (favorites::file().getLastModificationTime().toMilliseconds()) << "|" << proc.song.savedAs;
        return sig;
    }

    void FavoritesPage::refreshLists()
    {
        // keep the selections on the same song and sound
        const auto songFile = selectedSong() != nullptr ? selectedSong()->file : juce::File();
        const auto soundKey = selectedFav() != nullptr ? selectedFav()->key : juce::String();
        songs = mysongs::list();
        sounds = favorites::sounds();
        seen = signature();
        songList.updateContent();
        soundList.updateContent();
        for (int i = 0; i < (int) songs.size(); ++i) if (songs[(size_t) i].file == songFile) songList.selectRow (i);
        for (int i = 0; i < (int) sounds.size(); ++i) if (sounds[(size_t) i].key == soundKey) soundList.selectRow (i);
        if (songList.getSelectedRow() < 0 && ! songs.empty()) songList.selectRow (0);
        if (soundList.getSelectedRow() < 0 && ! sounds.empty()) soundList.selectRow (0);
        songList.repaint();
        soundList.repaint();
        updateButtons();
        repaint();
    }

    const mysongs::SavedSong* FavoritesPage::selectedSong() const
    {
        const int r = songList.getSelectedRow();
        return juce::isPositiveAndBelow (r, (int) songs.size()) ? &songs[(size_t) r] : nullptr;
    }

    const favorites::FavSound* FavoritesPage::selectedFav() const
    {
        const int r = soundList.getSelectedRow();
        return juce::isPositiveAndBelow (r, (int) sounds.size()) ? &sounds[(size_t) r] : nullptr;
    }

    const Sound* FavoritesPage::catalogSound (const favorites::FavSound& f) const
    {
        const auto cat = proc.catalog.catalog();
        if (cat == nullptr) return nullptr;
        const int i = findSoundByKey (*cat, f.key);
        return i >= 0 ? &cat->sounds[(size_t) i] : nullptr;   // (the catalog lives as long as the processor holds it)
    }

    void FavoritesPage::selectSong (int row)    { songList.selectRow (row); }
    void FavoritesPage::selectSound (int row)   { soundList.selectRow (row); }

    void FavoritesPage::updateButtons()
    {
        const bool song = selectedSong() != nullptr, fav = selectedFav() != nullptr;
        const bool found = fav && catalogSound (*selectedFav()) != nullptr;
        for (auto* b : { &openBtn, &renameBtn, &deleteBtn }) b->setEnabled (song && ! renaming);
        deleteBtn.setButtonText (deleteArmed ? "SURE? DELETE" : "DELETE");
        deleteBtn.setTooltip (deleteArmed ? "Click again to delete it (a copy stays in Backups)" : "Delete this song (click twice; a copy stays in Backups)");
        useBtn.setEnabled (found && proc.song.currentPalette() != nullptr);
        copyBtn.setEnabled (fav);
        removeBtn.setEnabled (fav);
        saveBtn.setButtonText (proc.song.savedAs.isNotEmpty() && mysongs::find (proc.song.savedAs).has_value() ? "SAVE CHANGES" : "SAVE THIS SONG");
        backupsBtn.setEnabled (! mysongs::backups().empty());
    }

    //==============================================================================
    // Saving and opening songs
    juce::String FavoritesPage::saveSong (bool asNew, bool* savedOver)
    {
        auto& song = proc.song;
        if (auto* pal = song.currentPalette(); pal != nullptr && song.arrangement.empty())
            if (const auto cat = proc.catalog.catalog())
                song.updateArrangement (arrangementParts (*pal, cat.get()));   // (ARRANGE makes it the first time it's shown)
        const bool over = ! asNew && song.savedAs.isNotEmpty() && mysongs::find (song.savedAs).has_value();
        const auto name = over ? song.savedAs : mysongs::freeName (asNew && song.savedAs.isNotEmpty() ? song.savedAs : mysongs::suggestedName (song));
        const auto before = song.savedAs;
        song.savedAs = name;                              // (saved inside the song: it knows which one it is)
        const auto file = mysongs::save (song, name);
        if (savedOver != nullptr) *savedOver = over;
        if (file == juce::File())
        {
            song.savedAs = before;
            status = "Couldn't save the song in " + mysongs::folder().getFullPathName();
            repaint();
            return {};
        }
        status = over ? "Saved your changes to \"" + name + "\" (the version before is in BACKUPS)."
                      : "Saved \"" + name + "\". RENAME it to call it something else.";
        refreshLists();
        for (int i = 0; i < (int) songs.size(); ++i) if (songs[(size_t) i].file == file) songList.selectRow (i);
        return name;
    }

    bool FavoritesPage::currentIsSaved() const
    {
        const auto& song = proc.song;
        if (song.form.description.trim().isEmpty() && song.palettes.empty() && ! song.needsRestore()) return true;   // nothing to lose
        if (song.savedAs.isEmpty()) return false;
        const auto saved = mysongs::find (song.savedAs);
        if (! saved.has_value()) return false;
        const auto xml = mysongs::load (saved->file);
        return xml != nullptr && xml->isEquivalentTo (song.toXml().get(), false);
    }

    bool FavoritesPage::openSong (const juce::File& file, bool force)
    {
        auto xml = mysongs::load (file);
        juce::String name = file.getFileNameWithoutExtension();
        for (auto& s : songs) if (s.file == file) name = s.name;
        if (xml == nullptr)
        {
            status = "Couldn't read \"" + name + "\".";
            repaint();
            return false;
        }
        if (! force && ! currentIsSaved())
        {
            juce::PopupMenu m;
            m.addSectionHeader ("THE SONG YOU HAVE NOW ISN'T SAVED");
            m.addItem (1, "Save it, then open \"" + name + "\"");
            m.addItem (2, "Open \"" + name + "\" without saving it");
            m.addItem (3, "Cancel");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&openBtn),
                             [safe = juce::Component::SafePointer<FavoritesPage> (this), file] (int r)
            {
                if (safe == nullptr) return;
                if (r == 1 && safe->saveSong (false).isNotEmpty()) safe->openSong (file, true);
                else if (r == 2) safe->openSong (file, true);
            });
            return false;
        }
        proc.stopSketch();
        proc.song.fromXml (*xml);
        proc.song.savedAs = name;
        if (const auto cat = proc.catalog.catalog()) proc.song.restore (*cat);
        status = "Opened \"" + name + "\".";
        refreshLists();
        if (onOpened) onOpened();
        return true;
    }

    void FavoritesPage::startRename()
    {
        auto* s = selectedSong();
        if (s == nullptr) return;
        renaming = true;
        deleteArmed = false;
        nameEdit.setText (s->name, false);
        nameEdit.setVisible (true);
        resized();
        nameEdit.grabKeyboardFocus();
        nameEdit.selectAll();
        updateButtons();
    }

    void FavoritesPage::finishRename (bool keep)
    {
        if (! renaming) return;
        renaming = false;
        nameEdit.setVisible (false);
        if (keep) renameSelected (nameEdit.getText());
        updateButtons();
        resized();
        repaint();
    }

    bool FavoritesPage::renameSelected (const juce::String& newName)
    {
        auto* s = selectedSong();
        const auto name = newName.trim();
        if (s == nullptr || name.isEmpty() || name == s->name) return false;
        const auto oldName = s->name;
        const auto renamed = mysongs::rename (s->file, name);
        if (renamed == juce::File())
        {
            status = "There's already a song called \"" + name + "\".";
            repaint();
            return false;
        }
        if (proc.song.savedAs.equalsIgnoreCase (oldName)) proc.song.savedAs = name;   // the song open now
        status = "Renamed \"" + oldName + "\" to \"" + name + "\".";
        refreshLists();
        for (int i = 0; i < (int) songs.size(); ++i) if (songs[(size_t) i].file == renamed) songList.selectRow (i);
        return true;
    }

    bool FavoritesPage::deleteSelected()
    {
        auto* s = selectedSong();
        deleteArmed = false;
        if (s == nullptr) return false;
        const auto name = s->name;
        if (! mysongs::remove (s->file)) return false;
        // (the song you have now stays as it is: it just isn't saved anywhere any more)
        if (proc.song.savedAs.equalsIgnoreCase (name)) proc.song.savedAs.clear();
        status = "Deleted \"" + name + "\". A copy is in BACKUPS.";
        refreshLists();
        return true;
    }

    void FavoritesPage::showBackups()
    {
        const auto all = mysongs::backups();
        juce::PopupMenu m;
        m.addSectionHeader ("BRING ONE BACK (AS A SONG OF ITS OWN)");
        for (int i = 0; i < (int) all.size() && i < 40; ++i)
        {
            const auto& b = all[(size_t) i];
            m.addItem (i + 1, b.name + "   (" + b.why + ", " + b.when.formatted ("%b %d %H:%M") + ")");
        }
        if (all.empty()) m.addItem (-1, "No backups yet", false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&backupsBtn),
                         [safe = juce::Component::SafePointer<FavoritesPage> (this), all] (int r)
        {
            if (safe == nullptr || r <= 0 || r > (int) all.size()) return;
            const auto f = mysongs::restore (all[(size_t) r - 1]);
            safe->status = f.existsAsFile() ? "Brought back \"" + all[(size_t) r - 1].name + "\" (as its own song)." : "Couldn't bring it back.";
            safe->refreshLists();
            for (int i = 0; i < (int) safe->songs.size(); ++i) if (safe->songs[(size_t) i].file == f) safe->songList.selectRow (i);
        });
    }

    //==============================================================================
    // Using a favorite sound
    void FavoritesPage::showUseMenu()
    {
        auto* f = selectedFav();
        auto* pal = proc.song.currentPalette();
        const auto cat = proc.catalog.catalog();
        const auto* snd = f != nullptr ? catalogSound (*f) : nullptr;
        if (snd == nullptr || pal == nullptr || cat == nullptr) return;
        // the cards it fits first, then the rest
        juce::StringArray roles;
        std::vector<bool> fit;
        const auto brief = proc.song.makeBrief();
        for (auto& slot : pal->slots)
        {
            roles.add (slot.part.role);
            fit.push_back (scoreSound (*snd, slot.part, brief) > 0.0f);
        }
        juce::PopupMenu flat;
        flat.addSectionHeader ("PUT \"" + f->name + "\" ON");
        for (int i = 0; i < roles.size(); ++i)
            if (fit[(size_t) i]) flat.addItem (i + 1, "The " + roles[i] + " card");
        bool header = false;
        for (int i = 0; i < roles.size(); ++i)
            if (! fit[(size_t) i])
            {
                if (! header) { flat.addSectionHeader ("Other cards"); header = true; }
                flat.addItem (i + 1, "The " + roles[i] + " card");
            }
        flat.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&useBtn),
                            [safe = juce::Component::SafePointer<FavoritesPage> (this), roles] (int r)
        {
            if (safe != nullptr && r > 0 && r <= roles.size()) safe->useSelectedSound (roles[r - 1]);
        });
    }

    bool FavoritesPage::useSelectedSound (const juce::String& role)
    {
        auto* f = selectedFav();
        const auto cat = proc.catalog.catalog();
        if (f == nullptr || cat == nullptr) return false;
        const int idx = findSoundByKey (*cat, f->key);
        if (idx < 0 || ! proc.song.putSound (*cat, idx, role)) return false;
        status = "\"" + f->name + "\" is on the " + role + " card (locked).";
        repaint();
        if (onUsedSound) onUsedSound (role);
        return true;
    }

    //==============================================================================
    void FavoritesPage::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        drawPanel (g, songsPanel);
        drawPanel (g, soundsPanel);

        auto title = [&g, &p] (juce::Rectangle<float> r, juce::Rectangle<float> subArea, const juce::String& t, const juce::String& sub)
        {
            g.setColour (p.text);
            g.setFont (serif (25.0f));
            g.drawText (t, r, juce::Justification::centredLeft, true);
            g.setColour (p.muted);
            g.setFont (sans (13.0f));
            g.drawText (sub, subArea, juce::Justification::topLeft, true);
        };
        title (songsTitle, songsSub, "My songs", "Everything about a song: its sounds, arrangement, chords and notes. Open one to keep working on it.");
        title (soundsTitle, soundsSub, "My sounds", "The sounds you hearted on a card or on the LIBRARY page.");

        g.setColour (p.dim);
        g.setFont (sans (15.0f));
        if (songs.empty())
            g.drawFittedText ("No saved songs yet.\nSAVE THIS SONG keeps the song you have now (or press Ctrl+S on any page).",
                              songsEmpty.toNearestInt(), juce::Justification::centred, 3);
        if (sounds.empty())
            g.drawFittedText ("No favorite sounds yet.\nClick the heart on a card (SONG) or on a sound (LIBRARY).",
                              soundsEmpty.toNearestInt(), juce::Justification::centred, 3);

        // how to load the selected sound
        if (auto* f = selectedFav())
        {
            auto r = howArea;
            const auto* s = catalogSound (*f);
            g.setColour (s != nullptr ? p.gold : p.warn);
            g.setFont (sans (12.5f, true));
            g.drawText (s != nullptr ? "HOW TO LOAD IT" : "NOT IN YOUR LIBRARY RIGHT NOW", r.removeFromTop (20.0f), juce::Justification::centredLeft);
            drawParagraph (g, s != nullptr ? howToLoad (*s)
                                           : "Song Savant can't find \"" + f->name + "\" in your libraries today (an instrument or library missing?). "
                                             "It comes back here when it's found again.", r, sans (13.5f), p.muted);
        }

        if (status.isNotEmpty())
        {
            g.setColour (p.good);
            g.setFont (sans (13.5f, true));
            g.drawFittedText (status, statusArea.toNearestInt(), juce::Justification::centredLeft, 2);
        }
    }

    void FavoritesPage::resized()
    {
        auto r = getLocalBounds().toFloat().reduced (24.0f, 0.0f).withTrimmedBottom (16.0f);
        const float gap = 18.0f;
        songsPanel = r.removeFromLeft (r.getWidth() * 0.56f);
        r.removeFromLeft (gap);
        soundsPanel = r;

        {
            auto in = songsPanel.reduced (22.0f, 18.0f);
            auto top = in.removeFromTop (58.0f);
            auto titleRow = top.removeFromTop (36.0f);
            songsSub = top;
            const float bw = juce::jmin (440.0f, titleRow.getWidth() * 0.72f);   // (worked out before the row is cut)
            auto buttons = titleRow.removeFromRight (bw).withSizeKeepingCentre (bw, 34.0f);
            backupsBtn.setBounds (buttons.removeFromRight (96.0f).toNearestInt());
            buttons.removeFromRight (8.0f);
            saveNewBtn.setBounds (buttons.removeFromRight (126.0f).toNearestInt());
            buttons.removeFromRight (8.0f);
            saveBtn.setBounds (buttons.toNearestInt());
            songsTitle = titleRow.withTrimmedRight (12.0f);
            in.removeFromTop (10.0f);
            statusArea = in.removeFromBottom (36.0f);
            auto actions = in.removeFromBottom (44.0f);
            in.removeFromBottom (10.0f);
            songList.setBounds (in.toNearestInt());
            songsEmpty = in;
            auto a = actions;
            openBtn.setBounds (a.removeFromLeft (130.0f).toNearestInt());
            a.removeFromLeft (10.0f);
            renameBtn.setBounds (a.removeFromLeft (120.0f).toNearestInt());
            a.removeFromLeft (10.0f);
            deleteBtn.setBounds (a.removeFromLeft (150.0f).toNearestInt());
            nameEdit.setBounds (actions.withTrimmedLeft (140.0f).withTrimmedRight (juce::jmax (0.0f, actions.getWidth() - 140.0f - 380.0f)).toNearestInt());
            if (renaming) { renameBtn.setVisible (false); deleteBtn.setVisible (false); }
            else { renameBtn.setVisible (true); deleteBtn.setVisible (true); }
        }
        {
            auto in = soundsPanel.reduced (22.0f, 18.0f);
            auto top = in.removeFromTop (58.0f);
            soundsTitle = top.removeFromTop (36.0f);
            soundsSub = top;
            in.removeFromTop (10.0f);
            auto actions = in.removeFromBottom (44.0f);
            in.removeFromBottom (8.0f);
            howArea = in.removeFromBottom (juce::jmin (110.0f, in.getHeight() * 0.32f));
            in.removeFromBottom (8.0f);
            soundList.setBounds (in.toNearestInt());
            soundsEmpty = in;
            auto a = actions;
            useBtn.setBounds (a.removeFromLeft (juce::jmin (120.0f, a.getWidth() * 0.34f)).toNearestInt());
            a.removeFromLeft (8.0f);
            copyBtn.setBounds (a.removeFromLeft (juce::jmin (130.0f, a.getWidth() * 0.52f)).toNearestInt());
            a.removeFromLeft (8.0f);
            removeBtn.setBounds (a.removeFromLeft (juce::jmin (110.0f, a.getWidth())).toNearestInt());
        }
    }
}
