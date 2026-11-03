#include "PatternEdit.h"
#include <algorithm>
#include <cmath>

namespace ss
{
    namespace
    {
        constexpr double tiny = 1.0e-6;
        constexpr double shortest = 1.0 / 32.0;                // the shortest note: a 128th
        constexpr size_t maxUndo = 200;
    }

    void PatternEditor::load (const Pattern& p, int visibleBars)
    {
        pat = p;
        visible = juce::jlimit (1, 64, visibleBars);
        if (pat.bars < visible) pat = unrolled (pat, visible);
        items.clear();
        for (auto& n : pat.notes) items.push_back ({ n, false });
        undos.clear();
        redos.clear();
        dragging = false;
        ++editCount;
    }

    Pattern PatternEditor::pattern() const
    {
        Pattern p = pat;
        p.notes.clear();
        for (auto& i : items) p.notes.push_back (i.n);
        sortNotes (p.notes);
        return p;
    }

    void PatternEditor::setVisibleBars (int bars)
    {
        visible = juce::jlimit (1, 64, bars);
        if (pat.bars >= visible) return;
        // the pattern repeats into the new bars: they're yours to change from here
        auto p = pattern();
        p = unrolled (p, visible);
        pat.bars = p.bars;
        items.clear();
        for (auto& n : p.notes) items.push_back ({ n, false });
        ++editCount;
    }

    double PatternEditor::snapNearest (double beats) const
    {
        return snap > 0.0 ? std::round (beats / snap) * snap : beats;
    }

    double PatternEditor::snapDown (double beats) const
    {
        return snap > 0.0 ? std::floor (beats / snap + tiny) * snap : beats;
    }

    int PatternEditor::noteAt (double beat, int pitch) const
    {
        for (int i = (int) items.size() - 1; i >= 0; --i)
        {
            const auto& n = items[(size_t) i].n;
            if (n.note == pitch && beat >= n.start - tiny && beat < n.start + n.length) return i;
        }
        return -1;
    }

    bool PatternEditor::nearEnd (int index, double beat, double tolerance) const
    {
        if (index < 0 || index >= (int) items.size()) return false;
        const auto& n = items[(size_t) index].n;
        return beat >= n.start + n.length - juce::jmin (tolerance, n.length * 0.4);
    }

    void PatternEditor::checkpoint()
    {
        undos.push_back (items);
        if (undos.size() > maxUndo) undos.erase (undos.begin());
        redos.clear();
    }

    //==============================================================================
    int PatternEditor::addNote (double beat, int pitch)
    {
        const double total = visible * 4.0;
        double start = juce::jlimit (0.0, total - shortest, snapDown (beat));
        checkpoint();
        for (auto& i : items) i.selected = false;
        PatternNote n;
        n.start = start;
        n.length = juce::jmax (shortest, juce::jmin (lastLength, total - start));
        n.note = juce::jlimit (0, 127, pitch);
        n.velocity = lastVelocity;
        items.push_back ({ n, true });
        changed();
        return (int) items.size() - 1;
    }

    void PatternEditor::erase (int index)
    {
        if (index < 0 || index >= (int) items.size()) return;
        checkpoint();
        items.erase (items.begin() + index);
        changed();
    }

    void PatternEditor::eraseSelected()
    {
        if (numSelected() == 0) return;
        checkpoint();
        items.erase (std::remove_if (items.begin(), items.end(), [] (const EditNote& e) { return e.selected; }), items.end());
        changed();
    }

    void PatternEditor::clear()
    {
        if (items.empty()) return;
        checkpoint();
        items.clear();
        changed();
    }

    void PatternEditor::replaceAll (std::vector<PatternNote> notes)
    {
        checkpoint();
        items.clear();
        sortNotes (notes);
        for (auto& n : notes) items.push_back ({ n, false });
        changed();
    }

    void PatternEditor::transposeSelected (int semitones)
    {
        if (numSelected() == 0 || semitones == 0) return;
        checkpoint();
        for (auto& i : items)
            if (i.selected) i.n.note = juce::jlimit (0, 127, i.n.note + semitones);
        changed();
    }

    double PatternEditor::selectionStart() const
    {
        double s = 1.0e9;
        for (auto& i : items) if (i.selected) s = juce::jmin (s, i.n.start);
        return s;
    }

    NoteClip PatternEditor::copySelected() const
    {
        NoteClip c;
        if (numSelected() == 0) return c;
        c.fromBar = std::floor (selectionStart() / 4.0 + tiny) * 4.0;
        for (auto& i : items)
            if (i.selected)
            {
                auto n = i.n;
                n.start -= c.fromBar;
                c.notes.push_back (n);
            }
        sortNotes (c.notes);
        return c;
    }

    void PatternEditor::paste (const NoteClip& clip, double atBar)
    {
        if (clip.empty()) return;
        const double total = visible * 4.0;
        const double at = atBar >= 0.0 ? std::floor (atBar / 4.0 + tiny) * 4.0 : clip.fromBar;
        checkpoint();
        for (auto& i : items) i.selected = false;
        for (auto n : clip.notes)
        {
            n.start += at;
            if (n.start >= total - tiny) continue;                 // past the section's end
            n.length = juce::jmin (n.length, total - n.start);
            items.push_back ({ n, true });
        }
        changed();
    }

    void PatternEditor::duplicateSelected()
    {
        if (numSelected() == 0) return;
        double from = 1.0e9, to = 0.0;
        for (auto& i : items)
            if (i.selected)
            {
                from = juce::jmin (from, i.n.start);
                to = juce::jmax (to, i.n.start + i.n.length);
            }
        // right after it: whole bars for a phrase, whole beats for a few notes
        const double span = to - from, unit = span > 2.0 ? 4.0 : 1.0;
        const double by = std::ceil (span / unit - tiny) * unit;
        const double total = visible * 4.0;
        checkpoint();
        std::vector<EditNote> added;
        for (auto& i : items)
            if (i.selected)
            {
                auto n = i.n;
                n.start += by;
                if (n.start >= total - tiny) continue;
                n.length = juce::jmin (n.length, total - n.start);
                added.push_back ({ n, true });
                i.selected = false;
            }
        for (auto& a : added) items.push_back (a);
        changed();
    }

    //==============================================================================
    // The building tools (1.2.0)
    std::vector<size_t> PatternEditor::targets() const
    {
        std::vector<size_t> out;
        const double total = visible * 4.0;
        for (size_t k = 0; k < items.size(); ++k) if (items[k].selected) out.push_back (k);
        if (out.empty())
            for (size_t k = 0; k < items.size(); ++k) if (items[k].n.start < total - tiny) out.push_back (k);
        return out;
    }

    namespace
    {
        bool inKey (int note, int key)
        {
            static const int major[] = { 0, 2, 4, 5, 7, 9, 11 }, minor[] = { 0, 2, 3, 5, 7, 8, 10 };
            const int d = ((note - key % 12) % 12 + 12) % 12;
            for (int i = 0; i < 7; ++i) if ((key >= 12 ? minor[i] : major[i]) == d) return true;
            return false;
        }

        // notes that start together (a chord): groups of indices, in time order
        std::vector<std::vector<size_t>> chordsOf (const std::vector<EditNote>& items, const std::vector<size_t>& which)
        {
            auto sorted = which;
            std::stable_sort (sorted.begin(), sorted.end(), [&items] (size_t a, size_t b)
            {
                return std::abs (items[a].n.start - items[b].n.start) > 0.01 ? items[a].n.start < items[b].n.start : items[a].n.note < items[b].n.note;
            });
            std::vector<std::vector<size_t>> groups;
            for (auto k : sorted)
            {
                if (groups.empty() || std::abs (items[groups.back().front()].n.start - items[k].n.start) > 0.01) groups.push_back ({});
                groups.back().push_back (k);
            }
            return groups;
        }
    }

    int PatternEditor::stampChord (double beat, int pitch, int rootPitchClass, const std::vector<int>& intervals)
    {
        if (intervals.empty()) return 0;
        const double total = visible * 4.0;
        const double start = juce::jlimit (0.0, total - shortest, snapDown (beat));
        // the chord's root at or just below where you clicked, the rest stacked above it
        const int root = pitch - (((pitch - rootPitchClass) % 12) + 12) % 12;
        checkpoint();
        for (auto& i : items) i.selected = false;
        int added = 0;
        for (int iv : intervals)
        {
            PatternNote n;
            n.start = start;
            n.length = juce::jmax (shortest, juce::jmin (lastLength, total - start));
            n.note = juce::jlimit (0, 127, root + iv);
            n.velocity = lastVelocity;
            items.push_back ({ n, true });
            ++added;
        }
        changed();
        return added;
    }

    void PatternEditor::strum (double perNote, bool up)
    {
        const auto which = targets();
        if (which.empty()) return;
        checkpoint();
        for (auto& g : chordsOf (items, which))
        {
            if (g.size() < 2) continue;
            // a down strum hits the low string first, an up strum the high one
            std::vector<size_t> order = g;
            std::sort (order.begin(), order.end(), [this, up] (size_t a, size_t b) { return up ? items[a].n.note > items[b].n.note : items[a].n.note < items[b].n.note; });
            for (size_t k = 0; k < order.size(); ++k)
            {
                auto& n = items[order[k]].n;
                const double by = perNote * (double) k;
                const double end = n.start + n.length;
                n.start += by;
                n.length = juce::jmax (shortest, end - n.start);
            }
        }
        changed();
    }

    void PatternEditor::arpeggiate (double step, int direction)
    {
        const auto which = targets();
        if (which.empty() || step <= 0.0) return;
        checkpoint();
        std::vector<EditNote> made;
        std::vector<bool> gone (items.size(), false);
        for (auto& g : chordsOf (items, which))
        {
            if (g.size() < 2) continue;
            std::vector<PatternNote> notes;
            double length = 0.0;
            for (auto k : g) { notes.push_back (items[k].n); length = juce::jmax (length, items[k].n.length); gone[k] = true; }
            std::sort (notes.begin(), notes.end(), [] (const PatternNote& a, const PatternNote& b) { return a.note < b.note; });
            std::vector<PatternNote> order = notes;
            if (direction < 0) std::reverse (order.begin(), order.end());
            if (direction == 0 && notes.size() > 2)
                for (size_t k = notes.size() - 2; k >= 1; --k) order.push_back (notes[k]);      // up and back down
            const int steps = juce::jmax (1, (int) std::floor (length / step + tiny));
            for (int s = 0; s < steps; ++s)
            {
                auto n = order[(size_t) s % order.size()];
                n.start = notes.front().start + s * step;
                n.length = step;
                made.push_back ({ n, true });
            }
        }
        std::vector<EditNote> keep;
        for (size_t k = 0; k < items.size(); ++k) if (! gone[k]) keep.push_back (items[k]);
        for (auto& m : made) keep.push_back (m);
        items = keep;
        changed();
    }

    void PatternEditor::quantize (double grid, double amount)
    {
        const auto which = targets();
        if (which.empty()) return;
        if (grid <= 0.0) grid = 0.25;
        checkpoint();
        const double total = visible * 4.0;
        for (auto k : which)
        {
            auto& n = items[k].n;
            const double to = std::round (n.start / grid) * grid;
            n.start = juce::jlimit (0.0, total - shortest, n.start + (to - n.start) * amount);
        }
        changed();
    }

    void PatternEditor::humanize (double timing, int velocity, juce::uint32 seed)
    {
        const auto which = targets();
        if (which.empty()) return;
        checkpoint();
        juce::Random random ((juce::int64) seed);
        const double total = visible * 4.0;
        for (auto k : which)
        {
            auto& n = items[k].n;
            n.start = juce::jlimit (0.0, total - shortest, n.start + (random.nextDouble() * 2.0 - 1.0) * timing);
            n.velocity = juce::jlimit (1, 127, n.velocity + random.nextInt (juce::Range<int> (-velocity, velocity + 1)));
        }
        changed();
    }

    void PatternEditor::legato()
    {
        const auto which = targets();
        if (which.empty()) return;
        checkpoint();
        const auto groups = chordsOf (items, which);
        for (size_t g = 0; g + 1 < groups.size(); ++g)
        {
            const double next = items[groups[g + 1].front()].n.start;
            for (auto k : groups[g]) items[k].n.length = juce::jmax (shortest, next - items[k].n.start);
        }
        changed();
    }

    void PatternEditor::moveInKey (int steps, int key)
    {
        const auto which = targets();
        if (which.empty() || steps == 0) return;
        checkpoint();
        for (auto k : which)
        {
            int n = items[k].n.note;
            for (int s = 0; s < std::abs (steps); ++s)
            {
                do n += steps > 0 ? 1 : -1;
                while (! inKey (n, key) && n > 0 && n < 127);
            }
            items[k].n.note = juce::jlimit (0, 127, n);
        }
        changed();
    }

    void PatternEditor::chop (double piece)
    {
        const auto which = targets();
        if (which.empty() || piece <= 0.0) return;
        checkpoint();
        std::vector<EditNote> made;
        std::vector<bool> gone (items.size(), false);
        for (auto k : which)
        {
            const auto& n = items[k].n;
            const int parts = (int) std::round (n.length / piece);
            if (parts < 2) continue;
            gone[k] = true;
            for (int p = 0; p < parts; ++p)
            {
                auto m = n;
                m.start = n.start + p * (n.length / parts);
                m.length = n.length / parts;
                made.push_back ({ m, items[k].selected });
            }
        }
        std::vector<EditNote> keep;
        for (size_t k = 0; k < items.size(); ++k) if (! gone[k]) keep.push_back (items[k]);
        for (auto& m : made) keep.push_back (m);
        items = keep;
        changed();
    }

    void PatternEditor::flipTime()
    {
        const auto which = targets();
        if (which.size() < 2) return;
        double from = 1.0e9, to = 0.0;
        for (auto k : which) { from = juce::jmin (from, items[k].n.start); to = juce::jmax (to, items[k].n.start + items[k].n.length); }
        checkpoint();
        for (auto k : which)
        {
            auto& n = items[k].n;
            n.start = from + (to - (n.start + n.length));
        }
        changed();
    }

    void PatternEditor::flipPitch (int key)
    {
        const auto which = targets();
        if (which.size() < 2) return;
        int lo = 127, hi = 0;
        for (auto k : which) { lo = juce::jmin (lo, items[k].n.note); hi = juce::jmax (hi, items[k].n.note); }
        checkpoint();
        for (auto k : which)
        {
            int n = lo + hi - items[k].n.note;
            while (! inKey (n, key) && n > 0) --n;                  // into the key
            items[k].n.note = juce::jlimit (0, 127, n);
        }
        changed();
    }

    //==============================================================================
    void PatternEditor::select (int index, bool addToSelection)
    {
        if (! addToSelection) for (auto& i : items) i.selected = false;
        if (index >= 0 && index < (int) items.size()) items[(size_t) index].selected = true;
    }

    void PatternEditor::selectNone()
    {
        for (auto& i : items) i.selected = false;
    }

    void PatternEditor::selectAll()
    {
        const double total = visible * 4.0;
        for (auto& i : items) i.selected = i.n.start < total - tiny;
    }

    void PatternEditor::selectBox (double beatA, double beatB, int pitchA, int pitchB, bool addToSelection)
    {
        const double b0 = juce::jmin (beatA, beatB), b1 = juce::jmax (beatA, beatB);
        const int p0 = juce::jmin (pitchA, pitchB), p1 = juce::jmax (pitchA, pitchB);
        for (auto& i : items)
        {
            const bool inside = i.n.note >= p0 && i.n.note <= p1 && i.n.start < b1 && i.n.start + i.n.length > b0;
            i.selected = inside || (addToSelection && i.selected);
        }
    }

    int PatternEditor::numSelected() const
    {
        int n = 0;
        for (auto& i : items) if (i.selected) ++n;
        return n;
    }

    //==============================================================================
    void PatternEditor::beginDrag()
    {
        dragFrom = items;
        dragging = true;
    }

    void PatternEditor::dragMove (double beats, int semitones)
    {
        if (! dragging) beginDrag();
        const double total = visible * 4.0;
        // keep the whole selection inside the section
        double lo = 1.0e9, hi = -1.0e9;
        int nlo = 127, nhi = 0;
        for (auto& i : dragFrom)
            if (i.selected)
            {
                lo = juce::jmin (lo, i.n.start);
                hi = juce::jmax (hi, i.n.start);
                nlo = juce::jmin (nlo, i.n.note);
                nhi = juce::jmax (nhi, i.n.note);
            }
        if (lo > hi) return;
        beats = juce::jlimit (-lo, total - shortest - hi, beats);
        semitones = juce::jlimit (-nlo, 127 - nhi, semitones);
        for (size_t k = 0; k < items.size() && k < dragFrom.size(); ++k)
            if (dragFrom[k].selected)
            {
                items[k].n.start = dragFrom[k].n.start + beats;
                items[k].n.note = dragFrom[k].n.note + semitones;
                items[k].n.length = juce::jmin (dragFrom[k].n.length, total - items[k].n.start);
            }
    }

    void PatternEditor::dragResize (double beats)
    {
        if (! dragging) beginDrag();
        const double total = visible * 4.0;
        for (size_t k = 0; k < items.size() && k < dragFrom.size(); ++k)
            if (dragFrom[k].selected)
            {
                const auto& n = dragFrom[k].n;
                items[k].n.length = juce::jlimit (shortest, total - n.start, n.length + beats);
            }
        for (size_t k = 0; k < items.size(); ++k)
            if (items[k].selected) { lastLength = items[k].n.length; break; }
    }

    void PatternEditor::dragVelocity (int index, int velocity)
    {
        if (! dragging) beginDrag();
        if (index < 0 || index >= (int) items.size() || index >= (int) dragFrom.size()) return;
        velocity = juce::jlimit (1, 127, velocity);
        // the others keep their differences: they all move by the same amount from where they started
        const int by = velocity - dragFrom[(size_t) index].n.velocity;
        const bool all = items[(size_t) index].selected;
        for (size_t k = 0; k < items.size() && k < dragFrom.size(); ++k)
            if ((int) k == index || (all && items[k].selected)) items[k].n.velocity = juce::jlimit (1, 127, dragFrom[k].n.velocity + by);
        lastVelocity = velocity;
    }

    bool PatternEditor::endDrag()
    {
        if (! dragging) return false;
        dragging = false;
        bool same = dragFrom.size() == items.size();
        for (size_t k = 0; same && k < items.size(); ++k) same = dragFrom[k].n == items[k].n;
        if (same) return false;
        undos.push_back (dragFrom);
        if (undos.size() > maxUndo) undos.erase (undos.begin());
        redos.clear();
        changed();
        return true;
    }

    void PatternEditor::undo()
    {
        if (undos.empty()) return;
        redos.push_back (items);
        items = undos.back();
        undos.pop_back();
        changed();
    }

    void PatternEditor::redo()
    {
        if (redos.empty()) return;
        undos.push_back (items);
        items = redos.back();
        redos.pop_back();
        changed();
    }
}
