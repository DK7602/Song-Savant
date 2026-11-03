#include "SongPage.h"
#include "PluginProcessor.h"
#include "core/Siblings.h"
#include "core/Paths.h"
#include "core/KitMap.h"
#include "core/GrooveGrabberLink.h"
#include "core/StyleGuide.h"

namespace ss::ui
{
    //==============================================================================
    void PartCard::show (const PaletteSlot* s, const Sound* snd, bool sel, const juce::String& info)
    {
        slot = s;
        sound = snd;
        isSelected = sel;
        partInfo = info;
        setTooltip (partInfo.isNotEmpty() ? "Starter part: " + partInfo + ". Drag the MIDI tag into Cubase." : juce::String());
        repaint();
    }

    void PartCard::setEditing (bool e, bool m)
    {
        if (e == canEdit && m == mine) return;
        canEdit = e;
        mine = m;
        repaint();
    }

    void PartCard::setPlaying (bool h, bool a)
    {
        if (h == heard && a == alone && ! heard) return;
        heard = h;
        alone = a;
        repaint();             // (while it plays: the level meter moves)
    }

    void PartCard::mouseMove (const juce::MouseEvent& e)
    {
        const auto tip = playArea.contains (e.position)
                           ? juce::String (alone ? "Stop" : "Hear just this part, looped. While it plays, click another card to hear that one.")
                           : partInfo.isNotEmpty() ? "Starter part: " + partInfo + ". Drag the MIDI tag into Cubase." : juce::String();
        if (tip != getTooltip()) setTooltip (tip);
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

        midiArea = {};
        if (partInfo.isNotEmpty())
        {
            // the starter part's tag: drag it into the DAW
            midiArea = juce::Rectangle<float> (art.getX() + 6.0f, art.getBottom() - 24.0f, 54.0f, 18.0f);
            const bool hot = midiArea.contains (getMouseXYRelative().toFloat()) && isMouseOver (true);
            g.setColour ((hot ? p.gold : p.panel).withAlpha (hot ? 0.95f : 0.85f));
            g.fillRoundedRectangle (midiArea, 9.0f);
            g.setColour (hot ? p.goldText : p.gold);
            g.drawRoundedRectangle (midiArea.reduced (0.5f), 9.0f, 1.0f);
            g.setFont (sans (10.5f, true));
            g.drawText ("MIDI", midiArea.withTrimmedLeft (8.0f), juce::Justification::centredLeft);
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 2; ++col)
                    g.fillEllipse (midiArea.getRight() - 15.0f + (float) col * 4.5f, midiArea.getY() + 4.5f + (float) row * 3.5f, 2.2f, 2.2f);
        }

        playArea = {};
        if (partInfo.isNotEmpty())
        {
            // play just this part (a stop square while it plays on its own)
            playArea = juce::Rectangle<float> (art.getRight() - 30.0f, art.getBottom() - 30.0f, 24.0f, 24.0f);
            const bool hot = playArea.contains (getMouseXYRelative().toFloat()) && isMouseOver (true);
            if (alone || hot)
            {
                g.setGradientFill (juce::ColourGradient (p.gold, playArea.getX(), playArea.getY(), p.goldDeep, playArea.getX(), playArea.getBottom(), false));
                g.fillEllipse (playArea);
                g.setColour (p.goldText);
            }
            else
            {
                g.setColour (p.panel.withAlpha (0.85f));
                g.fillEllipse (playArea);
                g.setColour (p.gold);
                g.drawEllipse (playArea.reduced (0.5f), 1.0f);
            }
            const float is = 9.0f;
            auto icon = juce::Rectangle<float> (is, is).withCentre (playArea.getCentre());
            TransportButton::drawIcon (g, TransportButton::Icon::play, alone, alone ? icon : icon.translated (1.0f, 0.0f));
        }
        editArea = {};
        if (canEdit && partInfo.isNotEmpty())
        {
            // EDIT: the piano roll
            editArea = juce::Rectangle<float> (art.getX() + 6.0f, art.getY() + 6.0f, 38.0f, 18.0f);   // top left: room on any card
            const bool hot = editArea.contains (getMouseXYRelative().toFloat()) && isMouseOver (true);
            g.setColour ((hot ? p.gold : p.panel).withAlpha (hot ? 0.95f : 0.85f));
            g.fillRoundedRectangle (editArea, 9.0f);
            g.setColour (hot ? p.goldText : p.gold);
            g.drawRoundedRectangle (editArea.reduced (0.5f), 9.0f, 1.0f);
            g.setFont (sans (10.5f, true));
            g.drawText ("EDIT", editArea, juce::Justification::centred);
        }
        const float meterX = editArea.isEmpty() ? art.getX() + 9.0f : editArea.getRight() + 6.0f;
        if (mine)
        {
            // your own part (from the piano roll), under EDIT
            auto tag = juce::Rectangle<float> (art.getX() + 6.0f, art.getY() + (editArea.isEmpty() ? 6.0f : 28.0f), 64.0f, 15.0f);
            g.setColour (p.purple.withAlpha (0.92f));
            g.fillRoundedRectangle (tag, 7.5f);
            g.setColour (juce::Colours::white);
            g.setFont (sans (9.5f, true));
            g.drawText ("YOUR PART", tag, juce::Justification::centred);
        }
        if (heard)
        {
            // a small moving level meter: this part is playing
            const auto t = (double) juce::Time::getMillisecondCounter() / 1000.0;
            auto meter = juce::Rectangle<float> (meterX, art.getY() + 8.0f, 16.0f, 13.0f);
            g.setColour (p.gold);
            for (int b = 0; b < 3; ++b)
            {
                const float level = 0.35f + 0.65f * (float) (0.5 + 0.5 * std::sin (t * (5.0 + b * 1.7) + b * 2.1));
                g.fillRoundedRectangle (meter.getX() + (float) b * 6.0f, meter.getBottom() - meter.getHeight() * level, 4.0f, meter.getHeight() * level, 1.0f);
            }
        }

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
            g.drawFittedText (sound->source == "Plug-in" ? juce::String ("Plug-in: pick inside") : sound->instrument,
                              in.removeFromTop (18.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
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

    void PartCard::mouseDown (const juce::MouseEvent& e)
    {
        pressedOnMidi = midiArea.contains (e.position);
        dragging = false;
    }

    void PartCard::mouseDrag (const juce::MouseEvent& e)
    {
        if (! pressedOnMidi || dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        if (onDragPart) onDragPart();
    }

    void PartCard::mouseUp (const juce::MouseEvent& e)
    {
        if (dragging) { dragging = false; return; }
        const auto pos = e.position;
        if (playArea.contains (pos) && ! pressedOnMidi) { if (onPlayPart) onPlayPart(); return; }
        if (editArea.contains (pos) && ! pressedOnMidi) { if (onEdit) onEdit(); return; }
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

        ideaBtn.setTooltip ("Another take on the starter parts: other chords from the style, a new hook");
        ideaBtn.onClick = [this] { ++proc.song.partsSeed; refresh(); };
        addAndMakeVisible (ideaBtn);
        allParts.setTooltip ("Drag every starter part into Cubase: one track each");
        addAndMakeVisible (allParts);
        playBtn.setTooltip ("Hear all the starter parts together, looped, with simple built-in sounds (inside Cubase: in time with Cubase's "
                            "play button). For one part on its own, click the play button on its card.");
        playBtn.onClick = [this] { playAll(); };
        addChildComponent (playBtn);

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
        sendBtn.setTooltip ("Send your song (style, key, tempo and sections) to Groove Grabber. It builds four kits for it from your own samples, "
                            "with the 808 tuned to the key. The kits you save there show up here as drum choices.");
        sendBtn.onClick = [this] { sendToGrooveGrabber(); };
        addChildComponent (sendBtn);

        showForm();          // the form as it was saved
        timerCallback();
        startTimerHz (6);
    }

    SongPage::~SongPage()   { stopTimer(); }

    void SongPage::showForm()
    {
        const auto& f = proc.song.form;
        describeBox.setText (f.description, false);
        genreBox.setSelectedId (f.genre >= 0 ? f.genre + 2 : 1, juce::dontSendNotification);
        fillStyles (f.genre);
        keyBox.setSelectedId (f.key >= 0 ? f.key + 2 : 1, juce::dontSendNotification);
        bpmBox.setText (f.bpm > 0 ? juce::String (juce::roundToInt (f.bpm)) : juce::String(), false);
        repaint();
    }

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

    void SongPage::themeChanged()
    {
        describeBox.setTextToShowWhenEmpty ("Describe your song: modern country like Morgan Wallen, warm, 120 BPM in A minor", pal().dim);
        bpmBox.setTextToShowWhenEmpty ("BPM", pal().dim);
        for (auto* ed : { &describeBox, &bpmBox }) ed->applyColourToAllText (pal().text, true);
        refresh();
    }

    void SongPage::playAll()
    {
        const bool playing = proc.isSketchPlaying (SongSavantProcessor::SketchMode::loop);
        if (playing && proc.getSketchSolo().isEmpty()) proc.stopSketch();
        else
        {
            proc.setSketchSolo ({});                                 // every part (in time, if one was playing alone)
            if (! playing) proc.playSketch (SongSavantProcessor::SketchMode::loop);
        }
        showPlaying();
    }

    void SongPage::playPart (int card)
    {
        auto* pal = proc.song.currentPalette();
        if (pal == nullptr || card < 0 || card >= (int) pal->slots.size()) return;
        const auto role = pal->slots[(size_t) card].part.role;
        if (selectedRole != role) { selectedRole = role; copiedNote.clear(); }
        const bool playing = proc.isSketchPlaying (SongSavantProcessor::SketchMode::loop);
        if (playing && proc.getSketchSolo() == role) proc.stopSketch();
        else if (proc.setSketchSolo (role) && ! playing) proc.playSketch (SongSavantProcessor::SketchMode::loop);
        refresh();
    }

    void SongPage::showPlaying()
    {
        const bool playing = proc.isSketchPlaying (SongSavantProcessor::SketchMode::loop);
        const auto solo = playing ? proc.getSketchSolo() : juce::String();
        const bool all = playing && solo.isEmpty();
        if (playBtn.getToggleState() != all)
        {
            playBtn.setToggleState (all, juce::dontSendNotification);
            playBtn.setButtonText (all ? "STOP" : "PLAY ALL");
        }
        if (auto* pal = proc.song.currentPalette())
            for (int i = 0; i < cards.size() && i < (int) pal->slots.size(); ++i)
            {
                const auto& role = pal->slots[(size_t) i].part.role;
                const bool has = playing && proc.sketchHasPart (role);
                cards[i]->setPlaying (has && (solo.isEmpty() || solo == role), has && solo == role);
            }
    }

    void SongPage::timerCallback()
    {
        showPlaying();          // PLAY ALL / STOP, and which cards are playing
        // inside a DAW: the song follows its tempo
        if (proc.isInDaw() && std::abs (proc.getHostBpm() - proc.song.hostBpm) > 0.01)
        {
            proc.song.hostBpm = proc.getHostBpm();
            refresh();
        }
        if (++kitTicks >= 18)            // every 3 seconds: a kit saved in Groove Grabber?
        {
            kitTicks = 0;
            checkGrooveGrabberKits();
        }
        if (proc.catalog.generation() != seenGeneration)
        {
            seenGeneration = proc.catalog.generation();
            if (lookedAtKits) kitsStale = true;      // the library was read again: read the kits into it again too
            const auto previous = catalog;
            catalog = proc.catalog.catalog();
            if (catalog != nullptr && proc.song.needsRestore()) proc.song.restore (*catalog);
            else if (catalog != nullptr && previous != nullptr && previous != catalog) proc.song.moveTo (*catalog);
            refresh();
        }
    }

    void SongPage::showKeyCompassMenu()
    {
        const auto songs = checkSiblingApps (proc.locationProvider()).songs;
        juce::PopupMenu m;
        m.addSectionHeader ("KEY COMPASS SONGS");
        if (songs.empty()) m.addItem (-1, "No Key Compass songs found", false);
        if (proc.song.form.fromSong.isNotEmpty())
        {
            m.addItem (1000, "Don't use " + proc.song.form.fromSong + " any more");
            m.addSeparator();
        }
        for (size_t i = 0; i < songs.size() && i < 20; ++i)
        {
            const auto& s = songs[i];
            m.addItem ((int) i + 1, s.name + "   " + keyName (s.key) + (s.bpm > 0 ? ", " + juce::String (juce::roundToInt (s.bpm)) + " BPM" : juce::String()));
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keyCompassBtn),
                         [this, songs, safe = juce::Component::SafePointer<SongPage> (this)] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            if (r == 1000)
            {
                proc.song.form.fromSong.clear();
                proc.song.form.chords.clear();
                proc.song.form.sections.clear();
                proc.song.form.sectionChords.clear();
                refresh();
                return;
            }
            if (r > (int) songs.size()) return;
            const auto& s = songs[(size_t) r - 1];
            if (s.key >= 0) keyBox.setSelectedId (s.key + 2, juce::dontSendNotification);
            if (s.bpm > 0) bpmBox.setText (juce::String (juce::roundToInt (s.bpm)), false);
            proc.song.form.fromSong = s.name;
            proc.song.form.chords = s.chords;      // the starter parts follow the song's chords
            proc.song.form.sections = s.sections;  // and the arrangement its sections
            proc.song.form.sectionChords = s.sectionChords;
            refresh();
        });
    }

    DrumNotes SongPage::drumNotes() const
    {
        if (proc.song.palettes.empty()) return generalMidiDrums();
        return drumNotesFor (proc.song.palettes[(size_t) juce::jlimit (0, (int) proc.song.palettes.size() - 1, proc.song.current)], catalog.get());
    }

    juce::File SongPage::writeStarterParts (int card)
    {
        auto* pal = proc.song.currentPalette();
        if (pal == nullptr) return {};
        const auto plan = proc.song.partsPlan();
        const auto drums = drumNotes();
        const auto all = arrangementParts (*pal, catalog.get());
        std::vector<NamedPart> parts;
        for (int i = 0; i < (int) pal->slots.size(); ++i)
        {
            if (card >= 0 && i != card) continue;
            const auto& slot = pal->slots[(size_t) i];
            for (auto& ap : all)
                if (ap.role == slot.part.role)
                {
                    // your own part where you made one in the piano roll, else the suggestion
                    parts.push_back ({ ap.label, starterPart (plan, proc.song.arrangement, &proc.song.patterns, ap, drums) });
                }
        }
        if (parts.empty()) return {};
        const int bars = starterBars (plan, proc.song.arrangement, &proc.song.patterns, all);
        const auto what = card >= 0 ? pal->slots[(size_t) card].part.role : juce::String ("Starter parts");
        const auto file = paths::tempDir().getChildFile ("Starter Parts")
                              .getChildFile (juce::File::createLegalFileName (what + " - " + keyName (plan.key) + " " + juce::String (juce::roundToInt (plan.bpm)) + " BPM.mid"));
        return writeMidiFile (plan, parts, file, {}, bars) ? file : juce::File();
    }

    void SongPage::visibilityChanged()
    {
        if (isVisible()) refresh();         // back from the piano roll: the cards play your parts
    }

    void SongPage::dragPart (int card)
    {
        const auto file = writeStarterParts (card);
        if (! file.existsAsFile() || card < 0 || card >= cards.size()) return;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, cards[card]);
    }

    PaletteSlot* SongPage::selectedSlot()
    {
        if (auto* p = proc.song.currentPalette())
            for (auto& s : p->slots)
                if (s.part.role == selectedRole) return &s;
        return nullptr;
    }

    juce::File SongPage::sendToGrooveGrabber()
    {
        auto* s = selectedSlot();
        juce::String drums;
        if (s != nullptr && catalog != nullptr && s->current().isValid() && s->current().sound < (int) catalog->sounds.size())
            drums = catalog->sounds[(size_t) s->current().sound].name;
        const auto file = ss::sendToGrooveGrabber (proc.song, drums);
        const auto name = songNameFor (proc.song);
        if (file != juce::File()) sentAt = juce::Time::getCurrentTime();
        if (file == juce::File())
            copiedNote = "Couldn't save the song for Groove Grabber in " + grooveGrabberSongsFolder().getFullPathName() + ".";
        else if (! grooveGrabberInstalled())
            copiedNote = "Saved \"" + name + "\" for Groove Grabber. It isn't installed on this computer yet: once it is, the song is in its KEY COMPASS menu.";
        else
            copiedNote = "Sent \"" + name + "\". In Groove Grabber click USE IT, then SAVE KIT on the one you like: it comes back here on the drum card.";
        repaint();
        return file;
    }

    bool SongPage::checkGrooveGrabberKits()
    {
        if (catalog == nullptr || proc.catalog.isBuilding()) return false;
        if (ggKits == juce::File()) ggKits = proc.locationProvider().grooveGrabberKits;
        const auto files = grooveGrabberKitFiles (ggKits);
        if (! lookedAtKits)
        {
            // the first look: the library was just read with these kits in it
            lookedAtKits = true;
            ggKitFiles = files;
            return false;
        }
        if (files == ggKitFiles && ! kitsStale) return false;
        const auto before = ggKitFiles;
        ggKitFiles = files;
        kitsStale = false;

        const auto key = newKitForSong (ggKits, before, proc.song, sentAt);
        proc.catalog.setCatalog (withGrooveGrabberKits (*catalog, ggKits));
        seenGeneration = proc.catalog.generation();
        catalog = proc.catalog.catalog();
        proc.song.moveTo (*catalog);

        bool placed = false;
        if (key.isNotEmpty() && putOnDrumCard (proc.song, *catalog, key) > 0)
        {
            if (auto* pal = proc.song.currentPalette())
                for (auto& slot : pal->slots)
                    if (partTypeFor (slot.part) == PartType::drums) selectedRole = slot.part.role;
            copiedNote = "Your Groove Grabber kit \"" + catalog->sounds[(size_t) findSoundByKey (*catalog, key)].name
                         + "\" is on the drum card (locked). The starter parts and ARRANGE use it.";
            placed = true;
        }
        refresh();
        return placed;
    }

    void SongPage::copySelected()
    {
        auto* s = selectedSlot();
        if (s == nullptr || catalog == nullptr || ! s->current().isValid()) return;
        const auto& snd = catalog->sounds[(size_t) s->current().sound];
        juce::SystemClipboard::copyTextToClipboard (nameToCopy (snd));
        copiedNote = "Copied \"" + nameToCopy (snd) + "\". " + whereToPaste (snd);
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
        const auto plan = proc.song.partsPlan();
        const auto drums = drumNotes();
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
            const auto type = partTypeFor (slot.part);
            const int main = mainSection (proc.song.arrangement, role);
            const bool mine = type != PartType::none && proc.song.isMine (main, role);
            card->show (&slot, snd, role == selectedRole,
                        type == PartType::none ? juce::String()
                        : mine ? "your own part (from the piano roll, the " + proc.song.arrangement.sections[(size_t) main].patternFor (role) + ")"
                               : describePart (plan, type, drums, snd != nullptr ? snd->kind : slot.part.kinds.front()));
            card->setEditing (type == PartType::bass || type == PartType::chords || type == PartType::pad || type == PartType::hook, mine);
            card->onEdit = [this, role] { if (onEditPart) onEditPart (role); };
            card->onDragPart = [this, i] { dragPart (i); };
            card->onPlayPart = [this, i] { playPart (i); };
            card->onSelect = [this, role]
            {
                if (selectedRole != role)
                {
                    selectedRole = role;
                    copiedNote.clear();
                    // one part playing on its own: now this card's (if it has a part to play)
                    if (proc.isSketchPlaying (SongSavantProcessor::SketchMode::loop) && proc.getSketchSolo().isNotEmpty())
                        proc.setSketchSolo (role);
                }
                refresh();
            };
            card->onPrev = [this, i] { if (auto* p = proc.song.currentPalette()) { auto& s = p->slots[(size_t) i]; s.cycle (-1); if (catalog != nullptr && s.current().isValid()) s.keepKey = soundKey (catalog->sounds[(size_t) s.current().sound]); } };
            card->onNext = [this, i] { if (auto* p = proc.song.currentPalette()) { auto& s = p->slots[(size_t) i]; s.cycle (1); if (catalog != nullptr && s.current().isValid()) s.keepKey = soundKey (catalog->sounds[(size_t) s.current().sound]); } };
            card->onLock = [this, i] { if (auto* p = proc.song.currentPalette()) { auto& s = p->slots[(size_t) i]; if (s.current().isValid()) s.locked = ! s.locked; } };
        }
        // the starter parts, all together
        ideaBtn.setVisible (pal != nullptr);
        allParts.setVisible (pal != nullptr);
        playBtn.setVisible (pal != nullptr);
        if (pal != nullptr)     // what PLAY ALL plays: the same starter parts as ALL PARTS
            proc.updateSketch (SongSavantProcessor::SketchMode::loop,
                               std::make_shared<const SketchSong> (sketchOfParts (plan, arrangementParts (*pal, catalog.get()),
                                                                                  &proc.song.arrangement, &proc.song.patterns)));
        showPlaying();
        if (pal != nullptr)
            allParts.setFile (writeStarterParts (-1), plan.chordLine() + "  |  " + juce::String (juce::roundToInt (plan.bpm)) + " BPM", "No parts yet");

        auto* sel = selectedSlot();
        choices.show (sel, catalog.get());
        copyBtn.setVisible (sel != nullptr);
        copyBtn.setEnabled (sel != nullptr && sel->current().isValid());
        sendBtn.setVisible (sel != nullptr && partTypeFor (sel->part) == PartType::drums);
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
                if (proc.song.hostBpm > 0 && proc.song.form.bpm <= 0)
                {
                    // inside a DAW the song follows its tempo; say when the style usually sits elsewhere
                    juce::String t = juce::String (juce::roundToInt (proc.song.hostBpm)) + " BPM (" + (proc.dawName().isNotEmpty() ? proc.dawName() : juce::String ("your DAW")) + ")";
                    const auto usual = styleGuide (b.genre, b.styleId).tempo;      // "85-100 BPM"
                    const auto nums = juce::StringArray::fromTokens (usual.retainCharacters ("0123456789-"), "-", "");
                    if (nums.size() >= 2 && nums[0].getIntValue() > 0)
                    {
                        const double lo = nums[0].getIntValue(), hi = juce::jmax (lo, (double) nums[1].getIntValue());
                        if (proc.song.hostBpm < lo - 5 || proc.song.hostBpm > hi + 5) t << ", the style is usually " << usual;
                    }
                    bits.add (t);
                }
                else if (b.bpm > 0 && ! bits.contains (juce::String (juce::roundToInt (b.bpm)) + " BPM")) bits.add (juce::String (juce::roundToInt (b.bpm)) + " BPM");
                if (b.key >= 0 && ! bits.contains (keyName (b.key))) bits.add (keyName (b.key));
                u = "Your song: " + bits.joinIntoString ("  |  ");
            }
            else u = "Name a genre, a feeling, an artist or a song. Song Savant picks sounds you already own.";
            if (proc.song.form.fromSong.isNotEmpty()) u << "     (key, tempo and chords from Key Compass: " << proc.song.form.fromSong << ")";
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
        if (barHint.getWidth() >= 300.0f)       // when there's room: what the starter parts play
        {
            const auto plan = proc.song.partsPlan();
            auto top = barHint.withTrimmedBottom (barHint.getHeight() * 0.5f);
            g.setColour (p.gold);
            g.setFont (sans (13.0f, true));
            g.drawText ("STARTER PARTS IN " + keyName (plan.key).toUpperCase(), top, juce::Justification::bottomRight, true);
            g.setColour (p.dim);
            g.setFont (sans (12.0f));
            juce::String sub = (plan.keyGiven ? juce::String() : "Key picked for you. ") + juce::String ("Drag a card's MIDI tag for one part.");
            g.drawText (sub, barHint.withTrimmedTop (barHint.getHeight() * 0.5f), juce::Justification::topRight, true);
        }

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
            juce::String inst = snd->source == "Plug-in" ? juce::String ("Plug-in  |  you pick the sound inside it") : snd->instrument;
            if (snd->library.isNotEmpty() && snd->source != "Plug-in") inst << "  |  " << snd->library;
            g.drawText (inst, why.removeFromTop (22.0f), juce::Justification::centredLeft, true);

            // its tags, as chips
            juce::StringArray chips;
            for (auto& t : snd->character) chips.addIfNotAlreadyThere (t.trim(), true);
            for (auto& t : snd->styles) chips.addIfNotAlreadyThere (t.trim(), true);
            chips.removeEmptyStrings();
            if (! chips.isEmpty())
            {
                why.removeFromTop (4.0f);
                auto row = why.removeFromTop (22.0f);
                const auto f = sans (11.5f, true);
                g.setFont (f);
                for (int i = 0; i < chips.size() && i < 6; ++i)
                {
                    const float w = juce::GlyphArrangement::getStringWidth (f, chips[i]) + 20.0f;
                    if (w > row.getWidth()) break;
                    auto chip = row.removeFromLeft (w);
                    row.removeFromLeft (6.0f);
                    g.setColour (p.purple.withAlpha (0.22f));
                    g.fillRoundedRectangle (chip, 11.0f);
                    g.setColour (p.purple.withAlpha (0.65f));
                    g.drawRoundedRectangle (chip.reduced (0.5f), 11.0f, 1.0f);
                    g.setColour (p.text);
                    g.drawText (chips[i], chip, juce::Justification::centred, false);
                }
            }
        }
        why.removeFromTop (8.0f);
        g.setColour (p.muted);
        g.setFont (sans (12.0f, true));
        g.drawText ("WHY IT WORKS", why.removeFromTop (18.0f), juce::Justification::centredLeft);
        if (catalog != nullptr)
        {
            // a tick for each reason, as many as fit
            const auto f = sans (13.5f);
            for (auto& point : whyItWorksPoints (*catalog, *sel))
            {
                if (why.getHeight() < 17.0f) break;
                auto line = why.removeFromTop (19.0f);
                auto tick = line.removeFromLeft (20.0f).withSizeKeepingCentre (11.0f, 11.0f);
                juce::Path t;
                t.startNewSubPath (tick.getX(), tick.getCentreY());
                t.lineTo (tick.getX() + tick.getWidth() * 0.38f, tick.getBottom() - 1.0f);
                t.lineTo (tick.getRight(), tick.getY() + 1.0f);
                g.setColour (p.gold);
                g.strokePath (t, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (p.text);
                g.setFont (f);
                g.drawFittedText (point, line.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
            }
        }

        g.setColour (p.muted);
        g.setFont (sans (12.0f, true));
        g.drawText ("OTHER CHOICES FOR THIS PART", choicesTitle, juce::Justification::centredLeft);

        auto how = howArea;
        g.setColour (p.muted);
        g.setFont (sans (12.0f, true));
        g.drawText ("HOW TO LOAD IT", how.removeFromTop (18.0f), juce::Justification::centredLeft);
        if (snd != nullptr)
        {
            // smaller type when it's long, so it never runs into the COPY NAME button
            const auto text = howToLoad (*snd);
            float size = 13.5f;
            while (size > 11.0f && paragraphHeight (text, how.getWidth(), sans (size)) > how.getHeight()) size -= 0.5f;
            drawParagraph (g, text, how, sans (size), p.text);
        }
        if (copiedNote.isNotEmpty())
        {
            g.setColour (p.good);
            g.setFont (sans (12.5f));
            const auto row = sendBtn.isVisible() ? copyBtn.getBounds().getUnion (sendBtn.getBounds()) : copyBtn.getBounds();
            g.drawFittedText (copiedNote, row.translated (0, row.getHeight() + 2).withHeight (32), juce::Justification::centredLeft, 2);
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
        auto bar = r.removeFromTop (44.0f);
        const float k = bar.getWidth() < 1100.0f ? 0.88f : 1.0f;       // a narrow window: everything on the bar a little smaller
        auto pills = bar.withSizeKeepingCentre (bar.getWidth(), 38.0f);
        for (auto* t : tabs) if (t->isVisible()) { t->setBounds (pills.removeFromLeft (120.0f * k).toNearestInt()); pills.removeFromLeft (8.0f); }
        pills.removeFromLeft (8.0f);
        newBtn.setBounds (pills.removeFromLeft (170.0f * k).toNearestInt());
        allParts.setBounds (bar.removeFromRight (250.0f * k).toNearestInt());
        bar.removeFromRight (10.0f);
        ideaBtn.setBounds (bar.removeFromRight (120.0f * k).withSizeKeepingCentre (120.0f * k, 38.0f).toNearestInt());
        bar.removeFromRight (8.0f);
        playBtn.setBounds (bar.removeFromRight (100.0f * k).withSizeKeepingCentre (100.0f * k, 38.0f).toNearestInt());
        barHint = bar.withLeft (newBtn.getRight() + 16.0f).withTrimmedRight (12.0f);
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
            auto buttons = in.removeFromBottom (80.0f).removeFromTop (44.0f);
            if (sendBtn.isVisible())
            {
                // the drums: COPY NAME and SEND TO GROOVE GRABBER side by side
                auto send = buttons.removeFromRight (buttons.getWidth() * 0.62f);
                buttons.removeFromRight (8.0f);
                sendBtn.setBounds (send.toNearestInt());
                const float full = juce::GlyphArrangement::getStringWidth (sans (15.0f, true), "SEND TO GROOVE GRABBER") + 30.0f;
                sendBtn.setButtonText (send.getWidth() >= full ? "SEND TO GROOVE GRABBER" : "TO GROOVE GRABBER");
            }
            copyBtn.setBounds (buttons.toNearestInt());
        }
    }
}
