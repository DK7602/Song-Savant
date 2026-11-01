#include "LibraryPage.h"
#include "PluginProcessor.h"

namespace ss::ui
{
    namespace
    {
        juce::String countText (int n)   { return withCommas (n); }
    }

    //==============================================================================
    void KindList::setCounts (std::vector<std::pair<Kind, int>> c)
    {
        counts = std::move (c);
        setSize (getWidth(), preferredHeight());
        repaint();
    }

    void KindList::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        int total = 0;
        for (auto& [k, n] : counts) total += n;
        auto drawRow = [&] (int i, const juce::String& name, int n, bool on, juce::Colour dot)
        {
            auto r = juce::Rectangle<float> (0.0f, (float) (i * rowH), (float) getWidth(), (float) rowH).reduced (0.0f, 2.0f);
            if (on || i == hover)
            {
                g.setColour (on ? p.purple.withAlpha (0.28f) : p.panelHi.withAlpha (0.6f));
                g.fillRoundedRectangle (r, 7.0f);
            }
            g.setColour (dot);
            g.fillEllipse (r.getX() + 8.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (on ? p.text : p.muted);
            g.setFont (sans (14.0f, on));
            g.drawText (name, r.withTrimmedLeft (24.0f).withTrimmedRight (60.0f), juce::Justification::centredLeft, true);
            g.setColour (p.dim);
            g.setFont (sans (12.5f));
            g.drawText (countText (n), r.withTrimmedRight (8.0f), juce::Justification::centredRight);
        };
        drawRow (0, "All kinds", total, selected.empty(), p.gold);
        for (size_t i = 0; i < counts.size(); ++i)
            drawRow ((int) i + 1, kindName (counts[i].first), counts[i].second, selected.count (counts[i].first) > 0, kindColour (counts[i].first));
    }

    void KindList::mouseDown (const juce::MouseEvent& e)
    {
        const int row = e.y / rowH;
        if (row == 0) selected.clear();
        else if (row - 1 < (int) counts.size())
        {
            const auto k = counts[(size_t) row - 1].first;
            if (e.mods.isCommandDown() || e.mods.isShiftDown())
            {
                if (! selected.erase (k)) selected.insert (k);
            }
            else if (selected.size() == 1 && selected.count (k)) selected.clear();
            else selected = { k };
        }
        repaint();
        if (onChange) onChange();
    }

    //==============================================================================
    LibraryPage::LibraryPage (SongSavantProcessor& p) : proc (p)
    {
        kindsView.setViewedComponent (&kinds, false);
        kindsView.setScrollBarsShown (true, false);
        kindsView.setScrollBarThickness (8);
        addAndMakeVisible (kindsView);
        kinds.onChange = [this] { filter.kinds = kinds.selected; applyFilter(); };

        addAndMakeVisible (instrumentBox);
        instrumentBox.onChange = [this]
        {
            filter.instrument = instrumentBox.getSelectedId() > 1 ? instrumentBox.getText() : juce::String();
            updateCounts();
            applyFilter();
        };

        for (auto* t : { &ownedToggle, &yoursToggle })
        {
            t->setClickingTogglesState (true);
            addAndMakeVisible (t);
        }
        ownedToggle.setToggleState (true, juce::dontSendNotification);
        ownedToggle.setTooltip ("Hide Analog Lab sounds that show a buy button");
        ownedToggle.onClick = [this] { filter.ownedOnly = ownedToggle.getToggleState(); updateCounts(); applyFilter(); };
        yoursToggle.onClick = [this] { filter.yoursOnly = yoursToggle.getToggleState(); updateCounts(); applyFilter(); };

        addAndMakeVisible (refreshBtn);
        refreshBtn.onClick = [this] { proc.startCatalog(); repaint(); };

        searchBox.setTextToShowWhenEmpty ("Search your sounds: warm piano, country, Retrologue, pad...", pal().dim);
        searchBox.setFont (sans (16.0f));
        searchBox.setIndents (14, 11);
        searchBox.onTextChange = [this] { filter.text = searchBox.getText(); applyFilter(); };
        searchBox.onEscapeKey = [this] { searchBox.clear(); filter.text.clear(); applyFilter(); };
        addAndMakeVisible (searchBox);

        list.setRowHeight (50);
        list.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, pal().purple);
        addAndMakeVisible (list);

        addAndMakeVisible (copyBtn);
        copyBtn.onClick = [this] { copySelected(); };

        refreshCatalog();
        startTimerHz (8);
    }

    LibraryPage::~LibraryPage()   { stopTimer(); }

    void LibraryPage::timerCallback()
    {
        if (proc.catalog.generation() != seenGeneration) refreshCatalog();
        refreshBtn.setEnabled (! proc.catalog.isBuilding() && ! proc.catalog.isCheckingPlugins());
        if (proc.catalog.isBuilding() || proc.catalog.isCheckingPlugins()) repaint (sourcesArea.toNearestInt());
    }

    void LibraryPage::refreshCatalog()
    {
        seenGeneration = proc.catalog.generation();
        catalog = proc.catalog.catalog();

        const auto current = instrumentBox.getText();
        instrumentBox.clear (juce::dontSendNotification);
        instrumentBox.addItem ("All instruments", 1);
        if (catalog != nullptr)
        {
            int id = 2;
            for (auto& name : catalog->instruments()) instrumentBox.addItem (name, id++);
        }
        int pick = 1;
        for (int i = 0; i < instrumentBox.getNumItems(); ++i)
            if (instrumentBox.getItemText (i) == current) pick = instrumentBox.getItemId (i);
        instrumentBox.setSelectedId (pick, juce::dontSendNotification);
        filter.instrument = pick > 1 ? current : juce::String();

        updateCounts();
        applyFilter();
    }

    void LibraryPage::updateCounts()
    {
        std::map<Kind, int> n;
        if (catalog != nullptr)
            for (auto& s : catalog->sounds)
            {
                if (filter.ownedOnly && ! s.owned) continue;
                if (filter.yoursOnly && ! s.yours) continue;
                if (filter.instrument.isNotEmpty() && ! s.instrument.equalsIgnoreCase (filter.instrument)) continue;
                n[s.kind]++;
            }
        std::vector<std::pair<Kind, int>> c;
        for (auto k : allKinds())
            if (n[k] > 0) c.push_back ({ k, n[k] });
        kinds.setCounts (c);
        for (auto it = kinds.selected.begin(); it != kinds.selected.end();)
            it = n[*it] > 0 ? std::next (it) : kinds.selected.erase (it);
        filter.kinds = kinds.selected;
        resized();
    }

    void LibraryPage::applyFilter()
    {
        const auto* before = selectedSound();
        const juce::String beforeKey = before != nullptr ? before->name + "|" + before->instrument + "|" + before->library : juce::String();
        rows = catalog != nullptr ? findSounds (*catalog, filter) : std::vector<int>();
        list.updateContent();
        int keep = rows.empty() ? -1 : 0;
        for (size_t i = 0; i < rows.size() && beforeKey.isNotEmpty(); ++i)
        {
            const auto& s = catalog->sounds[(size_t) rows[i]];
            if (s.name + "|" + s.instrument + "|" + s.library == beforeKey) { keep = (int) i; break; }
        }
        list.selectRow (keep);
        copyBtn.setEnabled (selectedSound() != nullptr);
        list.repaint();
        repaint();
    }

    const Sound* LibraryPage::selectedSound() const
    {
        const int r = list.getSelectedRow();
        if (catalog == nullptr || r < 0 || r >= (int) rows.size()) return nullptr;
        return &catalog->sounds[(size_t) rows[(size_t) r]];
    }

    void LibraryPage::copySelected()
    {
        if (auto* s = selectedSound())
        {
            juce::SystemClipboard::copyTextToClipboard (nameToCopy (*s));
            copiedNote = "Copied \"" + nameToCopy (*s) + "\". " + whereToPaste (*s);
            repaint();
        }
    }

    void LibraryPage::selectRow (int row)                       { list.selectRow (row); }

    void LibraryPage::themeChanged()
    {
        searchBox.setTextToShowWhenEmpty ("Search your sounds: warm piano, country, Retrologue, pad...", pal().dim);
        searchBox.applyColourToAllText (pal().text, true);
        list.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, pal().purple);
        list.repaint();
        repaint();
    }
    void LibraryPage::setSearchText (const juce::String& t)    { searchBox.setText (t, true); filter.text = t; applyFilter(); }

    //==============================================================================
    void LibraryPage::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
    {
        if (catalog == nullptr || row < 0 || row >= (int) rows.size()) return;
        const auto& p = pal();
        const auto& s = catalog->sounds[(size_t) rows[(size_t) row]];
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
        auto right = text.removeFromRight (150.0f);
        g.setColour (s.owned ? p.text : p.dim);
        g.setFont (sans (15.5f, true));
        g.drawText (s.name, text.withTrimmedBottom (text.getHeight() * 0.48f), juce::Justification::bottomLeft, true);
        g.setColour (p.muted);
        g.setFont (sans (13.0f));
        juce::String sub = s.instrument;
        if (s.library.isNotEmpty() && s.library != s.instrument) sub << "  |  " << s.library;
        if (s.origin.isNotEmpty()) sub << "  |  " << s.origin;
        g.drawText (sub, text.withTrimmedTop (text.getHeight() * 0.55f), juce::Justification::topLeft, true);

        g.setColour (kindColour (s.kind).withAlpha (0.9f));
        g.setFont (sans (12.5f, true));
        g.drawText ((s.source == "Plug-in" ? "Plug-in: " + kindName (s.kind) : s.detail.isNotEmpty() ? s.detail : kindName (s.kind)).toUpperCase(), right.withTrimmedBottom (right.getHeight() * 0.48f),
                    juce::Justification::bottomRight, true);
        juce::String badge = ! s.owned ? "NOT OWNED" : s.yours ? "YOURS" : juce::String();
        if (badge.isNotEmpty())
        {
            g.setColour (! s.owned ? p.warn : p.good);
            g.setFont (sans (11.5f, true));
            g.drawText (badge, right.withTrimmedTop (right.getHeight() * 0.55f), juce::Justification::topRight);
        }
    }

    //==============================================================================
    void LibraryPage::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        for (auto r : { leftPanel, centerPanel, rightPanel }) drawPanel (g, r);

        g.setColour (p.text);
        g.setFont (serif (21.0f));
        g.drawText ("Kinds of sound", kindsTitle, juce::Justification::centredLeft);
        g.setFont (serif (17.0f));
        g.drawText ("Instrument", instrumentTitle, juce::Justification::centredLeft);

        // where the sounds came from
        {
            juce::String s;
            if (proc.catalog.isBuilding()) s = "Reading your libraries...";
            else if (catalog == nullptr) s = "Your libraries haven't been read yet.";
            else
                for (auto& src : catalog->sources)
                {
                    s << src.name << ": ";
                    if (src.problem.isNotEmpty() && src.sounds == 0) s << "couldn't read it (" << src.problem << ")";
                    else
                    {
                        s << withCommas (src.sounds - src.notOwned) << " sounds";
                        if (src.notOwned > 0) s << " (+" << withCommas (src.notOwned) << " not owned)";
                        if (src.leftOut > 0) s << " (" << withCommas (src.leftOut) << " old ones left out)";
                    }
                    s << "\n";
                }
            if (proc.catalog.isCheckingPlugins())
            {
                const auto [done, total] = proc.catalog.pluginCheckProgress();
                s << "Finding out what your other plug-ins are: " << juce::jmin (done + 1, total) << " of " << total << "\n";
            }
            drawParagraph (g, s.trimEnd(), sourcesArea, sans (12.5f), p.dim);
        }

        g.setColour (p.muted);
        g.setFont (sans (13.5f));
        juce::String count = catalog == nullptr ? juce::String() : withCommas ((int) rows.size()) + (rows.size() == 1 ? " sound" : " sounds");
        if (rows.size() >= 5000) count = "Showing the first 5,000: type to narrow it down";
        g.drawText (count, countLine, juce::Justification::centredLeft);
        if (catalog != nullptr && rows.empty())
        {
            g.setColour (p.dim);
            g.setFont (sans (15.0f));
            g.drawText (catalog->sounds.empty() ? "No sounds found yet: click REFRESH LIBRARY." : "Nothing matches. Try fewer words or another kind.",
                        list.getBounds().toFloat(), juce::Justification::centred);
        }

        paintDetails (g, detailsArea);
    }

    void LibraryPage::paintDetails (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto& p = pal();
        const auto* s = selectedSound();
        if (s == nullptr)
        {
            g.setColour (p.dim);
            g.setFont (sans (15.0f));
            g.drawFittedText ("Pick a sound to see where it lives and how to load it.", r.toNearestInt(), juce::Justification::centred, 3);
            return;
        }
        auto nameH = paragraphHeight (s->name, r.getWidth(), serif (27.0f));
        drawParagraph (g, s->name, r.removeFromTop (nameH), serif (27.0f), p.text);
        r.removeFromTop (4.0f);
        g.setColour (p.gold);
        g.setFont (sans (16.0f, true));
        g.drawText (s->instrument, r.removeFromTop (24.0f), juce::Justification::centredLeft, true);

        auto line = [&] (const juce::String& label, const juce::String& value)
        {
            if (value.isEmpty()) return;
            const auto text = label + ": " + value;
            const auto h = paragraphHeight (text, r.getWidth(), sans (14.0f));
            juce::AttributedString a;
            a.append (label + ": ", sans (14.0f, true), p.muted);
            a.append (value, sans (14.0f), p.text);
            a.setWordWrap (juce::AttributedString::byWord);
            juce::TextLayout tl;
            tl.createLayout (a, r.getWidth());
            tl.draw (g, r.removeFromTop (h + 2.0f));
            r.removeFromTop (4.0f);
        };
        r.removeFromTop (6.0f);
        line ("Kind", kindName (s->kind) + (s->detail.isNotEmpty() && s->detail != kindName (s->kind) ? " (" + s->detail + ")" : juce::String())
                       + (s->guessed ? "  (guessed from its name)" : juce::String()));
        line ("Library", s->library);
        line ("From", s->origin);
        line ("Styles", s->styles.joinIntoString (", "));
        line ("Sound", s->character.joinIntoString (", "));
        if (s->rating > 0) line ("Rating", juce::String::repeatedString (juce::String::charToString ((juce::juce_wchar) 0x2605), s->rating));
        if (s->description.isNotEmpty())
        {
            r.removeFromTop (4.0f);
            const auto h = juce::jmin (90.0f, paragraphHeight (s->description, r.getWidth(), sans (14.0f)));
            drawParagraph (g, s->description, r.removeFromTop (h), sans (14.0f), p.muted);
            r.removeFromTop (6.0f);
        }

        // how to load it, above the copy button
        auto bottom = r.removeFromBottom (copyBtn.getHeight() + (copiedNote.isNotEmpty() ? 30.0f : 6.0f));
        if (copiedNote.isNotEmpty())
        {
            g.setColour (p.good);
            g.setFont (sans (13.0f));
            g.drawFittedText (copiedNote, bottom.removeFromBottom (24.0f).toNearestInt(), juce::Justification::centredLeft, 2);
        }
        const auto how = howToLoad (*s);
        const auto hh = paragraphHeight (how, r.getWidth(), sans (13.5f));
        auto howArea = r.removeFromBottom (juce::jmin (r.getHeight(), hh + 26.0f));
        g.setColour (s->owned ? p.gold : p.warn);
        g.setFont (sans (13.0f, true));
        g.drawText ("HOW TO LOAD IT", howArea.removeFromTop (22.0f), juce::Justification::centredLeft);
        drawParagraph (g, how, howArea, sans (13.5f), p.muted);
    }

    void LibraryPage::resized()
    {
        auto r = getLocalBounds().toFloat().reduced (24.0f, 0.0f).withTrimmedBottom (16.0f);
        const float gap = 18.0f;
        leftPanel = r.removeFromLeft (250.0f);
        r.removeFromLeft (gap);
        rightPanel = r.removeFromRight (juce::jlimit (300.0f, 380.0f, r.getWidth() * 0.34f));
        r.removeFromRight (gap);
        centerPanel = r;

        {
            auto in = leftPanel.reduced (16.0f, 18.0f);
            kindsTitle = in.removeFromTop (30.0f);
            in.removeFromTop (6.0f);
            refreshBtn.setBounds (in.removeFromBottom (40.0f).toNearestInt());
            in.removeFromBottom (8.0f);
            sourcesArea = in.removeFromBottom (80.0f);
            in.removeFromBottom (8.0f);
            yoursToggle.setBounds (in.removeFromBottom (32.0f).toNearestInt());
            in.removeFromBottom (6.0f);
            ownedToggle.setBounds (in.removeFromBottom (32.0f).toNearestInt());
            in.removeFromBottom (10.0f);
            instrumentBox.setBounds (in.removeFromBottom (36.0f).toNearestInt());
            in.removeFromBottom (4.0f);
            instrumentTitle = in.removeFromBottom (26.0f);
            in.removeFromBottom (8.0f);
            kindsView.setBounds (in.toNearestInt());
            kinds.setSize (kindsView.getWidth() - (kinds.preferredHeight() > kindsView.getHeight() ? 10 : 0), kinds.preferredHeight());
        }
        {
            auto in = centerPanel.reduced (16.0f, 18.0f);
            searchBox.setBounds (in.removeFromTop (46.0f).toNearestInt());
            in.removeFromTop (6.0f);
            countLine = in.removeFromTop (24.0f);
            list.setBounds (in.toNearestInt());
        }
        {
            auto in = rightPanel.reduced (22.0f, 22.0f);
            copyBtn.setBounds (in.removeFromBottom (44.0f).toNearestInt());
            detailsArea = in.withTrimmedBottom (-44.0f);   // the details drawing leaves room for the button itself
        }
    }
}
