#include "CoachView.h"
#include "PluginProcessor.h"

namespace ss::ui
{
    namespace
    {
        constexpr float pad = 18.0f, gap = 14.0f, buttonH = 32.0f;
        constexpr int fewTips = 5;
        juce::Font titleFont()   { return sans (16.5f, true); }
        juce::Font bodyFont()    { return sans (14.0f); }
        juce::Font tagFont()     { return sans (11.5f, true); }

        float textWidth (const juce::String& t, const juce::Font& f)   { return juce::GlyphArrangement::getStringWidth (f, t); }
        const char* howItReads = "Song Savant reads your sections, chords and notes and compares them with what your style usually does. "
                                 "It can't hear your recording or your mix, and these are the usual ways, not rules: if you broke one on purpose, click I MEANT THAT.";
    }

    CoachView::CoachView (SongSavantProcessor& p) : proc (p)
    {
        allBtn.onClick = [this]
        {
            showAll = ! showAll;
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<CoachView> (this)] { if (safe != nullptr) safe->refresh(); });
        };
        backBtn.setTooltip ("The tips you put away with I MEANT THAT, back in the list");
        backBtn.onClick = [this]
        {
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<CoachView> (this)] { if (safe != nullptr) safe->bringBack(); });
        };
        addChildComponent (allBtn);
        addChildComponent (backBtn);
    }

    int CoachView::shownTips() const
    {
        return showAll ? (int) report.tips.size() : juce::jmin (fewTips, (int) report.tips.size());
    }

    void CoachView::refresh()
    {
        // something else changed the song since the last fix: its UNDO would throw that away, so it goes
        if (! undos.empty() && ! coach::sameSong (coach::snapshot (proc.song), undos.back().after))
        {
            undos.clear();
            note.clear();
            if (onUndoChanged) onUndoChanged();
        }
        const auto catalog = proc.catalog.catalog();
        report = coach::coachSong (proc.song, catalog.get());
        rebuildRows();
        if (onLayoutChanged) onLayoutChanged();
        else layout (getWidth());
        repaint();
    }

    void CoachView::rebuildRows()
    {
        rows.clear();
        for (auto& t : report.tips)
        {
            auto row = std::make_unique<Row>();
            row->tip = t;
            const auto id = t.id;
            auto later = [this, id] (std::function<void (CoachView&, int)> action)
            {
                // (after the click: the buttons are made again)
                juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<CoachView> (this), id, action]
                {
                    if (safe == nullptr) return;
                    for (int i = 0; i < (int) safe->report.tips.size(); ++i)
                        if (safe->report.tips[(size_t) i].id == id) { action (*safe, i); return; }
                });
            };
            if (t.section >= 0)
            {
                row->show = std::make_unique<PillButton> ("SHOW ME", PillButton::Style::outline);
                row->show->setTooltip (t.role.isNotEmpty() ? "Open the " + t.role + " in the piano roll, where this is" : juce::String ("Show it on ARRANGE"));
                row->show->onClick = [later] { later ([] (CoachView& v, int i) { v.showMe (i); }); };
                addChildComponent (*row->show);
            }
            if (t.fixLabel.isNotEmpty())
            {
                row->fixBtn = std::make_unique<PillButton> (t.fixLabel, PillButton::Style::gold);
                row->fixBtn->setTooltip ("FIX IT: Song Savant makes this change for you. UNDO (top right) takes it back.");
                row->fixBtn->onClick = [later] { later ([] (CoachView& v, int i) { v.fix (i); }); };
                addChildComponent (*row->fixBtn);
            }
            row->meant = std::make_unique<PillButton> ("I MEANT THAT", PillButton::Style::subtle);
            row->meant->setTooltip ("You did it on purpose: this tip is put away for this song (BRING BACK shows it again)");
            row->meant->onClick = [later] { later ([] (CoachView& v, int i) { v.dismiss (i); }); };
            addChildComponent (*row->meant);
            rows.push_back (std::move (row));
        }
    }

    int CoachView::layout (int widthIn)
    {
        const float width = (float) juce::jmax (320, widthIn);
        float headH = pad + 30.0f + 22.0f + paragraphHeight (howItReads, width - 2 * pad, sans (13.5f)) + (note.isNotEmpty() ? 26.0f : 0.0f) + pad - 4.0f;
        headArea = { 0.0f, 0.0f, width, headH };
        float y = headH + gap;
        emptyArea = workArea = tipsTitle = {};
        for (auto& r : rows)
        {
            r->area = {};
            for (auto* b : { r->show.get(), r->fixBtn.get(), r->meant.get() }) if (b != nullptr) b->setVisible (false);
        }
        allBtn.setVisible (false);
        backBtn.setVisible (false);

        if (! report.yours || report.empty)
        {
            emptyArea = { 0.0f, y, width, 150.0f };
            y = emptyArea.getBottom();
            setSize ((int) width, (int) std::ceil (y + 8.0f));
            return getHeight();
        }

        // the tips on the left, what's working on the right (one under the other when narrow)
        const bool twoColumns = width >= 900.0f;
        const float tipsW = twoColumns ? std::floor ((width - gap) * 0.63f) : width;
        const float workX = twoColumns ? tipsW + gap : 0.0f, workW = twoColumns ? width - workX : width;
        float ty = y;
        tipsTitle = { 0.0f, ty, tipsW, 28.0f };
        ty += 30.0f;
        const int shown = shownTips();
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            auto& r = *rows[(size_t) i];
            if (i >= shown) continue;
            const float inner = tipsW - 2 * pad;
            float h = pad + 18.0f + paragraphHeight (r.tip.title, inner, titleFont()) + 6.0f
                      + paragraphHeight (r.tip.why, inner, bodyFont()) + 6.0f
                      + paragraphHeight ("Try this: " + r.tip.tryThis, inner, bodyFont()) + 12.0f + buttonH + pad;
            r.area = { 0.0f, ty, tipsW, h };
            r.textArea = r.area.reduced (pad);
            auto buttons = r.area.reduced (pad).removeFromBottom (buttonH);
            if (r.show != nullptr)
            {
                r.show->setBounds (buttons.removeFromLeft (110.0f).toNearestInt());
                r.show->setVisible (true);
                buttons.removeFromLeft (8.0f);
            }
            if (r.meant != nullptr)
            {
                r.meant->setBounds (buttons.removeFromRight (136.0f).toNearestInt());
                r.meant->setVisible (true);
                buttons.removeFromRight (8.0f);
            }
            if (r.fixBtn != nullptr)
            {
                const float want = textWidth (r.tip.fixLabel, sans (13.5f, true)) + 36.0f;
                r.fixBtn->textHeight = 13.5f;
                r.fixBtn->setBounds (buttons.removeFromLeft (juce::jmin (want, buttons.getWidth())).toNearestInt());
                r.fixBtn->setVisible (true);
            }
            ty += h + 10.0f;
        }
        if (report.tips.empty()) ty += 70.0f;        // (a card saying there's nothing to try)
        // SHOW ALL and BRING BACK
        auto more = juce::Rectangle<float> (0.0f, ty, tipsW, 36.0f);
        bool any = false;
        if ((int) report.tips.size() > fewTips)
        {
            allBtn.setButtonText (showAll ? juce::String ("SHOW FEWER") : "SHOW ALL " + juce::String ((int) report.tips.size()) + " TIPS");
            allBtn.setBounds (more.removeFromLeft (200.0f).toNearestInt());
            allBtn.setVisible (true);
            more.removeFromLeft (10.0f);
            any = true;
        }
        if (report.dismissed > 0)
        {
            backBtn.setButtonText ("BRING BACK " + juce::String (report.dismissed) + (report.dismissed == 1 ? " TIP" : " TIPS") + " YOU MEANT");
            backBtn.setBounds (more.removeFromLeft (juce::jmin (290.0f, more.getWidth())).toNearestInt());
            backBtn.setVisible (true);
            any = true;
        }
        if (any) ty += 46.0f;

        // what's working
        float wy = twoColumns ? y : ty + 6.0f;
        float wh = pad + 28.0f;
        for (auto& w : report.working) wh += paragraphHeight (w.text, workW - 2 * pad - 22.0f, bodyFont()) + 8.0f;
        if (report.working.empty()) wh += paragraphHeight ("Fix a few tips and what's working shows up here.", workW - 2 * pad, bodyFont()) + 8.0f;
        wh += pad - 8.0f;
        workArea = { workX, wy, workW, wh };
        const float bottom = juce::jmax (ty, workArea.getBottom());
        setSize ((int) width, (int) std::ceil (bottom + 8.0f));
        return getHeight();
    }

    void CoachView::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        // the head: what's coached, how it reads the song, and what was just done
        {
            drawPanel (g, headArea, 16.0f);
            auto r = headArea.reduced (pad, pad - 4.0f);
            g.setColour (p.text);
            g.setFont (serif (24.0f));
            g.drawText ("Coach my song", r.removeFromTop (30.0f), juce::Justification::centredLeft);
            g.setColour (p.gold);
            g.setFont (sans (14.0f, true));
            g.drawText (report.headline, r.removeFromTop (22.0f), juce::Justification::centredLeft, true);
            const float h = drawParagraph (g, howItReads, r, sans (13.5f), p.muted);
            r.removeFromTop (h);
            if (note.isNotEmpty())
            {
                g.setColour (p.good);
                g.setFont (sans (14.0f, true));
                g.drawText (note, r.removeFromTop (26.0f), juce::Justification::centredLeft, true);
            }
        }

        if (! emptyArea.isEmpty())
        {
            drawPanel (g, emptyArea, 16.0f);
            auto r = emptyArea.reduced (pad);
            g.setColour (p.text);
            g.setFont (sans (17.0f, true));
            g.drawText (report.empty ? "Your song is just getting started" : "Nothing of yours to coach yet", r.removeFromTop (28.0f), juce::Justification::centredLeft);
            drawParagraph (g, report.empty ? juce::String ("Nothing plays yet. Add sections on ARRANGE, switch parts on, or write notes in the piano roll, then come back here.")
                                           : juce::String ("COACH MY SONG looks at songs you've made your own: change the map on ARRANGE (add, move or lengthen sections, switch parts "
                                                           "on and off), write or change notes in the piano roll, pick your own chords, or start a BLANK SONG. Then come back here."),
                           r, bodyFont(), p.muted);
            return;
        }

        // the tips
        if (! tipsTitle.isEmpty())
        {
            g.setColour (p.gold);
            g.setFont (sans (13.5f, true));
            g.drawText (report.tips.empty() ? juce::String ("TRY THIS")
                                            : "TRY THIS  (" + juce::String ((int) report.tips.size()) + (report.tips.size() == 1 ? " TIP" : " TIPS")
                                                + (shownTips() < (int) report.tips.size() ? ", THE MOST USEFUL FIRST)" : ")"),
                        tipsTitle, juce::Justification::centredLeft, true);
        }
        if (report.tips.empty() && ! tipsTitle.isEmpty())
        {
            auto card = juce::Rectangle<float> (tipsTitle.getX(), tipsTitle.getBottom() + 2.0f, tipsTitle.getWidth(), 64.0f);
            drawPanel (g, card, 14.0f);
            drawParagraph (g, report.dismissed > 0 ? juce::String ("Nothing else to try: the rest are tips you said you meant.")
                                                   : "Nothing to try right now: your song follows the usual ways of its style. Change it some more and look again.",
                           card.reduced (pad, 14.0f), bodyFont(), p.text);
        }
        for (auto& rp : rows)
        {
            auto& row = *rp;
            if (row.area.isEmpty()) continue;
            drawPanel (g, row.area, 14.0f);
            auto r = row.textArea;
            g.setColour (p.gold.withAlpha (0.85f));
            g.setFont (tagFont());
            g.drawText (coach::areaName (row.tip.area).toUpperCase(), r.removeFromTop (18.0f), juce::Justification::centredLeft);
            r.removeFromTop (drawParagraph (g, row.tip.title, r, titleFont(), p.text) + 6.0f);
            r.removeFromTop (drawParagraph (g, row.tip.why, r, bodyFont(), p.muted) + 6.0f);
            drawParagraph (g, "Try this: " + row.tip.tryThis, r, bodyFont(), p.text);
        }

        // what's working
        if (! workArea.isEmpty())
        {
            drawPanel (g, workArea, 14.0f);
            auto r = workArea.reduced (pad);
            g.setColour (p.good);
            g.setFont (sans (13.5f, true));
            g.drawText ("WHAT'S WORKING", r.removeFromTop (28.0f), juce::Justification::centredLeft);
            if (report.working.empty())
                drawParagraph (g, "Fix a few tips and what's working shows up here.", r, bodyFont(), p.muted);
            for (auto& w : report.working)
            {
                auto line = r;
                // a tick
                juce::Path tick;
                const float x = line.getX(), ty = line.getY() + 4.0f;
                tick.startNewSubPath (x + 1.0f, ty + 6.0f);
                tick.lineTo (x + 5.5f, ty + 10.5f);
                tick.lineTo (x + 14.0f, ty + 1.0f);
                g.setColour (p.good);
                g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                const float h = drawParagraph (g, w.text, line.withTrimmedLeft (22.0f), bodyFont(), p.text);
                r.removeFromTop (h + 8.0f);
            }
        }
    }

    bool CoachView::fix (int tip)
    {
        if (tip < 0 || tip >= (int) report.tips.size()) return false;
        const auto t = report.tips[(size_t) tip];
        const auto before = coach::snapshot (proc.song);
        const auto catalog = proc.catalog.catalog();
        if (! coach::applyFix (proc.song, catalog.get(), t.fix))
        {
            note = "That's already done.";
            refresh();
            return false;
        }
        undos.push_back ({ before, coach::snapshot (proc.song) });
        if (undos.size() > 50) undos.erase (undos.begin());
        note = "Done: " + t.fixLabel.toLowerCase() + ". UNDO (top right) takes it back.";
        if (onSongChanged) onSongChanged();
        if (onUndoChanged) onUndoChanged();
        refresh();
        return true;
    }

    void CoachView::undo()
    {
        if (undos.empty()) return;
        if (coach::sameSong (coach::snapshot (proc.song), undos.back().after))
        {
            coach::restore (proc.song, undos.back().before);
            note = "Undone.";
            if (onSongChanged) onSongChanged();
        }
        undos.pop_back();
        if (onUndoChanged) onUndoChanged();
        refresh();
    }

    void CoachView::dismiss (int tip)
    {
        if (tip < 0 || tip >= (int) report.tips.size()) return;
        proc.song.coachDismissed.addIfNotAlreadyThere (report.tips[(size_t) tip].id);
        note.clear();
        refresh();
    }

    void CoachView::bringBack()
    {
        proc.song.coachDismissed.clear();
        refresh();
    }

    void CoachView::showMe (int tip)
    {
        if (tip < 0 || tip >= (int) report.tips.size()) return;
        const auto& t = report.tips[(size_t) tip];
        if (t.section < 0) return;
        if (t.role.isNotEmpty() && onShowPart) onShowPart (t.role, t.section);
        else if (onShowSection) onShowSection (t.section);
    }
}
