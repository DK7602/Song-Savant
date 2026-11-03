#include "ArrangePage.h"
#include "PluginProcessor.h"
#include "core/Paths.h"

namespace ss::ui
{
    namespace
    {
        Kind kindForPart (const ArrangementPart& p)
        {
            if (p.type == PartType::drums) return Kind::drums;
            if (p.type == PartType::bass) return Kind::bass;
            return p.instrument;
        }

        juce::String modeLabel (const ArrangementPart& p, const juce::String& mode)
        {
            if (mode.isEmpty()) return {};
            return p.type == PartType::drums ? mode.toUpperCase() : juce::String();
        }

        float modeStrength (const juce::String& mode)
        {
            return mode == "light" ? 0.35f : mode == "groove" ? 0.6f : mode == "build" ? 0.75f : 0.95f;
        }
    }

    //==============================================================================
    void ArrangementGrid::show (const Arrangement* a, const std::vector<ArrangementPart>* p, const PartsPlan* pl)
    {
        arr = a;
        parts = p;
        plan = pl;
        repaint();
    }

    int ArrangementGrid::layout (int width, int height)
    {
        columnX.clear();
        columnW.clear();
        if (arr == nullptr || parts == nullptr || arr->empty()) { setSize (width, height); return width; }
        const int rows = std::max (1, (int) parts->size());
        rowH = juce::jlimit (26.0f, 44.0f, ((float) height - headerH - 8.0f) / (float) rows);
        const float avail = (float) width - labelW - 8.0f;
        const float perBar = juce::jmax (7.0f, avail / (float) juce::jmax (1, arr->totalBars()));
        float x = labelW;
        for (auto& s : arr->sections)
        {
            const float w = juce::jmax (78.0f, perBar * (float) s.bars);
            columnX.push_back (x);
            columnW.push_back (w);
            x += w;
        }
        int needed = (int) std::ceil (x + 8.0f);
        if (needed <= width + 2) needed = width;          // it fits (rounding aside): no scroll bar
        setSize (needed, (int) (headerH + rowH * (float) rows + 8.0f));
        return needed;
    }

    juce::Rectangle<float> ArrangementGrid::headerArea (int s) const
    {
        return { columnX[(size_t) s], rulerH, columnW[(size_t) s], headerH - rulerH };
    }

    float ArrangementGrid::xAt (double beats) const
    {
        if (beats < 0.0 || arr == nullptr || columnX.empty() || columnX.size() != arr->sections.size()) return -1.0f;
        const double bar = beats / 4.0;
        double start = 0.0;
        for (size_t s = 0; s < arr->sections.size(); ++s)
        {
            const double bars = juce::jmax (1, arr->sections[s].bars);
            if (bar < start + bars) return columnX[s] + columnW[s] * (float) ((bar - start) / bars);
            start += bars;
        }
        return columnX.back() + columnW.back();             // (at the very end)
    }

    double ArrangementGrid::beatsAt (float x) const
    {
        if (arr == nullptr || columnX.empty() || columnX.size() != arr->sections.size()) return 0.0;
        double start = 0.0;
        for (size_t s = 0; s < columnX.size(); ++s)
        {
            const double bars = juce::jmax (1, arr->sections[s].bars);
            if (x < columnX[s] + columnW[s] || s + 1 == columnX.size())
            {
                const double f = juce::jlimit (0.0, 1.0, (double) (x - columnX[s]) / (double) columnW[s]);
                return std::round (start + f * bars) * 4.0;
            }
            start += bars;
        }
        return start * 4.0;
    }

    juce::Rectangle<float> ArrangementGrid::cellArea (int s, int p) const
    {
        return juce::Rectangle<float> (columnX[(size_t) s], headerH + rowH * (float) p, columnW[(size_t) s], rowH).reduced (2.0f, 3.0f);
    }

    void ArrangementGrid::hitTestAt (juce::Point<float> pos, int& section, int& part, bool& header) const
    {
        section = part = -1;
        header = false;
        if (arr == nullptr || parts == nullptr) return;
        for (size_t s = 0; s < columnX.size(); ++s)
            if (pos.x >= columnX[s] && pos.x < columnX[s] + columnW[s]) section = (int) s;
        if (section < 0 || pos.y < rulerH) { section = -1; return; }      // (the ruler: see mouseDown)
        if (pos.y < headerH) { header = true; return; }
        const int p = (int) ((pos.y - headerH) / rowH);
        if (p >= 0 && p < (int) parts->size()) part = p;
    }

    void ArrangementGrid::mouseMove (const juce::MouseEvent& e)
    {
        int s, p;
        bool h;
        hitTestAt (e.position, s, p, h);
        const bool ruler = e.position.y < rulerH && e.position.x >= labelW && ! columnX.empty();
        if (s != hoverSection || p != hoverPart || h != hoverHeader || ruler != hoverRuler)
        {
            hoverSection = s;
            hoverPart = p;
            hoverHeader = h;
            hoverRuler = ruler;
            setMouseCursor ((s >= 0 && (h || p >= 0)) || ruler ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            setTooltip (ruler ? "Click or drag here to move the playhead"
                        : h && s >= 0 ? "Change this section: length, copy, move, remove"
                        : p >= 0 && parts != nullptr && (*parts)[(size_t) p].type == PartType::drums ? "Click: no drums, light, groove, build, full"
                        : p >= 0 ? "Click to switch this part on or off here" : juce::String());
            repaint();
        }
    }

    void ArrangementGrid::mouseDown (const juce::MouseEvent& e)
    {
        onRuler = e.position.y < rulerH && e.position.x >= labelW && ! columnX.empty();
        if (onRuler && onSeek) onSeek (beatsAt (e.position.x));
    }

    void ArrangementGrid::mouseDrag (const juce::MouseEvent& e)
    {
        if (onRuler && onSeek) onSeek (beatsAt (e.position.x));
    }

    void ArrangementGrid::mouseUp (const juce::MouseEvent& e)
    {
        if (onRuler) { onRuler = false; return; }
        int s, p;
        bool h;
        hitTestAt (e.position, s, p, h);
        if (s < 0) return;
        if (h && onSection) onSection (s, localAreaToGlobal (headerArea (s).toNearestInt()));
        else if (p >= 0 && onCell) onCell (s, p);
    }

    void ArrangementGrid::setPlayhead (double beats, bool playing)
    {
        if (std::abs (beats - playhead) < 1.0e-3 && playing == playheadMoving) return;
        playhead = beats;
        playheadMoving = playing;
        repaint();
    }

    void ArrangementGrid::paint (juce::Graphics& g)
    {
        const auto& pl = pal();
        if (arr == nullptr || parts == nullptr || arr->empty()) return;
        // (the playhead is drawn last, over the boxes)
        struct Playhead
        {
            const ArrangementGrid& grid;
            juce::Graphics& g;
            ~Playhead()
            {
                const float x = grid.xAt (grid.playhead);
                if (x < 0.0f) return;
                // a line down through the sections, and a marker on the ruler (brighter while it plays)
                const auto gold = pal().gold.withAlpha (grid.playheadMoving ? 1.0f : 0.75f);
                g.setColour (gold);
                g.fillRect (x - 1.0f, rulerH - 2.0f, 2.0f, (float) grid.getHeight() - rulerH);
                juce::Path marker;
                marker.addTriangle (x - 7.0f, 2.0f, x + 7.0f, 2.0f, x, rulerH - 1.0f);
                g.fillPath (marker);
            }
        } playheadOnTop { *this, g };

        // the bar ruler: a tick for every bar, a taller one where each section starts
        if (! columnX.empty())
        {
            const float left = columnX.front(), right = columnX.back() + columnW.back();
            auto strip = juce::Rectangle<float> (left, 3.0f, right - left, rulerH - 6.0f);
            g.setColour (pl.panelHi.withAlpha (hoverRuler || onRuler ? 0.9f : 0.45f));
            g.fillRoundedRectangle (strip, 4.0f);
            for (size_t s = 0; s < arr->sections.size() && s < columnX.size(); ++s)
            {
                const int bars = juce::jmax (1, arr->sections[s].bars);
                for (int b = 0; b < bars; ++b)
                {
                    const float x = columnX[s] + columnW[s] * (float) b / (float) bars;
                    const bool first = b == 0;
                    g.setColour ((first ? pl.gold : pl.dim).withAlpha (first ? 0.8f : 0.5f));
                    g.fillRect (x, first ? strip.getY() : strip.getCentreY(), 1.0f, first ? strip.getHeight() : strip.getHeight() * 0.5f);
                }
            }
        }

        // the parts, down the left
        for (size_t p = 0; p < parts->size(); ++p)
        {
            const auto& part = (*parts)[p];
            auto row = juce::Rectangle<float> (0.0f, headerH + rowH * (float) p, labelW - 10.0f, rowH);
            const auto colour = kindColour (kindForPart (part));
            g.setColour (colour);
            g.fillRoundedRectangle (row.removeFromLeft (4.0f).reduced (0.0f, 6.0f), 2.0f);
            row.removeFromLeft (8.0f);
            const auto role = part.role + (part.extra ? "  (extra)" : "");
            const auto sound = part.label.fromFirstOccurrenceOf (" - ", false, false);
            g.setColour (pl.text);
            g.setFont (sans (13.5f, true));
            g.drawText (role, row.withTrimmedBottom (sound.isNotEmpty() && rowH > 30.0f ? rowH * 0.42f : 0.0f), juce::Justification::centredLeft, true);
            if (sound.isNotEmpty() && rowH > 30.0f)
            {
                g.setColour (pl.dim);
                g.setFont (sans (11.0f));
                g.drawText (sound, row.withTrimmedTop (rowH * 0.5f), juce::Justification::topLeft, true);
            }
        }

        // the sections, across
        int bar = 1;
        for (size_t s = 0; s < arr->sections.size(); ++s)
        {
            const auto& sec = arr->sections[s];
            auto head = headerArea ((int) s).reduced (3.0f, 2.0f);
            const bool hot = hoverHeader && hoverSection == (int) s;
            g.setColour (hot ? pl.purple.withAlpha (0.3f) : pl.panelHi.withAlpha (0.6f));
            g.fillRoundedRectangle (head, 8.0f);
            auto in = head.reduced (7.0f, 4.0f);
            g.setColour (pl.dim);
            g.setFont (sans (10.5f));
            g.drawText ("bar " + juce::String (bar), in.removeFromTop (13.0f), juce::Justification::centredLeft, true);
            g.setColour (pl.text);
            g.setFont (sans (13.5f, true));
            g.drawText (sec.name, in.removeFromTop (19.0f), juce::Justification::centredLeft, true);
            g.setColour (pl.gold);
            g.setFont (sans (11.0f));
            juce::String sub = juce::String (sec.bars) + " bars";
            if (plan != nullptr)
            {
                juce::StringArray names;
                for (auto& c : sectionChords (*plan, sec))
                {
                    if (names.size() >= 4) break;
                    if (names.isEmpty() || names[names.size() - 1] != c.name) names.add (c.name);
                }
                sub << "  " << names.joinIntoString (" ");
            }
            g.drawText (sub, in, juce::Justification::topLeft, true);
            bar += sec.bars;

            for (size_t p = 0; p < parts->size(); ++p)
            {
                const auto& part = (*parts)[p];
                const auto mode = sec.modeFor (part.role);
                auto cell = cellArea ((int) s, (int) p);
                const bool over = hoverSection == (int) s && hoverPart == (int) p;
                if (mode.isEmpty())
                {
                    g.setColour (pl.dim.withAlpha (over ? 0.6f : 0.25f));
                    g.drawRoundedRectangle (cell.reduced (0.5f), 6.0f, 1.0f);
                    continue;
                }
                const auto colour = kindColour (kindForPart (part));
                g.setColour (colour.withAlpha (modeStrength (mode) * (over ? 1.0f : 0.85f)));
                g.fillRoundedRectangle (cell, 6.0f);
                if (mode == "build")          // a ramp: the build-up
                {
                    juce::Path ramp;
                    ramp.addTriangle (cell.getX(), cell.getBottom(), cell.getRight(), cell.getBottom(), cell.getRight(), cell.getY());
                    g.setColour (pl.text.withAlpha (0.18f));
                    g.fillPath (ramp);
                }
                const auto label = modeLabel (part, mode);
                if (label.isNotEmpty() && cell.getWidth() > 46.0f)
                {
                    g.setColour (pl.text.withAlpha (0.9f));
                    g.setFont (sans (10.5f, true));
                    g.drawText (label, cell, juce::Justification::centred, true);
                }
            }
        }
    }

    //==============================================================================
    ArrangePage::ArrangePage (SongSavantProcessor& p) : proc (p)
    {
        shapeBox.onChange = [this]
        {
            proc.song.songShape = (SongShape) juce::jlimit (0, 2, shapeBox.getSelectedItemIndex());
            proc.song.resetArrangement (parts);
            changed();
        };
        addAndMakeVisible (shapeBox);
        resetBtn.setTooltip ("Put the map back the way it came for this song shape, undoing your changes to it");
        resetBtn.onClick = [this] { proc.song.resetArrangement (parts); changed(); };
        addAndMakeVisible (resetBtn);
        wholeSong.setTooltip ("Drag the whole song into Cubase: a track per part, a marker per section");
        addAndMakeVisible (wholeSong);

        // the transport: hear the whole song with simple built-in sounds
        toStartBtn.setTooltip ("To the start of the song");
        toStartBtn.onClick = [this] { proc.songMoveTo (0.0); showTransport(); };
        rewindBtn.setTooltip ("Back a bar (hold it down to keep going back)");
        rewindBtn.onClick = [this] { proc.songStep (-1); showTransport(); };
        playBtn.setTooltip ("Play the whole song from the playhead, with simple built-in sounds");
        playBtn.onClick = [this] { togglePlay(); };
        pauseBtn.setTooltip ("Pause: stops here, and PLAY carries on from here");
        pauseBtn.onClick = [this] { proc.songPause(); showTransport(); };
        forwardBtn.setTooltip ("On a bar (hold it down to keep going)");
        forwardBtn.onClick = [this] { proc.songStep (1); showTransport(); };
        toEndBtn.setTooltip ("To the end of the song");
        toEndBtn.onClick = [this] { proc.songMoveTo (proc.songLength()); showTransport(); };
        for (auto* b : { &rewindBtn, &forwardBtn })
        {
            b->setTriggeredOnMouseDown (true);         // a bar as soon as it's pressed,
            b->setRepeatSpeed (350, 110);              // and on while it's held down
        }
        for (auto* b : { &toStartBtn, &rewindBtn, &playBtn, &pauseBtn, &forwardBtn, &toEndBtn }) addChildComponent (b);
        grid.onSeek = [this] (double beats)
        {
            if (proc.songFollowsDaw() || std::abs (beats - proc.songPosition()) < 1.0e-6) return;
            proc.songMoveTo (beats);
            showTransport();
        };

        grid.onCell = [this] (int s, int part)
        {
            if (part >= 0 && part < (int) parts.size()) toggleArrangementCell (proc.song.arrangement, s, parts[(size_t) part]);
            changed();
        };
        grid.onSection = [this] (int s, juce::Rectangle<int> area) { sectionMenu (s, area); };
        view.setComponentID ("map");
        view.setViewedComponent (&grid, false);
        view.setScrollBarsShown (false, true);
        addAndMakeVisible (view);
        refresh();
        startTimerHz (2);
    }

    ArrangePage::~ArrangePage()   { stopTimer(); }

    const Arrangement& ArrangePage::getArrangement() const   { return proc.song.arrangement; }

    void ArrangePage::timerCallback()
    {
        if (! isVisible()) return;
        refresh();
        showTransport();
    }

    void ArrangePage::showTransport()
    {
        const bool playing = proc.isSketchPlaying (SongSavantProcessor::SketchMode::song);
        const bool withDaw = proc.songFollowsDaw();
        if (playBtn.getToggleState() != playing || playing != wasPlaying)
        {
            wasPlaying = playing;
            playBtn.setToggleState (playing, juce::dontSendNotification);
            playBtn.setTooltip (playing ? "Stop: the playhead goes back to where you started playing"
                                        : "Play the whole song from the playhead, with simple built-in sounds");
            stopTimer();
            startTimerHz (playing ? 15 : 2);          // the playhead moves smoothly while it plays
        }
        // inside Cubase while Cubase plays, it plays along with Cubase: Cubase's own buttons move it
        for (auto* b : { &toStartBtn, &rewindBtn, &forwardBtn, &toEndBtn }) b->setEnabled (! withDaw);
        pauseBtn.setEnabled (playing);
        grid.setPlayhead (hasPalette ? proc.songPosition() : -1.0, playing);
        if (playing && grid.getWidth() > view.getWidth())
        {
            // a long song that scrolls: keep the playhead in sight
            const float x = grid.xAt (proc.songPosition());
            const int left = view.getViewPositionX();
            if (x >= 0.0f && (x < (float) left + ArrangementGrid::labelW || x > (float) (left + view.getWidth() - 40)))
                view.setViewPosition (juce::jlimit (0, grid.getWidth() - view.getWidth(), (int) x - (int) ArrangementGrid::labelW - 20), 0);
        }
        const auto text = positionText();
        if (text != shownPosition)
        {
            shownPosition = text;
            repaint (transportInfo.getSmallestIntegerContainer().expanded (4));
        }
    }

    void ArrangePage::togglePlay()
    {
        if (proc.isSketchPlaying (SongSavantProcessor::SketchMode::song)) proc.songStop();
        else proc.songPlay();
        showTransport();
    }

    juce::String ArrangePage::positionText() const
    {
        const auto& a = proc.song.arrangement;
        if (! hasPalette || a.empty()) return {};
        const double beats = proc.songPosition();
        const int total = a.totalBars();
        const int bar = (int) std::floor (beats / 4.0 + 1.0e-6) + 1;
        juce::String section;
        int start = 1;
        for (auto& sec : a.sections)
        {
            if (bar < start + sec.bars) { section = sec.name; break; }
            start += sec.bars;
        }
        auto clock = [] (int s)   { return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2); };
        const double bpm = juce::jmax (40.0, plan.bpm);          // (as the WHOLE SONG tag works out the length)
        juce::String t = bar > total ? juce::String ("End") : "Bar " + juce::String (bar) + " of " + juce::String (total);
        if (section.isNotEmpty()) t << "  |  " << section;
        t << "  |  " << clock ((int) std::floor (beats * 60.0 / bpm + 1.0e-6)) << " / " << clock (juce::roundToInt (total * 4 * 60.0 / bpm));
        return t;
    }

    void ArrangePage::refresh()
    {
        catalog = proc.catalog.catalog();
        auto* pal = proc.song.currentPalette();
        hasPalette = pal != nullptr;
        parts = pal != nullptr ? arrangementParts (*pal, catalog.get()) : std::vector<ArrangementPart>();
        if (hasPalette) proc.song.updateArrangement (parts);
        plan = proc.song.partsPlan();

        // what to draw it for: only redraw (and rewrite the file) when something changed
        juce::String now = proc.song.arrangement.toXml()->toString() + "|" + plan.chordLine() + "|" + juce::String (plan.bpm) + "|" + juce::String (plan.key);
        for (auto& p : parts) now << "|" << p.label;
        if (now == seen) return;
        seen = now;

        const auto names = songShapeNames (proc.song.keyCompassShape());
        shapeBox.clear (juce::dontSendNotification);
        for (int i = 0; i < names.size(); ++i) shapeBox.addItem (names[i], i + 1);
        shapeBox.setSelectedItemIndex (juce::jlimit (0, names.size() - 1, (int) proc.song.songShape), juce::dontSendNotification);

        grid.show (hasPalette ? &proc.song.arrangement : nullptr, &parts, &plan);
        shapeBox.setVisible (hasPalette);
        resetBtn.setVisible (hasPalette);
        wholeSong.setVisible (hasPalette);
        for (auto* b : { &toStartBtn, &rewindBtn, &playBtn, &pauseBtn, &forwardBtn, &toEndBtn }) b->setVisible (hasPalette);
        if (hasPalette)         // what the transport plays: the same song as WHOLE SONG
            proc.updateSketch (SongSavantProcessor::SketchMode::song,
                               std::make_shared<const SketchSong> (sketchOfSong (plan, proc.song.arrangement, parts)));
        if (hasPalette)
        {
            const int bars = proc.song.arrangement.totalBars();
            const int seconds = juce::roundToInt (bars * 4 * 60.0 / juce::jmax (40.0, plan.bpm));
            wholeSong.setFile (writeWholeSong (), juce::String (bars) + " bars, " + juce::String (seconds / 60) + ":" + juce::String (seconds % 60).paddedLeft ('0', 2)
                                                  + " at " + juce::String (juce::roundToInt (plan.bpm)) + " BPM", "No song yet");
        }
        resized();
        showTransport();
        repaint();
    }

    void ArrangePage::changed()
    {
        seen.clear();
        refresh();
    }

    juce::File ArrangePage::writeWholeSong()
    {
        auto* pal = proc.song.currentPalette();
        if (pal == nullptr || proc.song.arrangement.empty()) return {};
        const auto song = arrangeParts (plan, proc.song.arrangement, parts, drumNotesFor (*pal, catalog.get()));
        const auto file = paths::tempDir().getChildFile ("Starter Parts").getChildFile (juce::File::createLegalFileName (
            "Whole song - " + keyName (plan.key) + " " + juce::String (juce::roundToInt (plan.bpm)) + " BPM.mid"));
        return writeArrangement (plan, proc.song.arrangement, song, file) ? file : juce::File();
    }

    void ArrangePage::sectionMenu (int s, juce::Rectangle<int> area)
    {
        auto& sections = proc.song.arrangement.sections;
        if (s < 0 || s >= (int) sections.size()) return;
        juce::PopupMenu m;
        m.addSectionHeader (sections[(size_t) s].name + "  (" + juce::String (sections[(size_t) s].bars) + " bars)");
        juce::PopupMenu lengths;
        for (int b : { 2, 4, 8, 12, 16, 24, 32 }) lengths.addItem (100 + b, juce::String (b) + " bars", true, sections[(size_t) s].bars == b);
        m.addSubMenu ("Length", lengths);
        m.addItem (1, "Copy this section (right after it)");
        m.addItem (2, "Move left", s > 0);
        m.addItem (3, "Move right", s + 1 < (int) sections.size());
        m.addSeparator();
        m.addItem (4, "Remove this section", sections.size() > 1);
        juce::Component::SafePointer<ArrangePage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (area), [safe, s] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            auto& a = safe->proc.song.arrangement;
            if (r > 100) setSectionBars (a, s, r - 100);
            else if (r == 1) copySection (a, s);
            else if (r == 2) moveSection (a, s, -1);
            else if (r == 3) moveSection (a, s, 1);
            else if (r == 4) removeSection (a, s);
            safe->changed();
        });
    }

    void ArrangePage::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        drawPanel (g, topPanel);
        auto t = titleArea;
        g.setColour (p.text);
        g.setFont (serif (24.0f));
        g.drawText ("Arrangement map", t.removeFromTop (30.0f), juce::Justification::centredLeft);
        g.setColour (p.muted);
        g.setFont (sans (13.0f));
        g.drawText (hasPalette ? plan.guide.name + ": which parts play where. Click a box to switch a part on or off; click a section's name to change it."
                               : juce::String ("The map uses your palette's parts."), t, juce::Justification::centredLeft, true);

        if (! hasPalette)
        {
            g.setColour (p.dim);
            g.setFont (serif (20.0f));
            g.drawFittedText ("Describe your song on the SONG page and click FIND MY SOUNDS first.\nThe map then lays your parts out across the song.",
                              emptyArea.toNearestInt(), juce::Justification::centred, 3);
            return;
        }

        // where the playhead is: the bar, the section, the time
        {
            auto box = transportInfo;
            g.setColour (p.panelHi.withAlpha (0.6f));
            g.fillRoundedRectangle (box, box.getHeight() * 0.5f);
            g.setColour (p.gold.withAlpha (0.5f));
            g.drawRoundedRectangle (box.reduced (0.5f), box.getHeight() * 0.5f, 1.0f);
            auto in = box.reduced (18.0f, 0.0f);
            const auto bits = juce::StringArray::fromTokens (shownPosition, "|", {});
            const juce::Colour colours[] = { p.gold, p.text, p.muted };
            for (int i = 0; i < bits.size() && i < 3; ++i)
            {
                const auto bit = bits[i].trim();
                const auto font = sans (i == 0 ? 15.0f : 14.0f, i < 2);
                const float w = juce::GlyphArrangement::getStringWidth (font, bit) + 2.0f;
                g.setColour (colours[i]);
                g.setFont (font);
                g.drawText (bit, in.removeFromLeft (juce::jmin (w, in.getWidth())), juce::Justification::centredLeft, false);
                if (i + 1 < bits.size() && i < 2)
                {
                    g.setColour (p.dim);
                    g.fillEllipse (in.getX() + 9.0f, in.getCentreY() - 2.0f, 4.0f, 4.0f);
                    in.removeFromLeft (22.0f);
                }
            }
            // what moves it
            const auto dawName = proc.dawName().isNotEmpty() ? proc.dawName() : juce::String ("your DAW");
            g.setColour (p.dim);
            g.setFont (sans (12.5f));
            g.drawText (proc.songFollowsDaw() ? "Playing along with " + dawName + ": move it with " + dawName + "'s own buttons"
                                              : juce::String ("Click the bar ruler above the sections to move the playhead"),
                        transportHint, juce::Justification::centredLeft, true);
        }

        // how songs in this style are built
        drawPanel (g, tipsArea, 14.0f);
        auto in = tipsArea.reduced (18.0f, 12.0f);
        g.setColour (p.gold);
        g.setFont (sans (12.0f, true));
        g.drawText (("How " + plan.guide.name + " songs are built").toUpperCase(), in.removeFromTop (20.0f), juce::Justification::centredLeft, true);
        juce::String tips;
        for (auto& line : plan.guide.arrangement) tips << line << ".   ";
        tips << "The drums box steps through light, groove, build and full; drag WHOLE SONG into Cubase for a track per part and a marker per section.";
        drawParagraph (g, tips, in, sans (13.0f), p.text);
    }

    void ArrangePage::resized()
    {
        auto r = getLocalBounds().toFloat().reduced (24.0f, 0.0f).withTrimmedBottom (16.0f);
        topPanel = r.removeFromTop (84.0f);
        {
            auto in = topPanel.reduced (20.0f, 14.0f);
            wholeSong.setBounds (in.removeFromRight (280.0f).withSizeKeepingCentre (280.0f, 52.0f).toNearestInt());
            in.removeFromRight (10.0f);
            resetBtn.setBounds (in.removeFromRight (130.0f).withSizeKeepingCentre (130.0f, 38.0f).toNearestInt());
            in.removeFromRight (10.0f);
            shapeBox.setBounds (in.removeFromRight (200.0f).withSizeKeepingCentre (200.0f, 36.0f).toNearestInt());
            titleArea = in;
        }
        r.removeFromTop (12.0f);

        // the transport: to the start, rewind, play / stop, pause, fast forward, to the end; then where the playhead is
        {
            auto row = r.removeFromTop (50.0f);
            for (auto* b : { &toStartBtn, &rewindBtn, &playBtn, &pauseBtn, &forwardBtn, &toEndBtn })
            {
                const float size = b == &playBtn ? 50.0f : 42.0f;
                b->setBounds (row.removeFromLeft (size).withSizeKeepingCentre (size, size).toNearestInt());
                row.removeFromLeft (8.0f);
            }
            row.removeFromLeft (10.0f);
            const float infoW = juce::jmin (380.0f, row.getWidth());
            transportInfo = row.removeFromLeft (infoW).withSizeKeepingCentre (infoW, 40.0f);
            row.removeFromLeft (16.0f);
            transportHint = row;
        }
        r.removeFromTop (8.0f);
        emptyArea = r;
        tipsArea = r.removeFromBottom (juce::jmin (130.0f, r.getHeight() * 0.25f));
        r.removeFromBottom (12.0f);
        view.setBounds (r.toNearestInt());
        grid.layout (view.getWidth(), view.getHeight() - (grid.getWidth() > view.getWidth() ? view.getScrollBarThickness() : 0));
    }
}
