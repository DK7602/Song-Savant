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
        return { columnX[(size_t) s], 0.0f, columnW[(size_t) s], headerH };
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
        if (section < 0) return;
        if (pos.y < headerH) { header = true; return; }
        const int p = (int) ((pos.y - headerH) / rowH);
        if (p >= 0 && p < (int) parts->size()) part = p;
    }

    void ArrangementGrid::mouseMove (const juce::MouseEvent& e)
    {
        int s, p;
        bool h;
        hitTestAt (e.position, s, p, h);
        if (s != hoverSection || p != hoverPart || h != hoverHeader)
        {
            hoverSection = s;
            hoverPart = p;
            hoverHeader = h;
            setMouseCursor (s >= 0 && (h || p >= 0) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            setTooltip (h && s >= 0 ? "Change this section: length, copy, move, remove"
                        : p >= 0 && parts != nullptr && (*parts)[(size_t) p].type == PartType::drums ? "Click: no drums, light, groove, build, full"
                        : p >= 0 ? "Click to switch this part on or off here" : juce::String());
            repaint();
        }
    }

    void ArrangementGrid::mouseUp (const juce::MouseEvent& e)
    {
        int s, p;
        bool h;
        hitTestAt (e.position, s, p, h);
        if (s < 0) return;
        if (h && onSection) onSection (s, localAreaToGlobal (headerArea (s).toNearestInt()));
        else if (p >= 0 && onCell) onCell (s, p);
    }

    void ArrangementGrid::paint (juce::Graphics& g)
    {
        const auto& pl = pal();
        if (arr == nullptr || parts == nullptr || arr->empty()) return;

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
        resetBtn.setTooltip ("Back to the song's shape as it comes, undoing your changes");
        resetBtn.onClick = [this] { proc.song.resetArrangement (parts); changed(); };
        addAndMakeVisible (resetBtn);
        wholeSong.setTooltip ("Drag the whole song into Cubase: a track per part, a marker per section");
        addAndMakeVisible (wholeSong);

        grid.onCell = [this] (int s, int part)
        {
            if (part >= 0 && part < (int) parts.size()) toggleArrangementCell (proc.song.arrangement, s, parts[(size_t) part]);
            changed();
        };
        grid.onSection = [this] (int s, juce::Rectangle<int> area) { sectionMenu (s, area); };
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
        if (isVisible()) refresh();
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
        if (hasPalette)
        {
            const int bars = proc.song.arrangement.totalBars();
            const int seconds = juce::roundToInt (bars * 4 * 60.0 / juce::jmax (40.0, plan.bpm));
            wholeSong.setFile (writeWholeSong (), juce::String (bars) + " bars, " + juce::String (seconds / 60) + ":" + juce::String (seconds % 60).paddedLeft ('0', 2)
                                                  + " at " + juce::String (juce::roundToInt (plan.bpm)) + " BPM", "No song yet");
        }
        resized();
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
            in.removeFromRight (12.0f);
            resetBtn.setBounds (in.removeFromRight (150.0f).withSizeKeepingCentre (150.0f, 38.0f).toNearestInt());
            in.removeFromRight (10.0f);
            shapeBox.setBounds (in.removeFromRight (220.0f).withSizeKeepingCentre (220.0f, 36.0f).toNearestInt());
            titleArea = in;
        }
        r.removeFromTop (14.0f);
        emptyArea = r;
        tipsArea = r.removeFromBottom (juce::jmin (130.0f, r.getHeight() * 0.25f));
        r.removeFromBottom (12.0f);
        view.setBounds (r.toNearestInt());
        grid.layout (view.getWidth(), view.getHeight() - (grid.getWidth() > view.getWidth() ? view.getScrollBarThickness() : 0));
    }
}
