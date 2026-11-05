#include "MasterclassPage.h"
#include "PluginProcessor.h"
#include "core/StarterParts.h"
#include "core/Palette.h"

namespace ss::ui
{
    namespace
    {
        constexpr float pad = 18.0f, gap = 16.0f, titleH = 26.0f, bulletGap = 6.0f, grooveRowH = 15.0f;
        const auto bulletFont = [] { return sans (14.0f); };

        // the drum lanes worth drawing, in the order a drummer reads them
        const char* laneOrder[] = { "crash", "ride", "openhat", "hat", "pedalhat", "shaker", "perc", "tomhigh", "tommid", "tomlow", "clap", "snare", "rim", "kick" };
        juce::String laneName (const juce::String& lane)
        {
            static const std::map<juce::String, juce::String> names { { "kick", "Kick" }, { "snare", "Snare" }, { "clap", "Clap" }, { "rim", "Rim" },
                { "hat", "Hi-hat" }, { "openhat", "Open hat" }, { "pedalhat", "Pedal hat" }, { "tomlow", "Low tom" }, { "tommid", "Mid tom" },
                { "tomhigh", "High tom" }, { "crash", "Crash" }, { "ride", "Ride" }, { "perc", "Perc" }, { "shaker", "Shaker" } };
            auto it = names.find (lane);
            return it != names.end() ? it->second : lane;
        }
    }

    //==============================================================================
    void Lesson::setContent (const StyleGuide& g, std::vector<Card> c, const juce::String& head, const juce::String& sub)
    {
        guide = g;
        cards = std::move (c);
        headline = head;
        subline = sub;
        layout (getWidth());
        repaint();
    }

    float Lesson::cardHeight (const Card& c, float width) const
    {
        float h = pad + titleH;
        for (auto& point : c.points) h += paragraphHeight (point, width - 2 * pad - 16.0f, bulletFont()) + bulletGap;
        if (c.groove)
        {
            int lanes = 0;
            for (auto* l : laneOrder) if (guide.groove.lanes.count (l) > 0) ++lanes;
            h += 14.0f + (float) lanes * grooveRowH + 18.0f;
        }
        return h + pad;
    }

    int Lesson::layout (int widthIn)
    {
        const float width = (float) juce::jmax (300, widthIn);
        // the headline: the style's name, its genre, what it is
        const float headH = 46.0f + 22.0f + paragraphHeight (subline, width - 2 * pad, sans (16.0f)) + 14.0f;
        headArea = { 0.0f, 0.0f, width, headH };
        float y = headH + gap;

        const int columns = width >= 860.0f ? 2 : 1;
        const float colW = (width - gap * (float) (columns - 1)) / (float) columns;
        float colY[2] = { y, y };
        for (auto& c : cards)
        {
            if (c.fullWidth)
            {
                const float top = juce::jmax (colY[0], colY[1]);
                c.area = { 0.0f, top, width, cardHeight (c, width) };
                colY[0] = colY[1] = c.area.getBottom() + gap;
                continue;
            }
            const int col = columns == 2 && colY[1] < colY[0] ? 1 : 0;
            c.area = { (float) col * (colW + gap), colY[col], colW, cardHeight (c, colW) };
            colY[col] = c.area.getBottom() + gap;
        }
        const int h = (int) std::ceil (juce::jmax (colY[0], colY[1]));
        setSize ((int) width, h);
        return h;
    }

    void Lesson::paintGroove (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const auto& p = pal();
        const auto& gr = guide.groove;
        const int steps = gr.steps;
        auto labels = r.removeFromLeft (78.0f);
        g.setColour (p.dim);
        g.setFont (sans (11.0f));
        auto head = r.removeFromTop (14.0f);
        const float stepW = r.getWidth() / (float) steps;
        for (int beat = 0; beat < steps / 4; ++beat)
            g.drawText (juce::String (beat % 4 + 1), head.getX() + (float) beat * 4.0f * stepW, head.getY(), 4.0f * stepW, head.getHeight(), juce::Justification::centredLeft);
        labels.removeFromTop (14.0f);
        for (auto* laneText : laneOrder)
        {
            const juce::String lane (laneText);
            auto it = gr.lanes.find (lane);
            if (it == gr.lanes.end()) continue;
            auto row = r.removeFromTop (grooveRowH);
            g.setColour (p.muted);
            g.setFont (sans (11.5f));
            g.drawText (laneName (lane), labels.removeFromTop (grooveRowH), juce::Justification::centredLeft);
            for (int s = 0; s < steps; ++s)
            {
                auto cell = juce::Rectangle<float> (row.getX() + (float) s * stepW, row.getY(), stepW, row.getHeight()).reduced (1.0f, 2.0f);
                g.setColour (s % 4 == 0 ? p.panelHi : p.panelHi.withAlpha (0.45f));
                g.fillRoundedRectangle (cell, 2.0f);
                const auto c = it->second[s];
                if (c == '.') continue;
                const float a = c == 'X' ? 1.0f : c == 'x' ? 0.85f : c == 'o' ? 0.55f : c == 'g' ? 0.35f : 0.9f;
                g.setColour ((lane == "kick" ? p.gold : lane == "snare" || lane == "clap" ? p.purple.brighter (0.3f) : p.blue.brighter (0.2f)).withAlpha (a));
                if (c == 'r' || c == 't')
                {
                    const int n = c == 'r' ? 2 : 3;
                    for (int i = 0; i < n; ++i)
                        g.fillRect (cell.getX() + cell.getWidth() * (float) i / (float) n + 0.5f, cell.getY(), cell.getWidth() / (float) n - 1.0f, cell.getHeight());
                }
                else
                    g.fillRoundedRectangle (cell, 2.0f);
            }
        }
        g.setColour (p.dim);
        g.setFont (sans (11.0f));
        g.drawText (gr.swing > 52 ? "Swing: about " + juce::String (gr.swing) + "%" : juce::String ("Straight 16ths"), r.removeFromTop (16.0f), juce::Justification::centredRight);
    }

    void Lesson::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        // the headline
        {
            auto r = headArea.reduced (pad, 6.0f);
            g.setColour (p.text);
            g.setFont (serif (34.0f));
            g.drawText (headline, r.removeFromTop (46.0f), juce::Justification::centredLeft, true);
            g.setColour (p.gold);
            g.setFont (sans (14.5f, true));
            g.drawText (guide.genre.toUpperCase() + (guide.name != guide.genre ? "  |  " + guide.name.toUpperCase() : juce::String()),
                        r.removeFromTop (22.0f), juce::Justification::centredLeft, true);
            drawParagraph (g, subline, r, sans (16.0f), p.text);
        }
        for (auto& c : cards)
        {
            drawPanel (g, c.area, 16.0f);
            auto r = c.area.reduced (pad);
            g.setColour (p.gold);
            g.setFont (sans (12.5f, true));
            g.drawText (c.title.toUpperCase(), r.removeFromTop (titleH), juce::Justification::topLeft);
            for (auto& point : c.points)
            {
                const float h = paragraphHeight (point, r.getWidth() - 16.0f, bulletFont());
                auto line = r.removeFromTop (h);
                g.setColour (p.purple.brighter (0.3f));
                g.fillEllipse (line.getX() + 2.0f, line.getY() + 7.0f, 5.0f, 5.0f);
                drawParagraph (g, point, line.withTrimmedLeft (16.0f), bulletFont(), p.text);
                r.removeFromTop (bulletGap);
            }
            if (c.groove)
            {
                r.removeFromTop (8.0f);
                paintGroove (g, r);
            }
        }
    }

    //==============================================================================
    MasterclassPage::MasterclassPage (SongSavantProcessor& p) : proc (p), coach (p)
    {
        for (int i = 0; i < numGenres; ++i) genreBox.addItem (genreName ((Genre) i), i + 1);
        genreBox.onChange = [this]
        {
            following = false;
            genre = (Genre) juce::jlimit (0, numGenres - 1, genreBox.getSelectedId() - 1);
            styleId.clear();
            fillStyles();
            rebuild();
        };
        styleBox.onChange = [this]
        {
            following = false;
            styleId = currentStyle();
            rebuild();
        };
        addAndMakeVisible (genreBox);
        addAndMakeVisible (styleBox);

        followBtn.setTooltip ("Learn the genre and style of the song on the Song page");
        followBtn.onClick = [this] { followSong(); };
        addChildComponent (followBtn);
        useBtn.setTooltip ("Make this the genre and style of your song (Song page)");
        useBtn.onClick = [this]
        {
            proc.song.form.genre = (int) genre;
            proc.song.form.styleId = styleId;
            following = true;
            rebuild();
            if (onUseForSong) onUseForSong();
        };
        addAndMakeVisible (useBtn);

        // THE LESSON / COACH MY SONG
        for (auto* t : { &lessonTab, &coachTab })
        {
            t->setComponentID ("tab");
            t->textHeight = 12.5f;
            addAndMakeVisible (*t);
        }
        lessonTab.setTooltip ("What the genre or style is, and how it goes");
        coachTab.setTooltip ("Tips for your own song, from what its style usually does");
        lessonTab.onClick = [this] { showCoach (false); };
        coachTab.onClick = [this] { showCoach (true); };
        lessonTab.setToggleState (true, juce::dontSendNotification);
        undoBtn.setTooltip ("Take back the last FIX IT");
        undoBtn.onClick = [this] { coach.undo(); };
        undoBtn.setEnabled (false);
        addChildComponent (undoBtn);
        coach.onLayoutChanged = [this] { layoutCoach(); };
        coach.onUndoChanged = [this] { undoBtn.setEnabled (coach.canUndo()); };
        coach.onSongChanged = [this] { if (onSongChanged) onSongChanged(); };
        coach.onShowSection = [this] (int section) { if (onShowSection) onShowSection (section); };
        coach.onShowPart = [this] (const juce::String& role, int section) { if (onShowPart) onShowPart (role, section); };

        view.setViewedComponent (&lesson, false);
        view.setScrollBarsShown (true, false);
        view.setScrollBarThickness (10);
        addAndMakeVisible (view);
        followSong();
        startTimerHz (2);
    }

    MasterclassPage::~MasterclassPage()   { stopTimer(); }

    void MasterclassPage::visibilityChanged()
    {
        if (isVisible() && following) followSong();
        if (isVisible() && coaching) coach.refresh();       // (the song may have changed on another page)
    }

    void MasterclassPage::timerCallback()
    {
        // your library's picks show once the libraries are read
        if (isVisible() && proc.catalog.generation() != seenCatalog)
        {
            rebuild();
            if (coaching) coach.refresh();
        }
    }

    void MasterclassPage::showCoach (bool on)
    {
        coaching = on;
        lessonTab.setToggleState (! on, juce::dontSendNotification);
        coachTab.setToggleState (on, juce::dontSendNotification);
        genreBox.setVisible (! on);
        styleBox.setVisible (! on);
        useBtn.setVisible (! on);
        followBtn.setVisible (! on && ! following);
        undoBtn.setVisible (on);
        view.setViewedComponent (on ? static_cast<juce::Component*> (&coach) : &lesson, false);
        resized();
        if (on) coach.refresh();
        else rebuild();
        repaint();
    }

    void MasterclassPage::layoutCoach()
    {
        coach.layout (view.getWidth() - view.getScrollBarThickness() - 4);    // (room for the scroll bar, shown or not)
    }

    void MasterclassPage::followSong()
    {
        following = true;
        const auto brief = proc.song.makeBrief();
        genre = brief.genre;
        styleId = brief.styleId;
        genreBox.setSelectedId ((int) genre + 1, juce::dontSendNotification);
        fillStyles();
        rebuild();
    }

    void MasterclassPage::study (Genre g, const juce::String& id)
    {
        following = false;
        genre = g;
        styleId = id;
        genreBox.setSelectedId ((int) genre + 1, juce::dontSendNotification);
        fillStyles();
        rebuild();
    }

    void MasterclassPage::fillStyles()
    {
        styleBox.clear (juce::dontSendNotification);
        int id = 1, selected = 1;
        for (auto* s : stylesFor (genre))
        {
            styleBox.addItem (s->isGeneral() ? "All of " + genreName (genre) : s->name, id);
            if (s->id == styleId || (styleId.isEmpty() && s->isGeneral())) selected = id;
            ++id;
        }
        styleBox.setSelectedId (selected, juce::dontSendNotification);
    }

    Genre MasterclassPage::currentGenre() const   { return genre; }

    juce::String MasterclassPage::currentStyle() const
    {
        const auto styles = stylesFor (genre);
        const int i = styleBox.getSelectedId() - 1;
        if (i < 0 || i >= (int) styles.size() || styles[(size_t) i]->isGeneral()) return {};
        return styles[(size_t) i]->id;
    }

    void MasterclassPage::rebuild()
    {
        seenCatalog = proc.catalog.generation();
        const auto guide = styleGuide (genre, styleId);
        auto brief = readBrief ("", genre, styleId);
        const auto songBrief = proc.song.makeBrief();
        if (songBrief.key >= 0) brief.key = songBrief.key;
        const auto plan = planParts (brief);

        std::vector<Lesson::Card> cards;
        auto card = [&cards] (const juce::String& title, juce::StringArray points, bool groove = false, bool full = false)
        {
            points.removeEmptyStrings();
            if (! points.isEmpty() || groove) cards.push_back ({ title, points, groove, full, {} });
        };

        card ("At a glance", { "Tempo: " + guide.tempo, "Feel: " + guide.feel, "Keys: " + guide.keys }, false, true);
        card ("The sounds it uses", guide.sounds);

        // the best match in your own library, part by part
        {
            juce::StringArray picks;
            if (auto cat = proc.catalog.catalog(); cat != nullptr && ! cat->sounds.empty())
            {
                auto pals = makePalettes (*cat, brief, { 1, 0, 1 });
                if (! pals.empty())
                    for (auto& slot : pals[0].slots)
                    {
                        if (! slot.pick.isValid()) { picks.add (slot.part.role + ": nothing in your library yet (" + slot.part.hint.toLowerCase() + ")"); continue; }
                        const auto& s = cat->sounds[(size_t) slot.pick.sound];
                        picks.add (slot.part.role + ": " + s.name + " (" + (s.source == "Plug-in" ? juce::String ("plug-in") : s.instrument) + ")");
                    }
            }
            else
                picks.add (proc.catalog.isBuilding() ? "Reading your libraries..." : "Open the Library page once to read your libraries.");
            card ("In your library", picks);
        }
        card ("Drums", guide.drums, true);
        card ("Bass", guide.bass);
        {
            juce::StringArray chords = guide.harmony;
            for (auto& prog : guide.progressions)
                chords.add (prog.joinIntoString (" - ") + "  =  " + progressionInKey (prog, plan.key) + "  (in " + keyName (plan.key)
                            + (songBrief.key >= 0 ? ", your key)" : ")"));
            card ("Chords", chords);
        }
        card ("Arrangement", guide.arrangement);
        card ("Mix", guide.mix);
        card ("Tips", guide.tips);

        // artists and songs to listen to
        {
            juce::StringArray artists, songs;
            for (auto& r : allReferences())
                for (auto& t : r.takes)
                {
                    const auto* s = styleById (t.styleId);
                    const bool fits = s != nullptr && s->genre == genre && (styleId.isEmpty() || t.styleId == styleId);
                    if (! fits) continue;
                    (r.by.isEmpty() ? artists : songs).addIfNotAlreadyThere (r.label());
                    break;
                }
            while (artists.size() > 16) artists.remove (artists.size() - 1);
            while (songs.size() > 8) songs.remove (songs.size() - 1);
            juce::StringArray listen;
            if (! artists.isEmpty()) listen.add ("Artists: " + artists.joinIntoString (", "));
            if (! songs.isEmpty()) listen.add ("Songs: " + songs.joinIntoString (", "));
            card ("Listen to", listen, false, true);
        }

        lesson.setContent (guide, std::move (cards), guide.name, guide.summary);
        lesson.layout (view.getMaximumVisibleWidth());
        followBtn.setVisible (! following && ! coaching);
        repaint();
    }

    void MasterclassPage::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        drawPanel (g, topPanel);
        auto t = titleArea;
        g.setColour (p.text);
        g.setFont (serif (24.0f));
        g.drawText ("Masterclass", t.removeFromTop (30.0f), juce::Justification::centredLeft);
        g.setColour (p.muted);
        g.setFont (sans (13.0f));
        g.drawText (coaching ? "Tips for the song you're making"
                             : following ? "Following your song. Pick another genre or style to study it."
                                         : "Studying another style. BACK TO MY SONG follows your song again.",
                    statusArea, juce::Justification::centredLeft, true);
    }

    void MasterclassPage::resized()
    {
        auto r = getLocalBounds().toFloat().reduced (24.0f, 0.0f).withTrimmedBottom (16.0f);
        topPanel = r.removeFromTop (84.0f);
        {
            auto in = topPanel.reduced (20.0f, 14.0f);
            // (a narrow window: the boxes and buttons a little smaller, so the tabs keep their room)
            const float k = juce::jlimit (0.6f, 1.0f, (in.getWidth() - 300.0f) / 826.0f);
            if (coaching)
                undoBtn.setBounds (in.removeFromRight (120.0f).withSizeKeepingCentre (120.0f, 40.0f).toNearestInt());
            else
            {
                useBtn.setBounds (in.removeFromRight (190.0f * k).withSizeKeepingCentre (190.0f * k, 40.0f).toNearestInt());
                in.removeFromRight (10.0f);
                followBtn.setBounds (in.removeFromRight (180.0f * k).withSizeKeepingCentre (180.0f * k, 40.0f).toNearestInt());
                in.removeFromRight (16.0f);
                styleBox.setBounds (in.removeFromRight (230.0f * k).withSizeKeepingCentre (230.0f * k, 36.0f).toNearestInt());
                in.removeFromRight (10.0f);
                genreBox.setBounds (in.removeFromRight (190.0f * k).withSizeKeepingCentre (190.0f * k, 36.0f).toNearestInt());
            }
            in.removeFromRight (12.0f);
            titleArea = in;
            // under the title: THE LESSON and COACH MY SONG, then what the page is doing
            auto tabs = in.withTrimmedTop (30.0f);
            lessonTab.setBounds (tabs.removeFromLeft (124.0f).withSizeKeepingCentre (124.0f, 28.0f).toNearestInt());
            tabs.removeFromLeft (8.0f);
            coachTab.setBounds (tabs.removeFromLeft (156.0f).withSizeKeepingCentre (156.0f, 28.0f).toNearestInt());
            statusArea = tabs.withTrimmedLeft (14.0f);
        }
        r.removeFromTop (14.0f);
        view.setBounds (r.toNearestInt());
        lesson.layout (view.getMaximumVisibleWidth());
        if (coaching) layoutCoach();
    }
}
