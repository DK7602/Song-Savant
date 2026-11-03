#include "PianoRollPage.h"
#include "PluginProcessor.h"
#include "core/Paths.h"

namespace ss::ui
{
    namespace
    {
        constexpr float keysW = 64.0f, rulerH = 34.0f, velH = 58.0f, barThickness = 10.0f, velGap = 8.0f;
        constexpr int lowest = 24, highest = 96;                       // C1 .. C7

        juce::String noteName (int n)
        {
            static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
        }

        bool inScale (int note, int key)
        {
            static const int major[] = { 0, 2, 4, 5, 7, 9, 11 }, minor[] = { 0, 2, 3, 5, 7, 8, 10 };
            const int d = ((note - key % 12) % 12 + 12) % 12;
            for (int i = 0; i < 7; ++i)
                if ((key >= 12 ? minor[i] : major[i]) == d) return true;
            return false;
        }

        struct SnapChoice { const char* name; double beats; };
        const SnapChoice snapChoices[] = { { "Bar", 4.0 }, { "1/2", 2.0 }, { "1/4 (a beat)", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 },
                                           { "1/32", 0.125 }, { "1/8 triplet", 1.0 / 3.0 }, { "1/16 triplet", 1.0 / 6.0 }, { "Off", 0.0 } };

        juce::String snapName (double beats)
        {
            for (auto& s : snapChoices)
                if (std::abs (s.beats - beats) < 1.0e-6) return juce::String (s.name).upToFirstOccurrenceOf (" (", false, false).toUpperCase();
            return "OFF";
        }

        Kind kindForPart (const ArrangementPart& p)
        {
            if (p.type == PartType::drums) return Kind::drums;
            if (p.type == PartType::bass) return Kind::bass;
            return p.instrument;
        }

        bool editable (const ArrangementPart& p)    { return p.type == PartType::bass || p.type == PartType::chords || p.type == PartType::pad || p.type == PartType::hook; }

        int centreFor (PartType t)
        {
            switch (t)
            {
                case PartType::bass:   return 40;
                case PartType::chords: return 62;
                case PartType::pad:    return 60;
                case PartType::hook:   return 70;
                case PartType::drums:
                case PartType::none:   break;
            }
            return 60;
        }
    }

    //==============================================================================
    // The keyboard, the bar ruler, the notes and the velocity lane, with scrolling and zoom. The rows and lines are
    // drawn once into a picture (again only when the view changes); the notes and the playhead go on top.
    class PianoRollPage::Grid : public juce::Component, private juce::ScrollBar::Listener
    {
    public:
        explicit Grid (PianoRollPage& p) : page (p)
        {
            for (auto* b : { &hbar, &vbar })
            {
                b->setAutoHide (false);
                b->addListener (this);
                addAndMakeVisible (b);
            }
            setWantsKeyboardFocus (true);
            setMouseClickGrabsKeyboardFocus (true);
        }

        // after opening a part: fit the section across, and show where its notes are
        void reset (bool fit)
        {
            layoutAreas();
            const double beats = page.editor.visibleBars() * 4.0;
            if (fit && notes.getWidth() > 10.0f) pxPerBeat = juce::jlimit (8.0, 220.0, (double) notes.getWidth() / beats);
            scrollBeat = fit ? 0.0 : juce::jlimit (0.0, juce::jmax (0.0, beats - visibleBeats()), scrollBeat);
            int lo = 128, hi = -1;
            for (auto& e : page.editor.notes()) { lo = juce::jmin (lo, e.n.note); hi = juce::jmax (hi, e.n.note); }
            const int centre = hi >= lo ? (lo + hi) / 2 : (page.part() != nullptr ? centreFor (page.part()->type) : 60);
            topPitch = juce::jlimit (lowest + visibleRows() - 1, highest, centre + visibleRows() / 2);
            updateBars();
            invalidate();
        }

        void invalidate()                   { bgValid = false; repaint(); }

        void setPlayhead (double beats)
        {
            if (std::abs (beats - playhead) < 1.0e-4) return;
            const auto strip = [this] (double b) { return b < 0.0 ? juce::Rectangle<int>()
                                                                  : juce::Rectangle<int> ((int) beatX (b) - 8, 0, 16, getHeight()); };
            repaint (strip (playhead));
            playhead = beats;
            repaint (strip (playhead));
        }

        void zoom (double factor, float anchorX)
        {
            const double anchor = xBeat (anchorX);
            pxPerBeat = juce::jlimit (8.0, 220.0, pxPerBeat * factor);
            scrollBeat = anchor - (anchorX - notes.getX()) / pxPerBeat;
            clampScroll();
            updateBars();
            invalidate();
        }

        float zoomCentre() const            { return notes.getCentreX(); }
        double mouseBeat = -1.0;            // where the mouse is over the notes (a paste goes to that bar)

        void resized() override
        {
            layoutAreas();
            hbar.setBounds (juce::Rectangle<float> (notes.getX(), (float) getHeight() - barThickness, notes.getWidth(), barThickness).toNearestInt());
            vbar.setBounds (juce::Rectangle<float> (notes.getRight(), notes.getY(), barThickness, notes.getHeight()).toNearestInt());
            clampScroll();
            updateBars();
            bgValid = false;
        }

        void paint (juce::Graphics& g) override
        {
            const auto& p = pal();
            const auto* part = page.part();
            if (part == nullptr) return;
            if (! bgValid) drawBackground();
            g.drawImageAt (bg, (int) notes.getX(), (int) notes.getY());

            const double total = page.editor.visibleBars() * 4.0;
            const auto colour = kindColour (kindForPart (*part));
            {
                juce::Graphics::ScopedSaveState clip (g);
                g.reduceClipRegion (notes.toNearestInt());
                // the other parts, faint
                if (page.ghosts)
                    for (size_t i = 0; i < page.ghostNotes.size() && i < page.ghostColours.size(); ++i)
                    {
                        const auto r = noteRect (page.ghostNotes[i]);
                        if (! r.intersects (notes)) continue;
                        g.setColour (page.ghostColours[i].withAlpha (0.12f));
                        g.fillRoundedRectangle (r, 3.0f);
                        g.setColour (page.ghostColours[i].withAlpha (0.45f));
                        g.drawRoundedRectangle (r, 3.0f, 1.0f);
                    }
                // the notes
                for (auto& e : page.editor.notes())
                {
                    if (e.n.start >= total - 1.0e-6) continue;              // past the section's end (kept, not shown)
                    const auto r = noteRect (e.n);
                    if (! r.intersects (notes)) continue;
                    const float shade = 0.55f + 0.45f * (float) e.n.velocity / 127.0f;
                    g.setColour ((e.selected ? colour.brighter (0.55f) : colour).withMultipliedBrightness (e.selected ? 1.0f : shade));
                    g.fillRoundedRectangle (r, 3.0f);
                    g.setColour (e.selected ? juce::Colours::white : colour.brighter (0.6f).withAlpha (0.8f));
                    g.drawRoundedRectangle (r, 3.0f, e.selected ? 1.6f : 0.8f);
                    if (r.getWidth() > 30.0f && rowH >= 12.0f)
                    {
                        g.setColour (juce::Colour (0xff141040));
                        g.setFont (sans (juce::jmin (11.0f, rowH - 3.0f), true));
                        g.drawText (noteName (e.n.note), r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, false);
                    }
                    // the end you can drag
                    if (r.getWidth() > 14.0f)
                    {
                        g.setColour (juce::Colour (0xff141040).withAlpha (0.35f));
                        g.fillRect (r.getRight() - 5.0f, r.getY() + 3.0f, 2.0f, r.getHeight() - 6.0f);
                    }
                }
                if (! box.isEmpty())
                {
                    g.setColour (p.text.withAlpha (0.08f));
                    g.fillRect (box);
                    g.setColour (p.text.withAlpha (0.8f));
                    const float dashes[] = { 5.0f, 4.0f };
                    juce::Path outline;
                    outline.addRectangle (box);
                    juce::Path dashed;
                    juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
                    g.fillPath (dashed);
                }
                if (playhead >= 0.0)
                {
                    g.setColour (p.gold);
                    g.fillRect ((float) beatX (playhead) - 1.0f, notes.getY(), 2.5f, notes.getHeight());
                }
            }
            drawRuler (g);
            drawKeys (g);
            drawVelocity (g, colour);
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const auto pos = e.position;
            mouseBeat = notes.contains (pos) ? xBeat (pos.x) : -1.0;
            auto shape = juce::MouseCursor::NormalCursor;
            if (keys.contains (pos)) shape = juce::MouseCursor::PointingHandCursor;
            else if (notes.contains (pos))
            {
                const int i = page.editor.noteAt (xBeat (pos.x), yPitch (pos.y));
                if (i >= 0) shape = page.editor.nearEnd (i, xBeat (pos.x), 7.0 / pxPerBeat) ? juce::MouseCursor::LeftRightResizeCursor
                                                                                             : juce::MouseCursor::DraggingHandCursor;
                else if (page.tool == Tool::draw) shape = juce::MouseCursor::CrosshairCursor;
            }
            else if (vel.contains (pos)) shape = juce::MouseCursor::UpDownResizeCursor;
            setMouseCursor (shape);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            auto& ed = page.editor;
            const auto pos = e.position;
            drag = Drag::none;
            if (keys.contains (pos))
            {
                heard = yPitch (pos.y);
                page.preview (heard);
                drag = Drag::keys;
                return;
            }
            if (vel.contains (pos))
            {
                velNote = nearestStart (pos.x);
                if (velNote >= 0)
                {
                    ed.beginDrag();
                    ed.dragVelocity (velNote, velocityAt (pos.y));
                    drag = Drag::velocity;
                    repaint();
                }
                return;
            }
            if (! notes.contains (pos)) return;
            const double beat = xBeat (pos.x);
            const int pitch = yPitch (pos.y);
            const int i = ed.noteAt (beat, pitch);
            if (e.mods.isPopupMenu() || page.tool == Tool::erase)
            {
                if (i >= 0) { ed.erase (i); page.edited(); }
                drag = Drag::erase;
                return;
            }
            downBeat = beat;
            downPitch = pitch;
            if (i >= 0)
            {
                if (! ed.notes()[(size_t) i].selected || e.mods.isShiftDown()) ed.select (i, e.mods.isShiftDown());
                grabbed = i;
                ed.beginDrag();
                drag = ed.nearEnd (i, beat, 7.0 / pxPerBeat) ? Drag::resize : Drag::move;
                heard = ed.notes()[(size_t) i].n.note;
                if (drag == Drag::move) page.preview (heard);
            }
            else if (page.tool == Tool::select || e.mods.isCommandDown() || e.mods.isShiftDown())
            {
                if (! e.mods.isShiftDown()) ed.selectNone();
                downPos = pos;
                drag = Drag::box;
            }
            else
            {
                grabbed = ed.addNote (beat, pitch);
                page.edited();
                heard = pitch;
                page.preview (pitch);
                downBeat = ed.notes()[(size_t) grabbed].n.start;
                ed.beginDrag();
                drag = Drag::move;
            }
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            auto& ed = page.editor;
            const auto pos = e.position;
            switch (drag)
            {
                case Drag::keys:
                    if (const int pitch = yPitch (pos.y); pitch != heard) { heard = pitch; page.preview (pitch); }
                    break;
                case Drag::velocity:
                    ed.dragVelocity (velNote, velocityAt (pos.y));
                    repaint();
                    break;
                case Drag::erase:
                    if (notes.contains (pos))
                        if (const int i = ed.noteAt (xBeat (pos.x), yPitch (pos.y)); i >= 0) { ed.erase (i); page.edited(); }
                    break;
                case Drag::move:
                {
                    double d = xBeat (pos.x) - downBeat;
                    if (ed.snap > 0.0) d = std::round (d / ed.snap) * ed.snap;
                    const int dp = yPitch (pos.y) - downPitch;
                    ed.dragMove (d, dp);
                    if (grabbed >= 0 && grabbed < (int) ed.notes().size())
                        if (const int now = ed.notes()[(size_t) grabbed].n.note; now != heard) { heard = now; page.preview (now); }
                    repaint();
                    break;
                }
                case Drag::resize:
                {
                    double d = xBeat (pos.x) - downBeat;
                    if (ed.snap > 0.0) d = std::round (d / ed.snap) * ed.snap;
                    ed.dragResize (d);
                    repaint();
                    break;
                }
                case Drag::box:
                {
                    box = juce::Rectangle<float> (downPos, pos).getIntersection (notes);
                    ed.selectBox (xBeat (box.getX()), xBeat (box.getRight()), yPitch (box.getBottom() - 1.0f), yPitch (box.getY()), e.mods.isShiftDown());
                    repaint();
                    break;
                }
                case Drag::none:
                    break;
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if ((drag == Drag::move || drag == Drag::resize || drag == Drag::velocity) && page.editor.endDrag()) page.edited();
            drag = Drag::none;
            box = {};
            grabbed = velNote = -1;
            repaint();
        }

        void mouseExit (const juce::MouseEvent&) override   { mouseBeat = -1.0; }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            if (e.mods.isCommandDown())
            {
                zoom (w.deltaY > 0.0f ? 1.15 : 1.0 / 1.15, e.position.x);
                return;
            }
            if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
                scrollBeat -= (double) (std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) * visibleBeats() * 0.5;
            else
                topPitch += juce::roundToInt (w.deltaY * 10.0f);
            clampScroll();
            updateBars();
            invalidate();
        }

    private:
        enum class Drag { none, keys, velocity, erase, move, resize, box };

        void layoutAreas()
        {
            auto r = getLocalBounds().toFloat();
            r.removeFromBottom (barThickness);
            vel = r.removeFromBottom (velH).withTrimmedLeft (keysW).withTrimmedRight (barThickness);
            r.removeFromBottom (velGap);
            ruler = r.removeFromTop (rulerH).withTrimmedLeft (keysW).withTrimmedRight (barThickness);
            keys = r.removeFromLeft (keysW);
            r.removeFromRight (barThickness);
            notes = r;
        }

        double visibleBeats() const         { return notes.getWidth() / pxPerBeat; }
        int visibleRows() const             { return juce::jmax (1, (int) (notes.getHeight() / rowH)); }
        double beatX (double b) const       { return notes.getX() + (b - scrollBeat) * pxPerBeat; }
        double xBeat (float x) const        { return scrollBeat + (x - notes.getX()) / pxPerBeat; }
        float pitchY (int p) const          { return notes.getY() + (float) (topPitch - p) * rowH; }
        int yPitch (float y) const          { return juce::jlimit (lowest, highest, topPitch - (int) std::floor ((y - notes.getY()) / rowH)); }
        int velocityAt (float y) const      { return juce::jlimit (1, 127, juce::roundToInt (1.0f + 126.0f * (vel.getBottom() - 3.0f - y) / (vel.getHeight() - 8.0f))); }

        juce::Rectangle<float> noteRect (const PatternNote& n) const
        {
            return { (float) beatX (n.start) + 0.5f, pitchY (n.note) + 1.0f, juce::jmax (3.0f, (float) (n.length * pxPerBeat) - 1.0f), rowH - 2.0f };
        }

        int nearestStart (float x) const
        {
            int best = -1;
            float bestD = 9.0f;
            const double total = page.editor.visibleBars() * 4.0;
            for (size_t i = 0; i < page.editor.notes().size(); ++i)
            {
                const auto& n = page.editor.notes()[i].n;
                if (n.start >= total) continue;
                const float d = std::abs ((float) beatX (n.start) + 3.0f - x);
                if (d < bestD) { bestD = d; best = (int) i; }
            }
            return best;
        }

        void clampScroll()
        {
            const double beats = page.editor.visibleBars() * 4.0;
            scrollBeat = juce::jlimit (0.0, juce::jmax (0.0, beats - visibleBeats()), scrollBeat);
            topPitch = juce::jlimit (lowest + visibleRows() - 1, highest, topPitch);
        }

        void updateBars()
        {
            const double beats = page.editor.visibleBars() * 4.0;
            hbar.setRangeLimits (0.0, juce::jmax (beats, visibleBeats()), juce::dontSendNotification);
            hbar.setCurrentRange (scrollBeat, visibleBeats(), juce::dontSendNotification);
            vbar.setRangeLimits (0.0, highest - lowest + 1, juce::dontSendNotification);
            vbar.setCurrentRange (highest - topPitch, visibleRows(), juce::dontSendNotification);
        }

        void scrollBarMoved (juce::ScrollBar* b, double start) override
        {
            if (b == &hbar) scrollBeat = start;
            else topPitch = highest - juce::roundToInt (start);
            clampScroll();
            invalidate();
        }

        // the rows (the key's notes brighter, the bar's chord gold) and the lines: drawn once per view
        void drawBackground()
        {
            const auto& p = pal();
            const int w = juce::jmax (1, (int) notes.getWidth()), h = juce::jmax (1, (int) notes.getHeight());
            bg = juce::Image (juce::Image::ARGB, w, h, true);
            juce::Graphics g (bg);
            const bool light = isLightTheme();
            const auto keyRow = light ? juce::Colour (0xfff4efe4) : p.panelHi.withAlpha (0.75f);
            const auto otherRow = light ? juce::Colour (0xffe2dacb) : juce::Colour (0xff0d0a26);
            const int key = page.plan.key;
            const auto& arr = page.proc.song.arrangement;
            std::vector<Chord> chords;
            if (page.section >= 0 && page.section < (int) arr.sections.size()) chords = sectionChords (page.plan, arr.sections[(size_t) page.section]);
            const double total = page.editor.visibleBars() * 4.0;
            const float ox = notes.getX(), oy = notes.getY();
            for (int pitch = topPitch; pitch >= lowest && pitchY (pitch) - oy < (float) h; --pitch)
            {
                const float y = pitchY (pitch) - oy;
                g.setColour (inScale (pitch, key) ? keyRow : otherRow);
                g.fillRect (0.0f, y, (float) w, rowH);
                for (size_t bar = 0; bar < chords.size(); ++bar)
                {
                    const auto pcs = chords[bar].pitchClasses();
                    const int pc = pitch % 12;
                    if (std::find (pcs.begin(), pcs.end(), pc) == pcs.end()) continue;
                    const float x0 = (float) beatX (bar * 4.0) - ox, x1 = (float) beatX ((bar + 1) * 4.0) - ox;
                    g.setColour (p.gold.withAlpha (pc == chords[bar].root % 12 ? 0.2f : 0.1f));
                    g.fillRect (x0, y, x1 - x0, rowH);
                }
                g.setColour (juce::Colours::black.withAlpha (light ? 0.06f : 0.28f));
                g.fillRect (0.0f, y + rowH - 1.0f, (float) w, 1.0f);
                if (pitch % 12 == 0)
                {
                    g.setColour (p.dim.withAlpha (0.45f));
                    g.fillRect (0.0f, y + rowH - 1.0f, (float) w, 1.0f);
                }
            }
            // lines: bars, beats, and the snap when there's room
            const double snap = page.editor.snap;
            const double step = snap > 0.0 && snap * pxPerBeat >= 7.0 ? snap : 1.0;
            for (double b = std::floor (scrollBeat / step) * step; b <= scrollBeat + visibleBeats() + step; b += step)
            {
                const float x = (float) beatX (b) - ox;
                const bool bar = std::abs (std::fmod (b, 4.0)) < 1.0e-6, beat = std::abs (b - std::round (b)) < 1.0e-6;
                g.setColour (p.dim.withAlpha (bar ? 0.7f : beat ? 0.32f : 0.13f));
                g.fillRect (x, 0.0f, bar ? 1.5f : 1.0f, (float) h);
            }
            // after the section's end
            const float end = (float) beatX (total) - ox;
            if (end < (float) w)
            {
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillRect (end, 0.0f, (float) w - end, (float) h);
            }
            bgValid = true;
        }

        void drawRuler (juce::Graphics& g) const
        {
            const auto& p = pal();
            g.setColour (p.panelHi.withAlpha (0.9f));
            g.fillRoundedRectangle (ruler.withTrimmedBottom (3.0f), 6.0f);
            const auto& arr = page.proc.song.arrangement;
            std::vector<Chord> chords;
            if (page.section >= 0 && page.section < (int) arr.sections.size()) chords = sectionChords (page.plan, arr.sections[(size_t) page.section]);
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (ruler.toNearestInt());
            for (int bar = 0; bar < page.editor.visibleBars(); ++bar)
            {
                const float x = (float) beatX (bar * 4.0);
                if (x > ruler.getRight() || x + (float) (4.0 * pxPerBeat) < ruler.getX()) continue;
                g.setColour (p.gold.withAlpha (0.8f));
                g.fillRect (x, ruler.getY() + 3.0f, 1.5f, ruler.getHeight() - 6.0f);
                g.setColour (p.dim);
                g.setFont (sans (10.5f));
                g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 5.0f, ruler.getY() + 2.0f, 40.0f, 14.0f), juce::Justification::centredLeft, false);
                if ((size_t) bar < chords.size())
                {
                    g.setColour (p.gold);
                    g.setFont (sans (13.5f, true));
                    g.drawText (chords[(size_t) bar].name, juce::Rectangle<float> (x + 5.0f, ruler.getY() + 14.0f, (float) (4.0 * pxPerBeat) - 8.0f, 17.0f),
                                juce::Justification::centredLeft, true);
                }
                for (int beat = 1; beat < 4 && pxPerBeat > 14.0; ++beat)
                {
                    g.setColour (p.dim.withAlpha (0.6f));
                    g.fillRect ((float) beatX (bar * 4.0 + beat), ruler.getBottom() - 10.0f, 1.0f, 6.0f);
                }
            }
            if (playhead >= 0.0)
            {
                const float x = (float) beatX (playhead);
                juce::Path marker;
                marker.addTriangle (x - 7.0f, ruler.getY() + 2.0f, x + 7.0f, ruler.getY() + 2.0f, x, ruler.getY() + 12.0f);
                g.setColour (p.gold);
                g.fillPath (marker);
            }
        }

        void drawKeys (juce::Graphics& g) const
        {
            const auto& p = pal();
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (keys.toNearestInt());
            for (int pitch = topPitch; pitch >= lowest && pitchY (pitch) < keys.getBottom(); --pitch)
            {
                const float y = pitchY (pitch);
                const bool black = juce::String ("0101001010100").substring (pitch % 12, pitch % 12 + 1) == "1";
                g.setColour (juce::Colour (0xfff3f0fb));
                g.fillRect (keys.getX(), y, keys.getWidth(), rowH);
                g.setColour (juce::Colour (0xffbdb6dc));
                g.fillRect (keys.getX(), y + rowH - 0.6f, keys.getWidth(), 0.6f);
                if (black)
                {
                    g.setColour (juce::Colour (0xff1a1640));
                    g.fillRoundedRectangle (keys.getX(), y + 1.0f, keys.getWidth() * 0.6f, rowH - 2.0f, 2.0f);
                }
                if (pitch == heard && drag == Drag::keys)
                {
                    g.setColour (p.gold.withAlpha (0.7f));
                    g.fillRect (keys.getX(), y, keys.getWidth(), rowH);
                }
                if (pitch % 12 == 0 || (pitch % 12 == page.plan.key % 12 && rowH >= 12.0f))
                {
                    g.setColour (pitch % 12 == page.plan.key % 12 ? p.goldDeep : juce::Colour (0xff5a5480));
                    g.setFont (sans (10.0f, true));
                    g.drawText (noteName (pitch), juce::Rectangle<float> (keys.getX(), y, keys.getWidth() - 5.0f, rowH), juce::Justification::centredRight, false);
                }
            }
        }

        void drawVelocity (juce::Graphics& g, juce::Colour colour) const
        {
            const auto& p = pal();
            g.setColour (p.panelHi.withAlpha (0.55f));
            g.fillRoundedRectangle (vel, 6.0f);
            g.setColour (p.gold);
            g.setFont (sans (9.5f, true));
            g.drawText ("VELOCITY", juce::Rectangle<float> (0.0f, vel.getY() + 4.0f, keysW - 6.0f, 12.0f), juce::Justification::centredRight, false);
            g.setColour (p.dim);
            g.setFont (sans (9.5f));
            g.drawText ("how hard", juce::Rectangle<float> (0.0f, vel.getY() + 17.0f, keysW - 6.0f, 12.0f), juce::Justification::centredRight, false);
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (vel.toNearestInt());
            const double total = page.editor.visibleBars() * 4.0;
            for (auto& e : page.editor.notes())
            {
                if (e.n.start >= total) continue;
                const float x = (float) beatX (e.n.start) + 3.0f;
                if (x < vel.getX() - 4.0f || x > vel.getRight() + 4.0f) continue;
                const float hgt = (vel.getHeight() - 8.0f) * (float) e.n.velocity / 127.0f;
                const float top = vel.getBottom() - 3.0f - hgt;
                g.setColour (e.selected ? (isLightTheme() ? p.text : juce::Colours::white) : colour);
                g.fillRect (x - 1.0f, top, 2.0f, hgt);
                g.fillEllipse (x - 3.5f, top - 3.5f, 7.0f, 7.0f);
            }
        }

        PianoRollPage& page;
        juce::ScrollBar hbar { false }, vbar { true };
        juce::Rectangle<float> notes, keys, ruler, vel;
        double pxPerBeat = 30.0, scrollBeat = 0.0;
        float rowH = 15.0f;
        int topPitch = 84;
        juce::Image bg;
        bool bgValid = false;
        double playhead = -1.0;
        Drag drag = Drag::none;
        juce::Point<float> downPos;
        juce::Rectangle<float> box;
        double downBeat = 0.0;
        int downPitch = 60, grabbed = -1, velNote = -1, heard = -1;
    };

    //==============================================================================
    PianoRollPage::PianoRollPage (SongSavantProcessor& p) : proc (p)
    {
        grid = std::make_unique<Grid> (*this);
        addAndMakeVisible (*grid);

        for (auto* b : { &drawBtn, &selectBtn, &eraseBtn, &ghostsBtn, &aloneBtn, &bandBtn }) b->setComponentID ("tab");
        drawBtn.setTooltip ("Click to add a note, drag a note to move it, drag its end to stretch it (right-click deletes)");
        selectBtn.setTooltip ("Drag a box around notes to select them (or hold Ctrl with DRAW)");
        eraseBtn.setTooltip ("Click or drag over notes to delete them");
        drawBtn.onClick = [this] { tool = Tool::draw; updateButtons(); };
        selectBtn.onClick = [this] { tool = Tool::select; updateButtons(); };
        eraseBtn.onClick = [this] { tool = Tool::erase; updateButtons(); };
        snapBtn.setTooltip ("What notes line up to as you draw, move and stretch them");
        snapBtn.onClick = [this] { showSnapMenu(); };
        zoomOutBtn.setTooltip ("See more bars (or Ctrl + mouse wheel)");
        zoomInBtn.setTooltip ("Closer in (or Ctrl + mouse wheel)");
        zoomOutBtn.onClick = [this] { grid->zoom (1.0 / 1.4, grid->zoomCentre()); };
        zoomInBtn.onClick = [this] { grid->zoom (1.4, grid->zoomCentre()); };
        ghostsBtn.setTooltip ("Show the other parts faintly behind yours");
        ghostsBtn.onClick = [this] { ghosts = ! ghosts; updateButtons(); grid->repaint(); };
        aloneBtn.setTooltip ("Hear this part on its own");
        bandBtn.setTooltip ("Hear it with the other parts in this section");
        aloneBtn.onClick = [this] { alone = true; proc.setSectionSolo (role); updateButtons(); };
        bandBtn.onClick = [this] { alone = false; proc.setSectionSolo ({}); updateButtons(); };
        playBtn.onClick = [this] { togglePlay(); };
        undoBtn.setTooltip ("Ctrl+Z");
        redoBtn.setTooltip ("Ctrl+Y");
        undoBtn.onClick = [this] { editor.undo(); edited(); };
        redoBtn.onClick = [this] { editor.redo(); edited(); };
        doneBtn.setTooltip ("Back (your changes are kept with the song as you make them)");
        doneBtn.onClick = [this] { if (onDone) onDone(); };
        lengthBtn.setTooltip ("How many bars this section is (every section that plays this pattern)");
        lengthBtn.onClick = [this]
        {
            juce::PopupMenu m;
            const int now = editor.visibleBars();
            for (int b : { 1, 2, 4, 8, 12, 16, 24, 32 }) m.addItem (b, juce::String (b) + (b == 1 ? " bar" : " bars"), true, b == now);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&lengthBtn),
                             [safe = juce::Component::SafePointer<PianoRollPage> (this)] (int r) { if (safe != nullptr && r > 0) safe->setLength (r); });
        };
        differentBtn.setTooltip ("Give one of the sections that share this pattern a pattern of its own");
        differentBtn.onClick = [this] { showDifferentMenu(); };
        startBtn.setTooltip ("Start this pattern again: from Song Savant's suggestion, an empty grid, another section or another part");
        startBtn.onClick = [this] { showStartMenu(); };
        dragChip.setTooltip ("Drag this part of this section into your DAW as MIDI");

        for (auto* b : { &lengthBtn, &differentBtn, &startBtn, &drawBtn, &selectBtn, &eraseBtn, &snapBtn, &zoomOutBtn, &zoomInBtn,
                         &ghostsBtn, &aloneBtn, &bandBtn, &undoBtn, &redoBtn, &doneBtn })
            addAndMakeVisible (b);
        addAndMakeVisible (playBtn);
        addAndMakeVisible (dragChip);
        // the keys (Space, Delete, Ctrl+C...) always reach the piano roll, never a button that was just clicked
        for (auto* c : getChildren()) c->setWantsKeyboardFocus (false);
        grid->setWantsKeyboardFocus (true);
        setWantsKeyboardFocus (true);
        updateButtons();
    }

    PianoRollPage::~PianoRollPage()
    {
        stopTimer();
    }

    const ArrangementPart* PianoRollPage::part() const
    {
        for (auto& p : parts) if (p.role == role) return &p;
        return nullptr;
    }

    juce::String PianoRollPage::patternName() const
    {
        const auto& a = proc.song.arrangement;
        return section >= 0 && section < (int) a.sections.size() ? a.sections[(size_t) section].patternFor (role) : juce::String();
    }

    juce::Rectangle<int> PianoRollPage::gridBounds() const   { return grid->getBounds(); }

    void PianoRollPage::refreshData()
    {
        catalog = proc.catalog.catalog();
        auto* pal = proc.song.currentPalette();
        parts = pal != nullptr ? arrangementParts (*pal, catalog.get()) : std::vector<ArrangementPart>();
        if (pal != nullptr) proc.song.updateArrangement (parts);
        plan = proc.song.partsPlan();
    }

    bool PianoRollPage::open (const juce::String& r, int s)
    {
        refreshData();
        const auto& a = proc.song.arrangement;
        role = r;
        const auto* p = part();
        if (p == nullptr || ! editable (*p) || a.empty()) { role.clear(); section = -1; return false; }
        section = juce::jlimit (0, (int) a.sections.size() - 1, s < 0 ? juce::jmax (0, mainSection (a, role)) : s);
        proc.setSectionSolo (alone ? role : juce::String());
        load();
        buildTabs();
        updateSketch();
        grid->reset (true);
        startTimerHz (15);
        return true;
    }

    void PianoRollPage::load()
    {
        const auto* p = part();
        if (p == nullptr) return;
        const auto& s = proc.song.arrangement.sections[(size_t) section];
        editor.load (proc.song.patternAt (section, *p, generalMidiDrums()), s.bars);
        seenEdits = editor.edits();
        findGhosts();
        // a new note is as long as the part's notes usually are
        std::vector<double> lengths;
        for (auto& e : editor.notes()) lengths.push_back (e.n.length);
        if (! lengths.empty())
        {
            std::nth_element (lengths.begin(), lengths.begin() + (long) lengths.size() / 2, lengths.end());
            editor.lastLength = juce::jlimit (0.25, 8.0, std::round (lengths[lengths.size() / 2] * 4.0) / 4.0);
        }
        else editor.lastLength = p->type == PartType::pad ? 4.0 : p->type == PartType::hook ? 0.5 : 1.0;
        dragChip.setTitle ((role + " - " + patternName()).toUpperCase());
        dragChip.setFile (writeMidi(), juce::String (s.bars) + " bars, " + keyName (plan.key), "Nothing to drag yet");
        updateButtons();
        repaint();
    }

    void PianoRollPage::edited()
    {
        if (editor.edits() == seenEdits) return;
        seenEdits = editor.edits();
        if (const auto* p = part()) proc.song.storePattern (section, *p, editor.pattern());
        updateSketch();
        updateButtons();
        dragChip.setFile (writeMidi(), juce::String (editor.visibleBars()) + " bars, " + keyName (plan.key), "Nothing to drag yet");
        grid->repaint();
        repaint (titleArea.getSmallestIntegerContainer());
    }

    void PianoRollPage::updateSketch()
    {
        if (part() == nullptr) return;
        const auto mine = playPattern (editor.pattern(), editor.visibleBars(), plan.key);
        proc.updateSketch (SongSavantProcessor::SketchMode::section,
                           std::make_shared<const SketchSong> (sketchOfSection (plan, proc.song.arrangement, section, parts, &proc.song.patterns, &mine, role)));
    }

    void PianoRollPage::findGhosts()
    {
        ghostNotes.clear();
        ghostColours.clear();
        for (auto& p : parts)
        {
            if (p.role == role || ! editable (p)) continue;
            const auto c = kindColour (kindForPart (p));
            for (auto& n : sectionNotes (plan, proc.song.arrangement, section, p, generalMidiDrums(), &proc.song.patterns))
            {
                ghostNotes.push_back (n);
                ghostColours.push_back (c);
            }
        }
    }

    void PianoRollPage::preview (int note)
    {
        if (const auto* p = part()) proc.sketch.preview (note, editor.lastVelocity, p->type, p->instrument);
    }

    juce::File PianoRollPage::writeMidi()
    {
        const auto* p = part();
        if (p == nullptr) return {};
        const auto name = p->role + " - " + patternName();
        NamedPart np { p->label + " (" + patternName() + ")", sequenceFromNotes (playPattern (editor.pattern(), editor.visibleBars(), plan.key)) };
        const auto file = paths::tempDir().getChildFile ("Starter Parts").getChildFile (juce::File::createLegalFileName (
            name + " - " + keyName (plan.key) + " " + juce::String (juce::roundToInt (plan.bpm)) + " BPM.mid"));
        return writeMidiFile (plan, { np }, file, {}, editor.visibleBars()) ? file : juce::File();
    }

    void PianoRollPage::buildTabs()
    {
        partTabs.clear();
        sectionTabs.clear();
        for (auto& p : parts)
        {
            if (p.type == PartType::none) continue;
            auto* t = partTabs.add (new PillButton (p.role.toUpperCase() + (proc.song.patterns.hasAny (p.role) ? juce::String (" *") : juce::String()),
                                                    PillButton::Style::subtle));
            t->setComponentID ("tab");
            t->setWantsKeyboardFocus (false);
            t->setToggleState (p.role == role, juce::dontSendNotification);
            if (editable (p))
            {
                const auto r = p.role;
                t->onClick = [this, r] { open (r, section); };
                t->setTooltip (proc.song.patterns.hasAny (p.role) ? "Your own part (*: you changed it somewhere)" : "Edit the " + p.role.toLowerCase());
            }
            else
            {
                t->setEnabled (false);
                t->setTooltip ("Drums come in a later version: for now, change them in Groove Grabber or in your DAW");
            }
            addAndMakeVisible (t);
        }
        const auto& a = proc.song.arrangement;
        for (auto& name : patternNames (a, role))
        {
            const auto uses = sectionsUsing (a, role, name);
            if (uses.empty()) continue;
            const auto& first = a.sections[(size_t) uses.front()];
            const bool own = name != first.name;
            bool plays = false;
            juce::StringArray where;
            for (int i : uses)
            {
                plays = plays || a.sections[(size_t) i].modeFor (role).isNotEmpty();
                int bar = 1;
                for (int k = 0; k < i; ++k) bar += a.sections[(size_t) k].bars;
                where.add ("bar " + juce::String (bar));
            }
            auto* t = sectionTabs.add (new PillButton (name.toUpperCase() + (own ? juce::String (" *") : juce::String()), PillButton::Style::subtle));
            t->setComponentID ("tab");
            t->setWantsKeyboardFocus (false);
            t->setToggleState (name == patternName(), juce::dontSendNotification);
            t->setAlpha (plays ? 1.0f : 0.55f);
            t->setTooltip ((plays ? "Plays at " + where.joinIntoString (", ")
                                  : "The " + role.toLowerCase() + " doesn't play here yet: draw a note to bring it in")
                           + (own ? juce::String (" (* its own pattern)") : juce::String()));
            const int target = uses.front();
            t->onClick = [this, target] { open (role, target); };
            addAndMakeVisible (t);
        }
        lengthBtn.setButtonText (juce::String (editor.visibleBars()) + (editor.visibleBars() == 1 ? " BAR" : " BARS"));
        differentBtn.setEnabled (sectionsUsing (a, role, patternName()).size() > 1);
        resized();
    }

    void PianoRollPage::updateButtons()
    {
        drawBtn.setToggleState (tool == Tool::draw, juce::dontSendNotification);
        selectBtn.setToggleState (tool == Tool::select, juce::dontSendNotification);
        eraseBtn.setToggleState (tool == Tool::erase, juce::dontSendNotification);
        ghostsBtn.setToggleState (ghosts, juce::dontSendNotification);
        aloneBtn.setToggleState (alone, juce::dontSendNotification);
        bandBtn.setToggleState (! alone, juce::dontSendNotification);
        undoBtn.setEnabled (editor.canUndo());
        redoBtn.setEnabled (editor.canRedo());
        snapBtn.setButtonText ("SNAP: " + snapName (editor.snap));
        const bool playing = proc.isSketchPlaying (SongSavantProcessor::SketchMode::section);
        playBtn.setToggleState (playing, juce::dontSendNotification);
        playBtn.setTooltip (playing ? "Stop (Space)" : "Hear this section, looped (Space)");
    }

    void PianoRollPage::togglePlay()
    {
        if (proc.isSketchPlaying (SongSavantProcessor::SketchMode::section)) proc.stopSketch();
        else
        {
            updateSketch();
            proc.playSketch (SongSavantProcessor::SketchMode::section);
        }
        updateButtons();
    }

    void PianoRollPage::timerCallback()
    {
        if (! isVisible()) return;
        const bool playing = proc.isSketchPlaying (SongSavantProcessor::SketchMode::section);
        if (playing != playBtn.getToggleState()) updateButtons();
        grid->setPlayhead (playing ? proc.sketch.getPosition() : -1.0);
    }

    void PianoRollPage::visibilityChanged()
    {
        if (isVisible()) { grabKeyboardFocus(); return; }
        stopTimer();
        if (proc.isSketchPlaying (SongSavantProcessor::SketchMode::section)) proc.stopSketch();
    }

    void PianoRollPage::themeChanged()
    {
        if (part() != nullptr) findGhosts();       // (the parts' colours change with the theme)
        grid->invalidate();
        repaint();
    }

    //==============================================================================
    void PianoRollPage::startFrom (int choice)
    {
        const auto* p = part();
        if (p == nullptr) return;
        const auto& a = proc.song.arrangement;
        const int bars = editor.visibleBars();
        if (choice == 1)
        {
            // Song Savant's suggestion again: it follows NEW IDEA from now on (until you change it)
            auto suggestion = suggestedNotes (plan, a.sections[(size_t) section], *p, generalMidiDrums());
            editor.replaceAll (suggestion);
            proc.song.forgetPattern (section, role);
            seenEdits = editor.edits();
            updateSketch();
            updateButtons();
            dragChip.setFile (writeMidi(), juce::String (bars) + " bars, " + keyName (plan.key), "Nothing to drag yet");
            grid->repaint();
            repaint();
            return;
        }
        if (choice == 2) editor.clear();
        else if (choice >= 200)
        {
            const int k = choice - 200;
            if (k < 0 || k >= (int) parts.size()) return;
            auto notes = sectionNotes (plan, a, section, parts[(size_t) k], generalMidiDrums(), &proc.song.patterns);
            if (notes.empty()) notes = playPattern (proc.song.patternAt (section, parts[(size_t) k], generalMidiDrums()), bars, plan.key);
            editor.replaceAll (notes);
        }
        else if (choice >= 100)
        {
            const auto names = patternNames (a, role);
            const auto name = names[choice - 100];
            const auto uses = sectionsUsing (a, role, name);
            if (uses.empty()) return;
            editor.replaceAll (playPattern (proc.song.patternAt (uses.front(), *p, generalMidiDrums()), bars, plan.key));
        }
        edited();
    }

    void PianoRollPage::makeDifferent (int s)
    {
        const auto* p = part();
        if (p == nullptr) return;
        proc.song.makeDifferent (s, *p, generalMidiDrums());
        open (role, s);
    }

    void PianoRollPage::setLength (int bars)
    {
        const auto* p = part();
        if (p == nullptr) return;
        const bool mine = proc.song.isMine (section, role);
        setPatternBars (proc.song.arrangement, role, patternName(), bars);
        if (mine)
        {
            editor.setVisibleBars (bars);
            proc.song.storePattern (section, *p, editor.pattern());
            seenEdits = editor.edits();
        }
        else load();
        findGhosts();
        updateSketch();
        buildTabs();
        dragChip.setFile (writeMidi(), juce::String (bars) + " bars, " + keyName (plan.key), "Nothing to drag yet");
        grid->reset (true);
    }

    void PianoRollPage::showSnapMenu()
    {
        juce::PopupMenu m;
        for (int i = 0; i < (int) std::size (snapChoices); ++i)
            m.addItem (i + 1, snapChoices[i].name, true, std::abs (snapChoices[i].beats - editor.snap) < 1.0e-6);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&snapBtn), [safe = juce::Component::SafePointer<PianoRollPage> (this)] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            safe->editor.snap = snapChoices[r - 1].beats;
            safe->updateButtons();
            safe->grid->invalidate();
        });
    }

    void PianoRollPage::showStartMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Song Savant's suggestion", true);
        m.addItem (2, "An empty grid (make your own)", true);
        const auto& a = proc.song.arrangement;
        const auto names = patternNames (a, role);
        juce::PopupMenu sections;
        for (int i = 0; i < names.size(); ++i)
            if (names[i] != patternName()) sections.addItem (100 + i, "The " + role.toLowerCase() + " of the " + names[i]);
        if (sections.getNumItems() > 0) m.addSubMenu ("A copy of another section", sections);
        juce::PopupMenu others;
        for (int k = 0; k < (int) parts.size(); ++k)
            if (parts[(size_t) k].role != role && editable (parts[(size_t) k]))
                others.addItem (200 + k, "The " + parts[(size_t) k].role.toLowerCase() + "'s notes here");
        if (others.getNumItems() > 0) m.addSubMenu ("A copy of another part", others);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&startBtn), [safe = juce::Component::SafePointer<PianoRollPage> (this)] (int r)
        {
            if (safe != nullptr && r > 0) safe->startFrom (r);
        });
    }

    void PianoRollPage::showDifferentMenu()
    {
        const auto& a = proc.song.arrangement;
        const auto uses = sectionsUsing (a, role, patternName());
        if (uses.size() < 2) return;
        juce::PopupMenu m;
        m.addSectionHeader ("Which " + patternName() + " gets its own " + role.toLowerCase() + "?");
        for (int i : uses)
        {
            int bar = 1;
            for (int k = 0; k < i; ++k) bar += a.sections[(size_t) k].bars;
            m.addItem (i + 1, "The " + a.sections[(size_t) i].name + " at bar " + juce::String (bar));
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&differentBtn), [safe = juce::Component::SafePointer<PianoRollPage> (this)] (int r)
        {
            if (safe != nullptr && r > 0) safe->makeDifferent (r - 1);
        });
    }

    bool PianoRollPage::keyPressed (const juce::KeyPress& key)
    {
        using KP = juce::KeyPress;
        const auto cmd = juce::ModifierKeys::commandModifier;
        auto& ed = editor;
        if (key == KP::deleteKey || key == KP::backspaceKey) { ed.eraseSelected(); edited(); return true; }
        if (key == KP ('a', cmd, 0)) { ed.selectAll(); grid->repaint(); return true; }
        if (key == KP ('c', cmd, 0)) { proc.noteClip = ed.copySelected(); return true; }
        if (key == KP ('x', cmd, 0)) { proc.noteClip = ed.copySelected(); ed.eraseSelected(); edited(); return true; }
        if (key == KP ('v', cmd, 0)) { ed.paste (proc.noteClip, grid->mouseBeat); edited(); return true; }
        if (key == KP ('d', cmd, 0)) { ed.duplicateSelected(); edited(); return true; }
        if (key == KP ('z', cmd, 0)) { ed.undo(); edited(); return true; }
        if (key == KP ('y', cmd, 0) || key == KP ('z', cmd | juce::ModifierKeys::shiftModifier, 0)) { ed.redo(); edited(); return true; }
        if (key == KP::upKey || key == KP::downKey)
        {
            const int by = (key.getKeyCode() == KP::upKey ? 1 : -1) * (key.getModifiers().isShiftDown() ? 12 : 1);
            ed.transposeSelected (by);
            for (auto& e : ed.notes()) if (e.selected) { preview (e.n.note); break; }
            edited();
            return true;
        }
        if (key == KP::spaceKey) { togglePlay(); return true; }
        if (key == KP::escapeKey)
        {
            if (ed.numSelected() > 0) { ed.selectNone(); grid->repaint(); }
            else if (onDone) onDone();
            return true;
        }
        return false;
    }

    //==============================================================================
    void PianoRollPage::paint (juce::Graphics& g)
    {
        const auto& p = pal();
        const auto* pt = part();
        if (pt == nullptr) return;
        g.setColour (p.gold);
        g.setFont (sans (12.0f, true));
        auto top = topLine;
        g.drawText ("PART", top.removeFromLeft (52.0f), juce::Justification::centredLeft, false);
        auto second = topLine.translated (0.0f, 44.0f);
        g.drawText ("SECTION", second.removeFromLeft (70.0f), juce::Justification::centredLeft, false);

        // what's open, and whether it's yours
        auto t = titleArea;
        const auto& a = proc.song.arrangement;
        juce::StringArray where;
        for (int i : sectionsUsing (a, role, patternName()))
        {
            int bar = 1;
            for (int k = 0; k < i; ++k) bar += a.sections[(size_t) k].bars;
            where.add (juce::String (bar) + "-" + juce::String (bar + a.sections[(size_t) i].bars - 1));
        }
        g.setColour (p.text);
        g.setFont (serif (19.0f));
        const auto title = pt->label + " in the " + patternName();
        const float tw = juce::GlyphArrangement::getStringWidth (serif (19.0f), title) + 14.0f;
        g.drawText (title, t.removeFromLeft (juce::jmin (tw, t.getWidth() * 0.55f)), juce::Justification::centredLeft, true);
        g.setColour (p.muted);
        g.setFont (sans (12.5f));
        const bool mine = proc.song.isMine (section, role);
        g.drawText ("bars " + where.joinIntoString (", ") + "  |  " + keyName (plan.key) + "  |  " + juce::String (juce::roundToInt (plan.bpm)) + " BPM  |  "
                        + (mine ? juce::String ("your pattern") : juce::String ("Song Savant's suggestion: change anything to make it yours")),
                    t, juce::Justification::centredLeft, true);

        g.setColour (p.dim);
        g.setFont (sans (12.0f));
        g.drawText ("Click: add a note  |  drag: move  |  drag its end: longer / shorter  |  right-click: delete  |  Ctrl + drag: select  |  "
                    "Ctrl+C / V / D: copy, paste, duplicate  |  Ctrl+Z: undo  |  arrows: up / down  |  Space: play",
                    hintArea, juce::Justification::centredLeft, true);
    }

    void PianoRollPage::resized()
    {
        auto r = getLocalBounds().toFloat().reduced (24.0f, 0.0f).withTrimmedBottom (10.0f);
        // the parts, then DONE
        auto row1 = r.removeFromTop (44.0f);
        topLine = row1.withSizeKeepingCentre (row1.getWidth(), 36.0f);
        doneBtn.setBounds (row1.removeFromRight (110.0f).withSizeKeepingCentre (110.0f, 36.0f).toNearestInt());
        row1.removeFromRight (10.0f);
        dragChip.setBounds (row1.removeFromRight (250.0f).toNearestInt());
        row1.removeFromRight (10.0f);
        row1 = row1.withSizeKeepingCentre (row1.getWidth(), 36.0f);
        row1.removeFromLeft (52.0f);
        for (auto* t : partTabs)
        {
            const float w = juce::GlyphArrangement::getStringWidth (sans (13.0f, true), t->getButtonText()) + 30.0f;
            t->setBounds (row1.removeFromLeft (w).toNearestInt());
            row1.removeFromLeft (6.0f);
        }
        r.removeFromTop (4.0f);
        // the sections, then the length, a section of its own, start again
        auto row2 = r.removeFromTop (36.0f);
        startBtn.setBounds (row2.removeFromRight (130.0f).toNearestInt());
        row2.removeFromRight (8.0f);
        differentBtn.setBounds (row2.removeFromRight (190.0f).toNearestInt());
        row2.removeFromRight (8.0f);
        lengthBtn.setBounds (row2.removeFromRight (100.0f).toNearestInt());
        row2.removeFromRight (12.0f);
        row2.removeFromLeft (70.0f);
        for (auto* t : sectionTabs)
        {
            const float w = juce::GlyphArrangement::getStringWidth (sans (13.0f, true), t->getButtonText()) + 30.0f;
            if (w > row2.getWidth()) { t->setBounds ({}); continue; }
            t->setBounds (row2.removeFromLeft (w).toNearestInt());
            row2.removeFromLeft (6.0f);
        }
        r.removeFromTop (8.0f);
        // the tools; play, alone / with the band, undo, redo; the drag chip
        auto row3 = r.removeFromTop (46.0f);
        // a narrow window: everything on this row a little smaller
        const float need = 3 * 88.0f + 8.0f + 136.0f + 86.0f + 102.0f + 44.0f + 6.0f + 84.0f + 140.0f + 12.0f + 84.0f + 84.0f + 24.0f;
        const float k = juce::jlimit (0.7f, 1.0f, row3.getWidth() / need);
        auto place = [&row3, k] (juce::Component& c, float w, bool fromRight, float h = 34.0f)
        {
            auto area = fromRight ? row3.removeFromRight (w * k) : row3.removeFromLeft (w * k);
            c.setBounds (area.withSizeKeepingCentre (w * k, h).toNearestInt());
        };
        place (redoBtn, 78.0f, true);
        row3.removeFromRight (6.0f);
        place (undoBtn, 78.0f, true);
        row3.removeFromRight (12.0f * k);
        place (bandBtn, 140.0f, true);
        place (aloneBtn, 84.0f, true);
        row3.removeFromRight (6.0f);
        place (playBtn, 44.0f, true, 44.0f);
        for (auto* b : { &drawBtn, &selectBtn, &eraseBtn })
        {
            place (*b, 84.0f, false);
            row3.removeFromLeft (4.0f);
        }
        row3.removeFromLeft (8.0f * k);
        place (snapBtn, 130.0f, false);
        row3.removeFromLeft (6.0f * k);
        place (zoomOutBtn, 40.0f, false);
        place (zoomInBtn, 40.0f, false);
        row3.removeFromLeft (6.0f * k);
        place (ghostsBtn, 96.0f, false);
        r.removeFromTop (4.0f);
        titleArea = r.removeFromTop (30.0f);
        hintArea = r.removeFromBottom (20.0f);
        r.removeFromBottom (4.0f);
        grid->setBounds (r.toNearestInt());
    }
}
