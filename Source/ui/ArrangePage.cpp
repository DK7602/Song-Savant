#include "ArrangePage.h"
#include "PluginProcessor.h"
#include "core/Paths.h"
#include "core/DrumLanes.h"
#include <limits>

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
    void ArrangementGrid::show (const Arrangement* a, const std::vector<ArrangementPart>* p, const PartsPlan* pl, const PatternBook* b)
    {
        arr = a;
        parts = p;
        plan = pl;
        book = b;
        repaint();
    }

    bool ArrangementGrid::editable (const ArrangementPart& p)
    {
        return p.type != PartType::none;
    }

    // a MIDI pattern dragged over the map: dropped on a box, it opens that part of that section in the piano roll
    bool ArrangementGrid::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (auto& f : files) if (isMidiFile (juce::File (f))) return parts != nullptr && ! parts->empty();
        return false;
    }

    void ArrangementGrid::fileDragMove (const juce::StringArray&, int x, int y)
    {
        int s, p;
        bool h;
        hitTestAt ({ (float) x, (float) y }, s, p, h);
        if (p < 0 && x < (int) labelW && y >= (int) headerH && parts != nullptr) { p = (int) (((float) y - headerH) / rowH); s = -1; }
        if (p >= (int) (parts != nullptr ? parts->size() : 0)) p = -1;
        if (s != dropSection || p != dropPart) { dropSection = s; dropPart = p; repaint(); }
    }

    void ArrangementGrid::fileDragExit (const juce::StringArray&)
    {
        dropSection = -2;
        dropPart = -1;
        repaint();
    }

    void ArrangementGrid::filesDropped (const juce::StringArray& files, int x, int y)
    {
        fileDragMove (files, x, y);
        const int s = dropSection, p = dropPart;
        dropSection = -2;
        dropPart = -1;
        repaint();
        if (p < 0 || parts == nullptr || ! editable ((*parts)[(size_t) p]) || onDropFile == nullptr) return;
        for (auto& f : files)
            if (isMidiFile (juce::File (f))) { onDropFile (s, p, juce::File (f)); return; }
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
                        : h && s >= 0 ? "Click: change this section (add or remove bars, add a section...)  |  drag it left or right to move it"
                        : p >= 0 && parts != nullptr && (*parts)[(size_t) p].type == PartType::drums
                              ? "Click: no drums, light, groove, build, full  |  drag it into your DAW for just this bit"
                        : p >= 0 ? "Click: switch this part on or off here  |  double-click: open it in the piano roll  |  drag it into your DAW for just this bit"
                        : e.position.x < labelW && e.position.y >= headerH ? "Drag a part's name into your DAW for the whole part"
                        : juce::String());
            repaint();
        }
    }

    void ArrangementGrid::mouseDown (const juce::MouseEvent& e)
    {
        if (auto* page = findParentComponentOfClass<ArrangePage>()) page->grabKeyboardFocus();   // (Ctrl+Z / Ctrl+Y)
        menuShown = false;
        movingSection = moveGap = -1;
        onRuler = e.position.y < rulerH && e.position.x >= labelW && ! columnX.empty();
        if (onRuler && onSeek) onSeek (beatsAt (e.position.x));
        bool h;
        hitTestAt (e.position, pressSection, pressPart, h);
        pressHeader = h && pressSection >= 0;
        if (h) pressPart = -1;
        pressLabel = e.position.x < labelW - 10.0f && e.position.y >= headerH && parts != nullptr
                     && (int) ((e.position.y - headerH) / rowH) < (int) parts->size();
        if (pressLabel) pressPart = (int) ((e.position.y - headerH) / rowH);
        draggedOut = false;

        // right-click anywhere in a section (its name or a box): its menu; past the last section: add one at the end
        if (e.mods.isPopupMenu() && ! onRuler && ! pressLabel && arr != nullptr && ! columnX.empty() && onSection)
        {
            const float end = columnX.back() + columnW.back();
            if (pressSection >= 0)
            {
                menuShown = true;
                onSection (pressSection, localAreaToGlobal (headerArea (pressSection).toNearestInt()));
            }
            else if (e.position.x >= end && e.position.y >= rulerH)
            {
                menuShown = true;
                onSection ((int) arr->sections.size(), localAreaToGlobal (juce::Rectangle<int> ((int) e.position.x, (int) e.position.y, 1, 1)));
            }
        }
    }

    int ArrangementGrid::gapAt (float x) const
    {
        if (columnX.empty()) return -1;
        int best = 0;
        float bestD = std::numeric_limits<float>::max();
        for (size_t i = 0; i <= columnX.size(); ++i)
        {
            const float edge = i < columnX.size() ? columnX[i] : columnX.back() + columnW.back();
            if (const float d = std::abs (x - edge); d < bestD) { bestD = d; best = (int) i; }
        }
        return best;
    }

    void ArrangementGrid::showMove (int section, int gap, float x)
    {
        movingSection = section;
        moveGap = gap;
        moveX = x >= 0.0f ? x : (gap >= 0 && ! columnX.empty() ? (gap < (int) columnX.size() ? columnX[(size_t) gap] : columnX.back() + columnW.back()) : 0.0f);
        repaint();
    }

    void ArrangementGrid::mouseDrag (const juce::MouseEvent& e)
    {
        if (menuShown) return;
        if (onRuler) { if (onSeek) onSeek (beatsAt (e.position.x)); return; }
        // a section's name dragged left or right: it moves (the gold line shows where it lands)
        if (pressHeader && ! e.mods.isPopupMenu())
        {
            if (movingSection < 0 && e.getDistanceFromDragStart() < 6) return;
            movingSection = pressSection;
            moveGap = gapAt (e.position.x);
            moveX = e.position.x;
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            {
                const auto inView = e.getEventRelativeTo (vp).position.toInt();
                vp->autoScroll (inView.x, inView.y, 40, 10);
            }
            repaint();
            return;
        }
        if (draggedOut || e.getDistanceFromDragStart() < 6 || pressPart < 0) return;
        draggedOut = true;               // into the DAW: this box, or the whole part
        if (pressLabel) { if (onDragPart) onDragPart (pressPart); }
        else if (pressSection >= 0 && onDragCell) onDragCell (pressSection, pressPart);
    }

    void ArrangementGrid::mouseUp (const juce::MouseEvent& e)
    {
        if (menuShown) { menuShown = false; return; }
        if (onRuler) { onRuler = false; return; }
        if (movingSection >= 0)
        {
            const int from = movingSection, gap = moveGap;
            movingSection = moveGap = -1;
            pressHeader = false;
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            repaint();
            if (gap >= 0 && gap != from && gap != from + 1 && onMoveSection) onMoveSection (from, gap);
            return;
        }
        pressHeader = false;
        if (draggedOut) { draggedOut = false; return; }
        int s, p;
        bool h;
        hitTestAt (e.position, s, p, h);
        if (s < 0) return;
        if (h && onSection) onSection (s, localAreaToGlobal (headerArea (s).toNearestInt()));
        else if (p >= 0 && onCell)
        {
            // a double-click on a part that can be edited: the piano roll (the first click's switch is put back)
            if (e.getNumberOfClicks() >= 2 && parts != nullptr && editable ((*parts)[(size_t) p]) && onEditCell)
            {
                if (onCellRevert) onCellRevert (s, p);
                onEditCell (s, p);
                return;
            }
            onCell (s, p);
        }
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

        // a MIDI pattern being dragged over a box: where it will go
        struct DropTarget
        {
            const ArrangementGrid& grid;
            juce::Graphics& g;
            ~DropTarget()
            {
                if (grid.dropPart < 0 || grid.dropSection < -1 || grid.columnX.empty()) return;
                juce::Rectangle<float> r;
                if (grid.dropSection >= 0) r = grid.cellArea (grid.dropSection, grid.dropPart);
                else r = { 0.0f, headerH + grid.rowH * (float) grid.dropPart, labelW - 10.0f, grid.rowH };
                g.setColour (pal().gold);
                g.drawRoundedRectangle (r.expanded (1.0f), 6.0f, 2.5f);
            }
        } dropOnTop { *this, g };

        // a section's name being dragged: its column faded, a gold line where it lands, its name with the mouse
        struct Moving
        {
            const ArrangementGrid& grid;
            juce::Graphics& g;
            ~Moving()
            {
                const auto s = grid.movingSection;
                if (s < 0 || grid.arr == nullptr || s >= (int) grid.columnX.size()) return;
                const auto& pl = pal();
                const float bottom = (float) grid.getHeight() - 4.0f;
                g.setColour (pl.bgTop.withAlpha (0.55f));
                g.fillRect (juce::Rectangle<float> (grid.columnX[(size_t) s], rulerH, grid.columnW[(size_t) s], bottom - rulerH));
                const bool lands = grid.moveGap >= 0 && grid.moveGap != s && grid.moveGap != s + 1;
                if (lands)
                {
                    const float x = grid.moveGap < (int) grid.columnX.size() ? grid.columnX[(size_t) grid.moveGap]
                                                                            : grid.columnX.back() + grid.columnW.back();
                    g.setColour (pl.gold);
                    g.fillRoundedRectangle (x - 2.0f, rulerH, 4.0f, bottom - rulerH, 2.0f);
                    juce::Path arrow;
                    arrow.addTriangle (x - 8.0f, rulerH - 2.0f, x + 8.0f, rulerH - 2.0f, x, rulerH + 8.0f);
                    g.fillPath (arrow);
                }
                const auto& name = grid.arr->sections[(size_t) s].name;
                const auto font = sans (13.5f, true);
                const float w = juce::GlyphArrangement::getStringWidth (font, name) + 28.0f;
                auto chip = juce::Rectangle<float> (w, 30.0f).withCentre ({ grid.moveX, rulerH + 34.0f });
                g.setColour (pl.purple.withAlpha (0.92f));
                g.fillRoundedRectangle (chip, 15.0f);
                g.setColour (pl.gold);
                g.drawRoundedRectangle (chip.reduced (0.5f), 15.0f, 1.5f);
                g.setColour (juce::Colours::white);
                g.setFont (font);
                g.drawText (name, chip, juce::Justification::centred, false);
            }
        } movingOnTop { *this, g };

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
                // your own pattern here (a pencil), and a section with a pattern of its own (a star)
                const auto pattern = sec.patternFor (part.role);
                const bool own = pattern != sec.name;
                const bool mine = book != nullptr && book->find (part.role, pattern) != nullptr;
                if ((mine || own) && cell.getWidth() > 30.0f)
                {
                    auto corner = juce::Rectangle<float> (cell.getRight() - 16.0f, cell.getY() + 3.0f, 12.0f, 12.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.9f));
                    if (mine)
                    {
                        juce::Path pencil;
                        pencil.addRectangle (-1.6f, -6.0f, 3.2f, 9.0f);
                        pencil.addTriangle (-1.6f, 3.0f, 1.6f, 3.0f, 0.0f, 6.0f);
                        pencil.applyTransform (juce::AffineTransform::rotation (0.7f).translated (corner.getCentreX(), corner.getCentreY()));
                        g.fillPath (pencil);
                        corner = corner.translated (-14.0f, 0.0f);
                    }
                    if (own)
                    {
                        juce::Path star;
                        star.addStar (corner.getCentre(), 5, 2.4f, 6.0f);
                        g.fillPath (star);
                    }
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
        resetBtn.onClick = [this] { edit ([this] (Arrangement&) { proc.song.resetArrangement (parts); }); };
        addAndMakeVisible (resetBtn);
        undoBtn.setTooltip ("Take back your last change to the map (Ctrl+Z)");
        undoBtn.onClick = [this] { undo(); };
        redoBtn.setTooltip ("Put it back again (Ctrl+Y)");
        redoBtn.onClick = [this] { redo(); };
        for (auto* b : { &undoBtn, &redoBtn })
        {
            b->setWantsKeyboardFocus (false);
            addChildComponent (b);
        }
        setWantsKeyboardFocus (true);
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
            if (part < 0 || part >= (int) parts.size()) return;
            edit ([this, s, part] (Arrangement& a) { toggleArrangementCell (a, s, parts[(size_t) part]); });
            lastCell = { s, part };
        };
        grid.onCellRevert = [this] (int s, int part)
        {
            // a double-click: the first click switched the box; put it back (and forget that step)
            if (lastCell == juce::Point<int> (s, part) && ! undos.empty())
            {
                proc.song.arrangement = undos.back();
                undos.pop_back();
                lastCell = { -1, -1 };
                changed();
            }
        };
        grid.onSection = [this] (int s, juce::Rectangle<int> area) { sectionMenu (s, area); };
        grid.onMoveSection = [this] (int s, int gap) { moveSectionTo (s, gap); };
        grid.onEditCell = [this] (int s, int part)
        {
            if (part >= 0 && part < (int) parts.size() && onEditPattern) onEditPattern (parts[(size_t) part].role, s);
        };
        grid.onDragCell = [this] (int s, int part)
        {
            if (const auto f = writeBit (s, part); f.existsAsFile())
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, &grid);
        };
        grid.onDropFile = [this] (int s, int part, const juce::File& f)
        {
            if (part >= 0 && part < (int) parts.size() && onDropPattern) onDropPattern (parts[(size_t) part].role, s, f);
        };
        grid.onDragPart = [this] (int part)
        {
            if (const auto f = writeBit (-1, part); f.existsAsFile())
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, &grid);
        };
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
        if (proc.song.arrangement.signature != undoFor)
        {
            // a new palette, style or shape: a new map, and the steps back belonged to the old one
            undos.clear();
            redos.clear();
            undoFor = proc.song.arrangement.signature;
        }
        updateUndoButtons();

        // what to draw it for: only redraw (and rewrite the file) when something changed
        juce::String now = proc.song.arrangement.toXml()->toString() + "|" + plan.chordLine() + "|" + juce::String (plan.bpm) + "|" + juce::String (plan.key)
                           + "|" + juce::String (proc.song.patterns.changeCount()) + "/" + juce::String (proc.song.patterns.size());
        for (auto& p : parts) now << "|" << p.label;
        if (now == seen) return;
        seen = now;

        const auto names = songShapeNames (proc.song.keyCompassShape());
        shapeBox.clear (juce::dontSendNotification);
        for (int i = 0; i < names.size(); ++i) shapeBox.addItem (names[i], i + 1);
        shapeBox.setSelectedItemIndex (juce::jlimit (0, names.size() - 1, (int) proc.song.songShape), juce::dontSendNotification);

        grid.show (hasPalette ? &proc.song.arrangement : nullptr, &parts, &plan, &proc.song.patterns);
        shapeBox.setVisible (hasPalette);
        resetBtn.setVisible (hasPalette);
        wholeSong.setVisible (hasPalette);
        for (auto* b : { &toStartBtn, &rewindBtn, &playBtn, &pauseBtn, &forwardBtn, &toEndBtn }) b->setVisible (hasPalette);
        for (auto* b : { &undoBtn, &redoBtn }) b->setVisible (hasPalette);
        if (hasPalette)         // what the transport plays: the same song as WHOLE SONG
            proc.updateSketch (SongSavantProcessor::SketchMode::song,
                               std::make_shared<const SketchSong> (sketchOfSong (plan, proc.song.arrangement, parts, &proc.song.patterns)));
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
        const auto song = arrangeParts (plan, proc.song.arrangement, parts, drumNotesFor (*pal, catalog.get()), &proc.song.patterns);
        const auto file = paths::tempDir().getChildFile ("Starter Parts").getChildFile (juce::File::createLegalFileName (
            "Whole song - " + keyName (plan.key) + " " + juce::String (juce::roundToInt (plan.bpm)) + " BPM.mid"));
        return writeArrangement (plan, proc.song.arrangement, song, file) ? file : juce::File();
    }

    juce::File ArrangePage::writeBit (int section, int partIndex)
    {
        auto* pal = proc.song.currentPalette();
        const auto& a = proc.song.arrangement;
        if (pal == nullptr || a.empty() || partIndex < 0 || partIndex >= (int) parts.size() || section >= (int) a.sections.size()) return {};
        const auto& part = parts[(size_t) partIndex];
        const auto song = arrangePart (plan, a, part, drumNotesFor (*pal, catalog.get()), &proc.song.patterns, section);
        juce::String name = part.role + " - whole song";
        if (section >= 0)
        {
            int bar = 1;
            for (int i = 0; i < section; ++i) bar += a.sections[(size_t) i].bars;
            name = part.role + " - " + a.sections[(size_t) section].name + " (bar " + juce::String (bar) + ")";
        }
        const auto file = paths::tempDir().getChildFile ("Starter Parts").getChildFile (juce::File::createLegalFileName (
            name + " - " + keyName (plan.key) + " " + juce::String (juce::roundToInt (plan.bpm)) + " BPM.mid"));
        if (section < 0) return writeArrangement (plan, a, song, file) ? file : juce::File();      // with the section markers
        return writeMidiFile (plan, song, file, {}, a.sections[(size_t) section].bars) ? file : juce::File();
    }

    void ArrangePage::sectionMenu (int s, juce::Rectangle<int> area)
    {
        const auto& sections = proc.song.arrangement.sections;
        const int n = (int) sections.size();
        if (s < 0 || s > n) return;

        // what can be added: one more like a section the song has (it plays the same), or a new kind
        const auto inSong = sectionNamesInSong (proc.song.arrangement);
        const auto fresh = newSectionNames (plan.guide, proc.song.arrangement);
        auto addMenu = [&inSong, &fresh] (int base)
        {
            juce::PopupMenu m;
            m.addSectionHeader ("ONE MORE (PLAYS THE SAME)");
            for (int i = 0; i < inSong.size(); ++i) m.addItem (base + i, inSong[i]);
            if (! fresh.isEmpty())
            {
                m.addSectionHeader ("A NEW SECTION");
                for (int i = 0; i < fresh.size(); ++i) m.addItem (base + 500 + i, fresh[i]);
            }
            return m;
        };

        juce::PopupMenu m;
        if (s == n)
        {
            m.addSectionHeader ("THE END OF THE SONG");
            m.addSubMenu ("Add a section at the end", addMenu (2000));
        }
        else
        {
            m.addSectionHeader (sections[(size_t) s].name + "  (" + juce::String (sections[(size_t) s].bars) + " bars)");
            m.addSubMenu ("Add a section before", addMenu (1000));
            m.addSubMenu ("Add a section after", addMenu (2000));
            m.addSeparator();
            // its length: bars added to (or taken off) the end, or a length picked; every part in the section goes with it
            const int bars = sections[(size_t) s].bars;
            auto barsText = [] (int b) { return juce::String (b) + (b == 1 ? " bar" : " bars"); };
            juce::PopupMenu more, fewer, lengths;
            more.addSectionHeader ("AT THE END, FOR EVERY PART");
            fewer.addSectionHeader ("FROM THE END, FOR EVERY PART");
            for (int b : { 1, 2, 4, 8 })
            {
                more.addItem (300 + b, barsText (b) + " more  (" + barsText (bars + b) + ")", bars + b <= 64);
                fewer.addItem (400 + b, barsText (b) + " fewer  (" + barsText (bars - b) + ")", bars - b >= 1);
            }
            juce::Array<int> sizes { 2, 4, 8, 12, 16, 24, 32 };
            if (! sizes.contains (bars)) { sizes.add (bars); sizes.sort(); }
            for (int b : sizes) lengths.addItem (100 + b, barsText (b), true, bars == b);
            m.addSubMenu ("Add bars", more, bars < 64);
            m.addSubMenu ("Remove bars", fewer, bars > 1);
            m.addSubMenu ("Length", lengths);
            m.addItem (1, "Copy this section (right after it)");
            m.addItem (2, "Move left", s > 0);
            m.addItem (3, "Move right", s + 1 < n);
            m.addSeparator();
            m.addItem (4, "Delete this section", n > 1);
        }
        juce::Component::SafePointer<ArrangePage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (area), [safe, s, inSong, fresh] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            if (r >= 1000)
            {
                const int at = r >= 2000 ? s + (s < (int) safe->proc.song.arrangement.sections.size() ? 1 : 0) : s;
                const int i = (r % 1000) % 500;
                const auto& names = (r % 1000) >= 500 ? fresh : inSong;
                if (i < names.size()) safe->addSectionAt (at, names[i]);
                return;
            }
            if (r > 400)       safe->changeBars (s, -(r - 400));
            else if (r > 300)  safe->changeBars (s, r - 300);
            else if (r > 100)  safe->edit ([s, r] (Arrangement& a) { setSectionBars (a, s, r - 100); });
            else if (r == 1)   safe->edit ([s] (Arrangement& a) { copySection (a, s); });
            else if (r == 2)   safe->moveSectionTo (s, s - 1);
            else if (r == 3)   safe->moveSectionTo (s, s + 2);
            else if (r == 4)   safe->deleteSection (s);
        });
    }

    //==============================================================================
    // Changing the map: every change is one step for UNDO
    void ArrangePage::edit (const std::function<void (Arrangement&)>& change)
    {
        auto before = proc.song.arrangement;
        change (proc.song.arrangement);
        if (proc.song.arrangement.toXml()->isEquivalentTo (before.toXml().get(), false)) return;   // (nothing changed)
        undos.push_back (std::move (before));
        if (undos.size() > 200) undos.erase (undos.begin());
        redos.clear();
        lastCell = { -1, -1 };
        undoFor = proc.song.arrangement.signature;
        changed();
    }

    void ArrangePage::moveSectionTo (int section, int gap)
    {
        edit ([section, gap] (Arrangement& a) { ss::moveSectionTo (a, section, gap); });
    }

    void ArrangePage::addSectionAt (int at, const juce::String& name)
    {
        edit ([this, at, name] (Arrangement& a) { ss::addSection (a, at, name, plan, parts); });
    }

    void ArrangePage::changeBars (int section, int by)
    {
        edit ([section, by] (Arrangement& a)
        {
            if (section >= 0 && section < (int) a.sections.size()) setSectionBars (a, section, a.sections[(size_t) section].bars + by);
        });
    }

    void ArrangePage::deleteSection (int section)
    {
        edit ([section] (Arrangement& a) { removeSection (a, section); });
    }

    void ArrangePage::undo()
    {
        if (undos.empty()) return;
        redos.push_back (proc.song.arrangement);
        proc.song.arrangement = undos.back();
        undos.pop_back();
        lastCell = { -1, -1 };
        changed();
    }

    void ArrangePage::redo()
    {
        if (redos.empty()) return;
        undos.push_back (proc.song.arrangement);
        proc.song.arrangement = redos.back();
        redos.pop_back();
        lastCell = { -1, -1 };
        changed();
    }

    void ArrangePage::updateUndoButtons()
    {
        undoBtn.setEnabled (! undos.empty());
        redoBtn.setEnabled (! redos.empty());
    }

    bool ArrangePage::keyPressed (const juce::KeyPress& key)
    {
        const auto mods = key.getModifiers();
        if (! mods.isCommandDown()) return false;
        const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
        const int code = key.getKeyCode();
        const bool z = c == 'z' || code == 'Z' || code == 'z', y = c == 'y' || code == 'Y' || code == 'y';
        if (z && ! mods.isShiftDown()) { undo(); return true; }
        if (y || (z && mods.isShiftDown())) { redo(); return true; }
        return false;
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
        g.setFont (sans (12.5f));
        // (narrow: without the style's name, which the box at the bottom shows)
        const juce::String how = "Click a box to switch a part on or off. Drag a section's name to move it; right-click a section to add or delete one.";
        g.drawFittedText (hasPalette ? (t.getWidth() > 450.0f ? plan.guide.name + ": which parts play where. " + how : how)
                                     : juce::String ("The map uses your palette's parts."), t.toNearestInt(), juce::Justification::centredLeft, 2, 0.95f);

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
            const auto hint = proc.songFollowsDaw() ? "Playing along with " + dawName + ": move it with " + dawName + "'s own buttons"
                                                    : juce::String ("Click the bar ruler to move the playhead");
            if (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), hint) <= transportHint.getWidth() || proc.songFollowsDaw())
                g.drawText (hint, transportHint, juce::Justification::centredLeft, true);
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
            redoBtn.setBounds (row.removeFromRight (84.0f).withSizeKeepingCentre (84.0f, 36.0f).toNearestInt());
            row.removeFromRight (8.0f);
            undoBtn.setBounds (row.removeFromRight (84.0f).withSizeKeepingCentre (84.0f, 36.0f).toNearestInt());
            row.removeFromRight (12.0f);
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
