#include "SongPage.h"
#include "PluginProcessor.h"
#include "core/Siblings.h"

namespace ss::ui
{
    //==============================================================================
    void PartCard::show (const PaletteSlot* s, const Sound* snd, bool sel)
    {
        slot = s;
        sound = snd;
        isSelected = sel;
        repaint();
    }

    void PartCard::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const bool over = isMouseOver (true);
        juce::Path box;
        box.addRoundedRectangle (r, 14.0f);
        if (isSelected) juce::DropShadow (p.gold.withAlpha (0.35f), 14, {}).drawForPath (g, box);
        g.setGradientFill (juce::ColourGradient (p.panelHi.withAlpha (over ? 1.0f : 0.9f), r.getX(), r.getY(), p.panel, r.getX(), r.getBottom(), false));
        g.fillPath (box);
        g.setColour (isSelected ? p.gold : p.blue.withAlpha (0.45f));
        g.strokePath (box, juce::PathStrokeType (isSelected ? 1.8f : 1.0f));
        if (slot == nullptr) return;

        auto in = r.reduced (12.0f, 10.0f);
        g.setColour (p.text);
        g.setFont (sans (14.5f, true));
        auto title = in.removeFromTop (20.0f);
        g.drawText (slot->part.role.toUpperCase(), title, juce::Justification::centred, true);
        g.setColour (p.muted);
        g.setFont (sans (11.5f));
        g.drawFittedText (slot->part.hint, in.removeFromTop (30.0f).toNearestInt(), juce::Justification::centredTop, 2, 0.9f);
        if (slot->part.optional)
        {
            g.setColour (p.purple.brighter (0.4f));
            g.setFont (sans (10.0f, true));
            g.drawText ("EXTRA", r.reduced (8.0f).removeFromTop (12.0f), juce::Justification::topLeft);
        }

        // the art: the kind of sound in its colour
        auto art = in.removeFromTop (juce::jmin (in.getHeight() * 0.42f, in.getWidth() * 0.72f));
        const auto colour = sound != nullptr ? kindColour (sound->kind) : p.dim;
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.38f), art.getCentreX(), art.getY(), p.bgTop.withAlpha (0.6f), art.getCentreX(), art.getBottom(), false));
        g.fillRoundedRectangle (art, 10.0f);
        if (sound != nullptr) drawKindIcon (g, sound->kind, art.reduced (art.getHeight() * 0.2f), colour.brighter (0.4f));

        lockArea = art.removeFromTop (30.0f).removeFromRight (30.0f).reduced (3.0f);
        if (slot->pick.isValid())
        {
            g.setColour (slot->locked ? p.gold : p.text.withAlpha (0.55f));
            auto lk = lockArea.reduced (5.0f);
            juce::Path shackle;
            shackle.addCentredArc (lk.getCentreX(), lk.getY() + lk.getHeight() * 0.42f, lk.getWidth() * 0.3f, lk.getHeight() * 0.32f, 0.0f,
                                   -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
            g.strokePath (shackle, juce::PathStrokeType (1.6f));
            g.fillRoundedRectangle (lk.withTrimmedTop (lk.getHeight() * 0.45f), 2.0f);
        }

        in.removeFromTop (8.0f);
        auto buttons = in.removeFromBottom (26.0f);
        if (sound != nullptr)
        {
            g.setColour (p.text);
            g.setFont (sans (14.5f, true));
            g.drawFittedText (sound->name, in.removeFromTop (38.0f).toNearestInt(), juce::Justification::centred, 2, 0.85f);
            g.setColour (p.gold);
            g.setFont (sans (12.0f));
            g.drawFittedText (sound->instrument, in.removeFromTop (18.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
        }
        else
        {
            g.setColour (p.dim);
            g.setFont (sans (12.5f));
            g.drawFittedText ("Nothing in your library fits this part yet", in.toNearestInt(), juce::Justification::centred, 3);
        }

        prevArea = nextArea = {};
        if (slot->numOptions() > 1)
        {
            prevArea = buttons.removeFromLeft (26.0f);
            nextArea = buttons.removeFromRight (26.0f);
            for (auto [a, left] : { std::pair { prevArea, true }, std::pair { nextArea, false } })
            {
                g.setColour (p.panelHi);
                g.fillEllipse (a);
                g.setColour (p.gold);
                juce::Path arrow;
                const float cx = a.getCentreX(), cy = a.getCentreY(), s = 4.5f;
                arrow.addTriangle (left ? cx + s * 0.6f : cx - s * 0.6f, cy - s, left ? cx + s * 0.6f : cx - s * 0.6f, cy + s, left ? cx - s * 0.8f : cx + s * 0.8f, cy);
                g.fillPath (arrow);
            }
            g.setColour (p.dim);
            g.setFont (sans (11.5f));
            g.drawText (juce::String (slot->choice + 1) + " of " + juce::String (slot->numOptions()), buttons, juce::Justification::centred);
        }
    }

    void PartCard::mouseUp (const juce::MouseEvent& e)
    {
        const auto pos = e.position;
        if (prevArea.contains (pos)) { if (onPrev) onPrev(); }
        else if (nextArea.contains (pos)) { if (onNext) onNext(); }
        else if (lockArea.contains (pos)) { if (onLock) onLock(); }
        if (onSelect) onSelect();
    }

    //==============================================================================
    void ChoiceList::show (const PaletteSlot* s, const Catalog* c)
    {
        slot = s;
        catalog = c;
        repaint();
    }

    void ChoiceList::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        if (slot == nullptr || catalog == nullptr) return;
        for (int i = 0; i < slot->numOptions() && (i + 1) * rowH <= getHeight(); ++i)
        {
            const auto& pick = i == 0 ? slot->pick : slot->alternatives[(size_t) i - 1];
            if (! pick.isValid()) continue;
            const auto& s = catalog->sounds[(size_t) pick.sound];
            auto r = juce::Rectangle<float> (0.0f, (float) (i * rowH), (float) getWidth(), (float) rowH).reduced (0.0f, 1.5f);
            if (i == slot->choice)
            {
                g.setColour (p.purple.withAlpha (0.3f));
                g.fillRoundedRectangle (r, 6.0f);
            }
            g.setColour (kindColour (s.kind));
            g.fillEllipse (r.getX() + 6.0f, r.getCentreY() - 3.5f, 7.0f, 7.0f);
            g.setColour (i == slot->choice ? p.text : p.muted);
            g.setFont (sans (13.5f, i == slot->choice));
            g.drawText (s.name, r.withTrimmedLeft (20.0f).withTrimmedRight (r.getWidth() * 0.42f), juce::Justification::centredLeft, true);
            g.setColour (p.dim);
            g.setFont (sans (12.0f));
            g.drawText (s.instrument, r.withTrimmedLeft (r.getWidth() * 0.6f), juce::Justification::centredRight, true);
        }
    }

    void ChoiceList::mouseUp (const juce::MouseEvent& e)
    {
        const int row = e.y / rowH;
        if (slot != nullptr && row >= 0 && row < slot->numOptions() && onChoose) onChoose (row);
    }

    //==============================================================================
    SongPage::SongPage (SongSavantProcessor& p) : proc (p)
    {
        describeBox.setTextToShowWhenEmpty ("Describe your song: modern country like Morgan Wallen, warm, 120 BPM in A minor", pal().dim);
        describeBox.setFont (sans (16.5f));
        describeBox.setIndents (14, 12);
        describeBox.onReturnKey = [this] { find(); };
        addAndMakeVisible (describeBox);

        genreBox.addItem ("Genre: from my words", 1);
        for (int g = 0; g < numGenres; ++g) genreBox.addItem (genreName ((Genre) g), g + 2);
        genreBox.onChange = [this] { fillStyles (genreBox.getSelectedId() - 2); };
        addAndMakeVisible (genreBox);
        addAndMakeVisible (styleBox);

        keyBox.addItem ("Key: from my words", 1);
        for (int k = 0; k < 24; ++k) keyBox.addItem (keyName (k), k + 2);
        addAndMakeVisible (keyBox);

        bpmBox.setTextToShowWhenEmpty ("BPM", pal().dim);
        bpmBox.setInputRestrictions (3, "0123456789");
        bpmBox.setFont (sans (15.0f));
        bpmBox.setJustification (juce::Justification::centred);
        addAndMakeVisible (bpmBox);

        keyCompassBtn.setTooltip ("Take the key and tempo from a Key Compass song");
        keyCompassBtn.onClick = [this] { showKeyCompassMenu(); };
        addAndMakeVisible (keyCompassBtn);

        findBtn.onClick = [this] { find(); };
        addAndMakeVisible (findBtn);

        for (int i = 0; i < 3; ++i)
        {
            auto* t = tabs.add (new PillButton ("PALETTE " + juce::String (i + 1), PillButton::Style::subtle));
            t->setComponentID ("tab");
            t->onClick = [this, i] { proc.song.current = i; refresh(); };
            addAndMakeVisible (t);
        }
        newBtn.setTooltip ("New takes on your song. Locked parts stay.");
        newBtn.onClick = [this]
        {
            if (catalog == nullptr || proc.song.palettes.empty()) return;
            proc.song.newPalettes (*catalog);
            refresh();
        };
        addAndMakeVisible (newBtn);

        choices.onChoose = [this] (int i)
        {
            if (auto* s = selectedSlot())
            {
                s->choice = i;
                if (s->current().isValid() && catalog != nullptr) s->keepKey = soundKey (catalog->sounds[(size_t) s->current().sound]);
                copiedNote.clear();
                refresh();
            }
        };
        addAndMakeVisible (choices);
        copyBtn.onClick = [this] { copySelected(); };
        addAndMakeVisible (copyBtn);

        // the form as it was saved
        const auto& f = proc.song.form;
        describeBox.setText (f.description, false);
        genreBox.setSelectedId (f.genre >= 0 ? f.genre + 2 : 1, juce::dontSendNotification);
        fillStyles (f.genre);
        keyBox.setSelectedId (f.key >= 0 ? f.key + 2 : 1, juce::dontSendNotification);
        if (f.bpm > 0) bpmBox.setText (juce::String (juce::roundToInt (f.bpm)), false);

        timerCallback();
        startTimerHz (6);
    }

    SongPage::~SongPage()   { stopTimer(); }

    void SongPage::fillStyles (int genre)
    {
        styleBox.clear (juce::dontSendNotification);
        styleBox.addItem ("Style: any", 1);
        if (genre >= 0 && genre < numGenres)
        {
            int id = 2;
            for (auto* s : stylesFor ((Genre) genre))
                if (! s->isGeneral()) styleBox.addItem (s->name, id++);
        }
        styleBox.setSelectedId (1, juce::dontSendNotification);
        if (genre >= 0 && genre < numGenres)
            for (int i = 0; i < styleBox.getNumItems(); ++i)
                for (auto* s : stylesFor ((Genre) genre))
                    if (s->id == proc.song.form.styleId && s->name == styleBox.getItemText (i))
                        styleBox.setSelectedItemIndex (i, juce::dontSendNotification);
        styleBox.setEnabled (genre >= 0);
    }

    void SongPage::readForm()
    {
        auto& f = proc.song.form;
        f.description = describeBox.getText().trim();
        f.genre = genreBox.getSelectedId() - 2;
        f.styleId.clear();
        if (f.genre >= 0 && styleBox.getSelectedId() > 1)
            for (auto* s : stylesFor ((Genre) f.genre))
                if (s->name == styleBox.getText()) f.styleId = s->id;
        f.key = keyBox.getSelectedId() - 2;
        f.bpm = bpmBox.getText().getIntValue();
    }

    void SongPage::find()
    {
        readForm();
        if (catalog == nullptr)
        {
            status = proc.catalog.isBuilding() ? "Still reading your libraries: try again in a moment." : "Your libraries haven't been read yet.";
            repaint();
            return;
        }
        proc.song.find (*catalog);
        selectedRole.clear();
        if (auto* p = proc.song.currentPalette())
            for (auto& s : p->slots)
                if (s.pick.isValid()) { selectedRole = s.part.role; break; }
        status.clear();
        copiedNote.clear();
        refresh();
    }

    void SongPage::setDescription (const juce::String& t)   { describeBox.setText (t, false); }

    void SongPage::timerCallback()
    {
        if (proc.catalog.generation() != seenGeneration)
        {
            seenGeneration = proc.catalog.generation();
            catalog = proc.catalog.catalog();
            if (catalog != nullptr && proc.song.needsRestore()) proc.song.restore (*catalog);
            refresh();
        }
    }

    void SongPage::showKeyCompassMenu()
    {
        const auto songs = checkSiblingApps (proc.locationProvider()).songs;
        juce::PopupMenu m;
        m.addSectionHeader ("KEY COMPASS SONGS");
        if (songs.empty()) m.addItem (-1, "No Key Compass songs found", false);
        for (size_t i = 0; i < songs.size() && i < 20; ++i)
        {
            const auto& s = songs[i];
            m.addItem ((int) i + 1, s.name + "   " + keyName (s.key) + (s.bpm > 0 ? ", " + juce::String (juce::roundToInt (s.bpm)) + " BPM" : juce::String()));
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keyCompassBtn),
                         [this, songs, safe = juce::Component::SafePointer<SongPage> (this)] (int r)
        {
            if (safe == nullptr || r <= 0 || r > (int) songs.size()) return;
            const auto& s = songs[(size_t) r - 1];
            if (s.key >= 0) keyBox.setSelectedId (s.key + 2, juce::dontSendNotification);
            if (s.bpm > 0) bpmBox.setText (juce::String (juce::roundToInt (s.bpm)), false);
            proc.song.form.fromSong = s.name;
            repaint();
        });
    }

    PaletteSlot* SongPage::selectedSlot()
    {
        if (auto* p = proc.song.currentPalette())
            for (auto& s : p->slots)
                if (s.part.role == selectedRole) return &s;
        return nullptr;
    }

    void SongPage::copySelected()
    {
        auto* s = selectedSlot();
        if (s == nullptr || catalog == nullptr || ! s->current().isValid()) return;
        const auto& snd = catalog->sounds[(size_t) s->current().sound];
        juce::SystemClipboard::copyTextToClipboard (snd.name);
        copiedNote = "Copied \"" + snd.name + "\". " + whereToPaste (snd);
        repaint();
    }

    void SongPage::refresh()
    {
        auto* pal = proc.song.currentPalette();
        for (int i = 0; i < tabs.size(); ++i)
        {
            tabs[i]->setVisible (i < (int) proc.song.palettes.size());
            tabs[i]->setToggleState (i == proc.song.current, juce::dontSendNotification);
        }
        newBtn.setEnabled (pal != nullptr);

        const int n = pal != nullptr ? (int) pal->slots.size() : 0;
        while (cards.size() > n) cards.removeLast();
        while (cards.size() < n) addAndMakeVisible (cards.add (new PartCard()));
        if (pal != nullptr && selectedSlot() == nullptr && ! pal->slots.empty()) selectedRole = pal->slots.front().part.role;
        for (int i = 0; i < n; ++i)
        {
            auto& slot = pal->slots[(size_t) i];
            const Sound* snd = catalog != nullptr && slot.current().isValid() && slot.current().sound < (int) catalog->sounds.size()
                                 ? &catalog->sounds[(size_t) slot.current().sound] : nullptr;
            auto* card = cards[i];
            const auto role = slot.part.role;
            card->show (&slot, snd, role == selectedRole);
            card->onSelect = [this, role] { if (selectedRole != role) { selectedRole = role; copiedNote.clear(); } refresh(); };
            card->onPrev = [this, i] { if (auto* p = proc.song.currentPalette()) { auto& s = p->slots[(size_t) i]; s.cycle (-1); if (catalog != nullptr && s.current().isValid()) s.keepKey = soundKey (catalog->sounds[(size_t) s.current().sound]); } };
            card->onNext = [this, i] { if (auto* p = proc.song.currentPalette()) { auto& s = p->slots[(size_t) i]; s.cycle (1); if (catalog != nullptr && s.current().isValid()) s.keepKey = soundKey (catalog->sounds[(size_t) s.current().sound]); } };
            card->onLock = [this, i] { if (auto* p = proc.song.currentPalette()) { auto& s = p->slots[(size_t) i]; if (s.current().isValid()) s.locked = ! s.locked; } };
        }
        auto* sel = selectedSlot();
        choices.show (sel, catalog.get());
        copyBtn.setVisible (sel != nullptr);
        copyBtn.setEnabled (sel != nullptr && sel->current().isValid());
        resized();
        repaint();
    }

    //==============================================================================
    void SongPage::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        drawPanel (g, formPanel);
        drawPanel (g, detailsPanel);

        // what it understood
        {
            juce::String u;
            const auto& b = proc.song.brief;
            if (! status.isEmpty()) u = status;
            else if (! proc.song.palettes.empty())
            {
                juce::StringArray bits;
                bits.add (genreName (b.genre));
                if (auto* s = b.style()) if (! s->isGeneral()) bits.add (s->name);
                for (auto& w : b.understood) if (! bits.contains (w)) bits.add (w);
                if (b.bpm > 0 && ! bits.contains (juce::String (juce::roundToInt (b.bpm)) + " BPM")) bits.add (juce::String (juce::roundToInt (b.bpm)) + " BPM");
                if (b.key >= 0 && ! bits.contains (keyName (b.key))) bits.add (keyName (b.key));
                u = "Your song: " + bits.joinIntoString ("  |  ");
            }
            else u = "Name a genre, a feeling, an artist or a song. Song Savant picks sounds you already own.";
            if (proc.song.form.fromSong.isNotEmpty()) u << "     (key and tempo from Key Compass: " << proc.song.form.fromSong << ")";
            g.setColour (status.isNotEmpty() ? p.warn : proc.song.palettes.empty() ? p.dim : p.gold);
            g.setFont (sans (14.0f, ! proc.song.palettes.empty()));
            g.drawText (u, understoodLine, juce::Justification::centredLeft, true);
        }

        auto* pal = proc.song.currentPalette();
        if (pal == nullptr)
        {
            g.setColour (p.dim);
            g.setFont (serif (22.0f));
            g.drawFittedText (catalog == nullptr ? "Reading your libraries..." : "Describe your song above and click FIND MY SOUNDS.",
                              cardsArea.toNearestInt(), juce::Justification::centred, 2);
            g.setFont (sans (14.0f));
            g.drawFittedText ("You'll get a card for each part (drums, bass, rhythm, lead, pad...) with a sound from your own library, plus alternatives.",
                              detailsPanel.reduced (30.0f).toNearestInt(), juce::Justification::centred, 3);
            return;
        }
        g.setColour (p.dim);
        g.setFont (sans (12.5f));
        g.drawText ("Click a card for details. Arrows switch sounds; the lock keeps a part when you ask for new palettes.", barHint, juce::Justification::centredRight, true);

        auto* sel = selectedSlot();
        if (sel == nullptr) return;
        const Sound* snd = catalog != nullptr && sel->current().isValid() ? &catalog->sounds[(size_t) sel->current().sound] : nullptr;

        auto why = whyArea;
        g.setColour (p.gold);
        g.setFont (sans (12.5f, true));
        g.drawText (sel->part.role.toUpperCase() + (sel->part.optional ? "  (EXTRA)" : juce::String()), why.removeFromTop (18.0f), juce::Justification::centredLeft);
        g.setColour (p.text);
        g.setFont (serif (24.0f));
        g.drawText (snd != nullptr ? snd->name : juce::String ("Nothing fits yet"), why.removeFromTop (32.0f), juce::Justification::centredLeft, true);
        if (snd != nullptr)
        {
            g.setColour (p.gold);
            g.setFont (sans (14.5f, true));
            juce::String inst = snd->instrument;
            if (snd->library.isNotEmpty()) inst << "  |  " << snd->library;
            g.drawText (inst, why.removeFromTop (22.0f), juce::Justification::centredLeft, true);
        }
        why.removeFromTop (8.0f);
        g.setColour (p.muted);
        g.setFont (sans (12.0f, true));
        g.drawText ("WHY IT WORKS", why.removeFromTop (18.0f), juce::Justification::centredLeft);
        drawParagraph (g, catalog != nullptr ? whyItWorks (*catalog, *sel) : juce::String(), why, sans (14.0f), p.text);

        g.setColour (p.muted);
        g.setFont (sans (12.0f, true));
        g.drawText ("OTHER CHOICES FOR THIS PART", choicesTitle, juce::Justification::centredLeft);

        auto how = howArea;
        g.setColour (p.muted);
        g.setFont (sans (12.0f, true));
        g.drawText ("HOW TO LOAD IT", how.removeFromTop (18.0f), juce::Justification::centredLeft);
        if (snd != nullptr) drawParagraph (g, howToLoad (*snd), how, sans (13.5f), p.text);
        if (copiedNote.isNotEmpty())
        {
            g.setColour (p.good);
            g.setFont (sans (12.5f));
            g.drawFittedText (copiedNote, copyBtn.getBounds().translated (0, copyBtn.getHeight() + 2).withHeight (32).toFloat().toNearestInt(),
                              juce::Justification::centredLeft, 2);
        }
    }

    void SongPage::resized()
    {
        auto r = getLocalBounds().toFloat().reduced (24.0f, 0.0f).withTrimmedBottom (16.0f);

        // the form
        formPanel = r.removeFromTop (150.0f);
        {
            auto in = formPanel.reduced (20.0f, 16.0f);
            auto row1 = in.removeFromTop (48.0f);
            findBtn.setBounds (row1.removeFromRight (230.0f).toNearestInt());
            row1.removeFromRight (14.0f);
            describeBox.setBounds (row1.toNearestInt());
            in.removeFromTop (12.0f);
            auto row2 = in.removeFromTop (36.0f);
            const float gap = 12.0f;
            genreBox.setBounds (row2.removeFromLeft (220.0f).toNearestInt());
            row2.removeFromLeft (gap);
            styleBox.setBounds (row2.removeFromLeft (220.0f).toNearestInt());
            row2.removeFromLeft (gap);
            keyBox.setBounds (row2.removeFromLeft (190.0f).toNearestInt());
            row2.removeFromLeft (gap);
            bpmBox.setBounds (row2.removeFromLeft (80.0f).toNearestInt());
            row2.removeFromLeft (gap);
            keyCompassBtn.setBounds (row2.removeFromLeft (150.0f).toNearestInt());
            understoodLine = in.withTrimmedTop (4.0f);
        }
        r.removeFromTop (12.0f);

        // the palette bar
        auto bar = r.removeFromTop (38.0f);
        for (auto* t : tabs) if (t->isVisible()) { t->setBounds (bar.removeFromLeft (120.0f).toNearestInt()); bar.removeFromLeft (8.0f); }
        bar.removeFromLeft (8.0f);
        newBtn.setBounds (bar.removeFromLeft (170.0f).toNearestInt());
        barHint = bar.withTrimmedLeft (16.0f);
        r.removeFromTop (12.0f);

        // the cards, then the details of the selected one
        detailsPanel = r.removeFromBottom (juce::jmax (190.0f, r.getHeight() * 0.42f));
        r.removeFromBottom (14.0f);
        cardsArea = r;
        if (! cards.isEmpty())
        {
            const float gap = 10.0f;
            const float w = (cardsArea.getWidth() - gap * (float) (cards.size() - 1)) / (float) cards.size();
            auto c = cardsArea;
            for (auto* card : cards)
            {
                card->setBounds (c.removeFromLeft (w).toNearestInt());
                c.removeFromLeft (gap);
            }
        }
        {
            auto in = detailsPanel.reduced (22.0f, 18.0f);
            whyArea = in.removeFromLeft (in.getWidth() * 0.38f);
            in.removeFromLeft (24.0f);
            auto mid = in.removeFromLeft (in.getWidth() * 0.5f);
            choicesTitle = mid.removeFromTop (18.0f);
            mid.removeFromTop (4.0f);
            choices.setBounds (mid.toNearestInt());
            in.removeFromLeft (24.0f);
            howArea = in.withTrimmedBottom (96.0f);
            copyBtn.setBounds (in.removeFromBottom (80.0f).removeFromTop (44.0f).toNearestInt());
        }
    }
}
