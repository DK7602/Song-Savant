#include "StarterParts.h"
#include <algorithm>
#include <cmath>
#include <set>
#include "KitMap.h"

namespace ss
{
    namespace
    {
        const int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int minorScale[] = { 0, 2, 3, 5, 7, 8, 10 };
        constexpr int tick16 = ticksPerQuarter / 4;      // a 16th note
        constexpr int barTicks = ticksPerQuarter * 4;

        int mod12 (int n)   { return ((n % 12) + 12) % 12; }

        // what follows the root or numeral: "m7", "maj7", "sus2"... -> intervals over a major or minor triad
        std::vector<int> intervalsFor (bool minor, juce::String suffix, juce::String& shown)
        {
            suffix = suffix.trim();
            const auto s = suffix.toLowerCase();
            std::vector<int> v = minor ? std::vector<int> { 0, 3, 7 } : std::vector<int> { 0, 4, 7 };
            shown = minor ? "m" : "";
            auto add = [&v] (int i) { if (std::find (v.begin(), v.end(), i) == v.end()) v.push_back (i); };
            if (s == "dim" || s == "o" || suffix == juce::String::fromUTF8 ("\xc2\xb0")) { v = { 0, 3, 6 }; shown = "dim"; }
            else if (s == "aug" || s == "+")      { v = { 0, 4, 8 }; shown = "aug"; }
            else if (s == "maj7" || suffix == "M7") { add (11); shown << "maj7"; }
            else if (s == "maj9")                 { add (11); add (14); shown << "maj9"; }
            else if (s == "7")                    { add (10); shown << "7"; }
            else if (s == "9")                    { add (10); add (14); shown << "9"; }
            else if (s == "11")                   { add (10); add (17); shown << "11"; }
            else if (s == "13")                   { add (10); add (21); shown << "13"; }
            else if (s == "6")                    { add (9); shown << "6"; }
            else if (s == "add9" || s == "add2")  { add (14); shown << "add9"; }
            else if (s == "sus2")                 { v = { 0, 2, 7 }; shown = "sus2"; }
            else if (s == "sus4" || s == "sus")   { v = { 0, 5, 7 }; shown = "sus4"; }
            else if (s == "7sus4")                { v = { 0, 5, 7, 10 }; shown = "7sus4"; }
            return v;
        }

        int noteIn (int pitchClass, int low)            // the first note of this pitch class at or above `low`
        {
            return low + mod12 (pitchClass - low);
        }

        // ---- the MIDI being built
        struct Writer
        {
            juce::MidiMessageSequence seq;
            void note (int note, double start, double length, int velocity)
            {
                if (note < 0 || note > 127 || length <= 0) return;
                const auto vel = (juce::uint8) juce::jlimit (1, 127, velocity);
                seq.addEvent (juce::MidiMessage::noteOn (1, note, vel), start);
                seq.addEvent (juce::MidiMessage::noteOff (1, note), start + length);
            }
            void chord (const std::vector<int>& notes, double start, double length, int velocity, int strumTicks = 0, bool upward = true)
            {
                for (size_t i = 0; i < notes.size(); ++i)
                {
                    const auto order = upward ? i : notes.size() - 1 - i;
                    note (notes[order], start + (double) (strumTicks * (int) i), length - strumTicks * (int) i, velocity);
                }
            }
            juce::MidiMessageSequence done()
            {
                seq.sort();
                seq.updateMatchedPairs();
                return seq;
            }
        };

        // where a 16th step lands, with the style's swing on the off-16ths
        double stepTick (int bar, int step, int swing)
        {
            double t = bar * barTicks + (step / 2) * (tick16 * 2);
            if (step % 2 == 1) t += 2.0 * tick16 * juce::jlimit (50, 75, swing) / 100.0;
            return t;
        }

        // a chord's notes in a register, as close as possible to the last chord (voice leading)
        std::vector<int> voice (const Chord& c, const std::vector<int>& previous, int low, int high, size_t maxNotes)
        {
            auto pcs = c.pitchClasses();
            if (pcs.size() > maxNotes && pcs.size() >= 4) pcs.erase (pcs.begin() + 2);   // drop the fifth first
            while (pcs.size() > maxNotes) pcs.pop_back();

            std::vector<int> best;
            double bestCost = 1e9;
            for (size_t rot = 0; rot < pcs.size(); ++rot)
                for (int base = low; base < low + 12; ++base)
                {
                    std::vector<int> v;
                    int n = noteIn (pcs[rot], base);
                    for (size_t i = 0; i < pcs.size(); ++i)
                    {
                        const int pc = pcs[(rot + i) % pcs.size()];
                        if (i > 0) n = noteIn (pc, n + 1);
                        v.push_back (n);
                    }
                    if (v.front() < low || v.back() > high) continue;
                    double cost = 0;
                    if (previous.size() == v.size())
                        for (size_t i = 0; i < v.size(); ++i) cost += std::abs (v[i] - previous[i]);
                    else
                        cost = std::abs ((v.front() + v.back()) / 2.0 - (low + high) / 2.0) * 2.0;
                    cost += (v.back() - v.front()) * 0.1;          // keep it compact
                    if (cost < bestCost) { bestCost = cost; best = v; }
                }
            if (best.empty())
                for (auto pc : pcs) best.push_back (noteIn (pc, low));
            std::sort (best.begin(), best.end());
            return best;
        }

        std::vector<int> scaleOf (const PartsPlan& p, bool pentatonic)
        {
            std::vector<int> s;
            if (pentatonic)
            {
                const int minorPenta[] = { 0, 3, 5, 7, 10 }, majorPenta[] = { 0, 2, 4, 7, 9 };
                for (int i = 0; i < 5; ++i) s.push_back (mod12 (p.tonic() + (p.minor() ? minorPenta[i] : majorPenta[i])));
            }
            else
                for (int i = 0; i < 7; ++i) s.push_back (mod12 (p.tonic() + (p.minor() ? minorScale[i] : majorScale[i])));
            return s;
        }

        // ---- the parts
        juce::MidiMessageSequence drumPart (const PartsPlan& p, const DrumNotes& dn)
        {
            Writer w;
            const auto& g = p.guide.groove;
            juce::Random rng ((juce::int64) p.seed * 7919 + 1);
            // a kit with its own 808 pad (Groove Grabber's), in a style built on the 808: it plays with the kick
            const int pad808 = p.guide.bassStyle.startsWith ("808") ? dn.note ("808") : -1;
            for (int bar = 0; bar < p.bars; ++bar)
            {
                const bool last = bar == p.bars - 1;
                const int offset = g.steps == 32 ? (bar % 2) * 16 : 0;
                for (auto& [lane, pattern] : g.lanes)
                {
                    auto pat = pattern.substring (offset, offset + 16);
                    if (last && g.fill.count (lane) > 0) pat = g.fill.at (lane);
                    // a new idea: now and then an extra kick or a hat roll
                    if (p.seed > 1 && lane == "kick" && bar % 2 == 1 && rng.nextInt (3) == 0)
                    {
                        const int s = 12 + rng.nextInt (4);
                        if (pat[s] == '.') pat = pat.substring (0, s) + "o" + pat.substring (s + 1);
                    }
                    const int noteNum = dn.note (lane);
                    if (noteNum < 0) continue;
                    const int with808 = lane == "kick" ? pad808 : -1;
                    for (int step = 0; step < 16 && step < pat.length(); ++step)
                    {
                        const auto c = pat[step];
                        const double t = stepTick (bar, step, g.swing);
                        if (with808 >= 0 && (c == 'X' || c == 'x' || c == 'o'))
                            w.note (with808, t, 240, c == 'X' ? 112 : c == 'x' ? 98 : 76);
                        switch (c)
                        {
                            case 'X': w.note (noteNum, t, 60, 122); break;
                            case 'x': w.note (noteNum, t, 60, 100); break;
                            case 'o': w.note (noteNum, t, 60, 72); break;
                            case 'g': w.note (noteNum, t, 60, 42); break;
                            case 'r': w.note (noteNum, t, 30, 92); w.note (noteNum, t + tick16 / 2.0, 30, 70); break;
                            case 't': for (int i = 0; i < 3; ++i) w.note (noteNum, t + i * tick16 / 3.0, 25, 80 - i * 6); break;
                            default: break;
                        }
                    }
                }
                // the fill takes over the lanes it names in the last bar; lanes it adds play too
                if (last)
                    for (auto& [lane, pat] : g.fill)
                        if (g.lanes.count (lane) == 0)
                        {
                            const int noteNum = dn.note (lane);
                            for (int step = 0; step < 16 && noteNum >= 0; ++step)
                                if (pat[step] != '.') w.note (noteNum, stepTick (bar, step, g.swing), 60, pat[step] == 'g' ? 42 : 96);
                        }
                if (g.crash && (bar == 0 || bar == p.bars / 2)) w.note (dn.note ("crash"), bar * barTicks, 240, 110);
            }
            return w.done();
        }

        std::vector<int> kickSteps (const PartsPlan& p, int bar)
        {
            std::vector<int> steps;
            const auto& g = p.guide.groove;
            auto it = g.lanes.find ("kick");
            if (it != g.lanes.end())
            {
                const int offset = g.steps == 32 ? (bar % 2) * 16 : 0;
                for (int s = 0; s < 16; ++s)
                    if (it->second[offset + s] != '.' && it->second[offset + s] != 'g') steps.push_back (s);
            }
            if (steps.empty()) steps = { 0, 8 };
            return steps;
        }

        juce::MidiMessageSequence bassPart (const PartsPlan& p)
        {
            Writer w;
            const auto style = p.guide.bassStyle;
            const int swing = p.guide.groove.swing;
            juce::Random rng ((juce::int64) p.seed * 104729 + 2);
            const int low = style.startsWith ("808") ? 26 : 28;      // 808s sit a little lower
            for (int bar = 0; bar < p.bars; ++bar)
            {
                const auto& c = p.chords[(size_t) bar];
                const auto& next = p.chords[(size_t) ((bar + 1) % p.bars)];
                const int root = noteIn (c.root, low);
                const int nextRoot = noteIn (next.root, low);
                const int fifth = root + 7 <= low + 14 ? root + 7 : root - 5;
                const double b = bar * barTicks;

                if (style == "808" || style == "808glide" || style == "rootkick")
                {
                    const auto steps = kickSteps (p, bar);
                    for (size_t i = 0; i < steps.size(); ++i)
                    {
                        const double start = stepTick (bar, steps[i], swing);
                        const double end = i + 1 < steps.size() ? stepTick (bar, steps[i + 1], swing) : (bar + 1) * (double) barTicks;
                        int n = root;
                        bool glide = false;
                        if (style == "808glide" && i > 0 && rng.nextInt (3) == 0) { n = rng.nextBool() ? root + 7 : root + (c.pitchClasses().size() > 1 ? c.intervals[1] : 12); glide = true; }
                        if (style == "808" && bar % 4 == 3 && i + 1 == steps.size() && steps.size() > 1) { n = root + 12; glide = true; }
                        const double len = style == "rootkick" ? std::min (end - start - 20.0, 2.0 * tick16) : end - start - (glide ? -40.0 : 20.0);
                        w.note (n, start, std::max (60.0, len), glide ? 100 : 112);
                    }
                }
                else if (style == "rootfifth")
                {
                    w.note (root, b, 2 * ticksPerQuarter - 40, 100);
                    w.note (fifth, b + 2 * ticksPerQuarter, ticksPerQuarter - 30, 90);
                    // walk into the next chord
                    const int approach = nextRoot + (nextRoot > root ? -1 : 1);
                    w.note (next.root != c.root ? approach : root + 12, b + 3 * ticksPerQuarter, ticksPerQuarter - 30, 85);
                }
                else if (style == "eighths")
                {
                    for (int e = 0; e < 8; ++e)
                    {
                        int n = root;
                        if (bar % 2 == 1 && e == 6) n = root + 12;
                        if (bar % 2 == 1 && e == 7) n = fifth;
                        w.note (n, b + e * 2 * tick16, 2 * tick16 - 30, e % 2 == 0 ? 105 : 85);
                    }
                }
                else if (style == "offbeat")
                {
                    for (int beat = 0; beat < 4; ++beat)
                        w.note (beat == 3 && bar % 2 == 1 ? root + 12 : root, b + beat * ticksPerQuarter + 2 * tick16, tick16 + 50, 105);
                }
                else if (style == "octave")
                {
                    for (int e = 0; e < 8; ++e) w.note (e % 2 == 0 ? root : root + 12, b + e * 2 * tick16, 2 * tick16 - 40, e % 2 == 0 ? 108 : 92);
                }
                else if (style == "funk")
                {
                    const char* pat = "x..x..x...x.x..x";
                    for (int s = 0; s < 16; ++s)
                        if (pat[s] == 'x')
                        {
                            int n = root;
                            if (s == 3 || s == 12) n = root + 12;
                            if (s == 15) n = root + 10 <= root + 12 ? root + 10 : root;
                            w.note (n, stepTick (bar, s, swing), tick16 + 20, s == 0 ? 115 : 92);
                        }
                }
                else if (style == "rnb")
                {
                    w.note (root, b, 5 * tick16, 105);
                    w.note (fifth, stepTick (bar, 6, swing), 2 * tick16, 88);
                    w.note (root, stepTick (bar, 10, swing), 3 * tick16, 92);
                    if (next.root != c.root) w.note (nextRoot + (nextRoot > root ? -1 : 1), stepTick (bar, 14, swing), 2 * tick16 - 20, 84);
                }
                else if (style == "rolling")
                {
                    for (int s = 0; s < 16; ++s)
                        if (s % 4 >= 2) w.note (root, stepTick (bar, s, swing), tick16 - 20, s % 4 == 2 ? 100 : 80);
                }
                else if (style == "wobble")
                {
                    for (int e = 0; e < 8; ++e) w.note (e >= 6 && bar % 2 == 1 ? root + 12 : root, b + e * 2 * tick16, 2 * tick16 - 10, e % 2 == 0 ? 110 : 90);
                }
                else // "sustain": long roots, doubled an octave up in the second half
                {
                    w.note (root, b, barTicks - 30, 96);
                    if (bar >= p.bars / 2) w.note (root + 12, b, barTicks - 30, 80);
                }
            }
            return w.done();
        }

        juce::MidiMessageSequence chordPart (const PartsPlan& p, const juce::String& style)
        {
            Writer w;
            const int swing = p.guide.groove.swing;
            std::vector<int> prev;
            for (int bar = 0; bar < p.bars; ++bar)
            {
                const auto& c = p.chords[(size_t) bar];
                const double b = bar * barTicks;
                if (style == "strum")
                {
                    // a guitar shape: low root, fifth, octave, third, fifth
                    const int r = noteIn (c.root, 40);
                    std::vector<int> shape { r, r + 7, r + 12 };
                    auto pcs = c.pitchClasses();
                    if (pcs.size() > 1) shape.push_back (noteIn (pcs[1], r + 13));
                    shape.push_back (r + 19);
                    // D . D U . U D U
                    const int steps[] = { 0, 4, 6, 10, 12, 14 };
                    const bool down[] = { true, true, false, false, true, false };
                    for (int i = 0; i < 6; ++i)
                    {
                        const double t = stepTick (bar, steps[i], swing);
                        const double len = (i < 5 ? stepTick (bar, steps[i + 1], swing) : b + barTicks) - t - 10;
                        if (down[i]) w.chord (shape, t, len, i == 0 ? 100 : 86, 10, true);
                        else w.chord (std::vector<int> (shape.end() - 3, shape.end()), t, len, 66, 8, false);
                    }
                }
                else if (style == "power")
                {
                    const int r = noteIn (c.root, 40);
                    for (int e = 0; e < 8; ++e) w.chord ({ r, r + 7, r + 12 }, b + e * 2 * tick16, 2 * tick16 - 50, e % 2 == 0 ? 108 : 88);
                }
                else if (style == "stabs")
                {
                    const auto v = voice (c, prev, 60, 79, 4);
                    prev = v;
                    const bool funky = swing > 51;
                    const char* pat = funky ? "x..x..x...x..x.." : "..x...x...x...x.";
                    for (int s = 0; s < 16; ++s)
                        if (pat[s] == 'x') w.chord (v, stepTick (bar, s, swing), tick16 - 10, 96);
                }
                else if (style == "neo" || style == "organ")
                {
                    const auto v = voice (c, prev, 55, 77, 4);
                    prev = v;
                    if (style == "organ")
                    {
                        w.chord (v, b, barTicks - 2 * tick16, 84);
                        w.chord (v, stepTick (bar, 10, swing), 2 * tick16, 90);
                        // a walk-up into the next chord: the next chord a half step lower, on the last 16th
                        if (bar % 2 == 1)
                        {
                            auto walk = voice (p.chords[(size_t) ((bar + 1) % p.bars)], v, 55, 77, 4);
                            for (auto& n : walk) --n;
                            w.chord (walk, stepTick (bar, 14, swing), 2 * tick16 - 20, 82);
                        }
                    }
                    else
                    {
                        w.chord (v, b, 5 * tick16, 88);
                        w.chord (v, stepTick (bar, 6, swing), 2 * tick16, 76);
                        w.chord (v, stepTick (bar, 10, swing), 4 * tick16 - 20, 80);
                    }
                }
                else if (style == "ballad")
                {
                    const int r = noteIn (c.root, 36);
                    w.chord ({ r, r + 12 }, b, barTicks - 30, 84);
                    const auto v = voice (c, prev, 55, 76, 3);
                    prev = v;
                    for (int e = 0; e < 8; ++e)            // the right hand broken into 8ths
                        w.note (v[(size_t) (e % 2 == 0 ? (e / 2) % v.size() : (e / 2 + 1) % v.size())], b + e * 2 * tick16, 2 * tick16 + 60, e == 0 ? 82 : 70);
                }
                else if (style == "pluck" || style == "arp")
                {
                    const auto v = voice (c, prev, style == "arp" ? 60 : 57, style == "arp" ? 84 : 79, 4);
                    prev = v;
                    std::vector<int> up = v;
                    up.push_back (v.front() + 12);
                    const int n = style == "arp" ? 16 : 8;
                    const int len = style == "arp" ? tick16 : 2 * tick16;
                    for (int i = 0; i < n; ++i)
                    {
                        const int idx = i % (int) (2 * up.size() - 2);
                        const int k = idx < (int) up.size() ? idx : (int) (2 * up.size() - 2) - idx;
                        w.note (up[(size_t) k], b + i * len, len - 20, i == 0 ? 92 : 78);
                    }
                }
                else // "pad"
                {
                    const auto v = voice (c, prev, 55, 79, 4);
                    prev = v;
                    w.chord (v, b, barTicks - 20, 78);
                }
            }
            return w.done();
        }

        juce::MidiMessageSequence padPart (const PartsPlan& p)
        {
            Writer w;
            std::vector<int> prev;
            for (int bar = 0; bar < p.bars; ++bar)
            {
                const auto v = voice (p.chords[(size_t) bar], prev, 57, 81, 4);
                prev = v;
                w.chord (v, bar * barTicks, barTicks - 20, 74);
            }
            return w.done();
        }

        juce::MidiMessageSequence hookPart (const PartsPlan& p)
        {
            Writer w;
            const auto style = p.guide.melodyStyle;
            juce::Random rng ((juce::int64) p.seed * 15485863 + 3);
            const int swing = p.guide.groove.swing;

            if (style == "arp")
            {
                auto plan = p;
                return chordPart (plan, "arp");
            }

            // the rhythm of a two-bar phrase (16th steps)
            juce::String rhythm;
            int low = 67, high = 86;
            bool pentatonic = false;
            if (style == "bells")       { rhythm = "x..x..x.x..x..x.x..x..x.x.x.x..."; low = 72; high = 91; pentatonic = true; }
            else if (style == "riff")   { rhythm = "x.xx..x.x.xx..x.x.xx..x.x..x.xx."; low = 40; high = 55; pentatonic = true; }
            else if (style == "fills")  { rhythm = "........................x.x.xxx."; low = 62; high = 79; pentatonic = true; }
            else if (style == "swells") { rhythm = "x.......x.......x..............."; low = 67; high = 84; }
            else                        { rhythm = "x...x.x...x.x...x...x.x.x......."; }

            // a little variety with each new idea: shift some notes by a 16th
            if (p.seed > 1)
                for (int i = 1; i + 1 < rhythm.length(); ++i)
                    if (rhythm[i] == 'x' && rhythm[i + 1] == '.' && rng.nextInt (4) == 0)
                        rhythm = rhythm.substring (0, i) + ".x" + rhythm.substring (i + 2);

            std::vector<int> onsets;
            for (int i = 0; i < rhythm.length(); ++i) if (rhythm[i] == 'x') onsets.push_back (i);
            // the shape of the phrase: steps up or down the scale
            std::vector<int> moves;
            for (size_t i = 0; i < onsets.size(); ++i) moves.push_back (rng.nextInt (5) - 2);

            const auto scale = scaleOf (p, pentatonic);
            auto scaleIndex = [&scale] (int note)
            {
                int best = 0, bestD = 99;
                for (int i = 0; i < (int) scale.size(); ++i)
                {
                    const int d = std::abs (mod12 (note - scale[(size_t) i]));
                    if (std::min (d, 12 - d) < bestD) { bestD = std::min (d, 12 - d); best = i; }
                }
                return best;
            };
            auto stepScale = [&] (int note, int move)
            {
                int idx = scaleIndex (note), oct = (note - mod12 (note)) / 12;
                int pc = mod12 (note);
                // snap to the scale first
                int n = oct * 12 + scale[(size_t) idx];
                if (std::abs (n - note) > 6) n += n < note ? 12 : -12;
                (void) pc;
                for (int m = 0; m < std::abs (move); ++m)
                {
                    idx += move > 0 ? 1 : -1;
                    if (idx >= (int) scale.size()) { idx = 0; }
                    if (idx < 0) { idx = (int) scale.size() - 1; }
                    const int target = scale[(size_t) idx];
                    if (move > 0) { do ++n; while (mod12 (n) != target); }
                    else          { do --n; while (mod12 (n) != target); }
                }
                return n;
            };
            auto nearestChordTone = [] (const Chord& c, int near)
            {
                int best = near, bestD = 99;
                for (auto pc : c.pitchClasses())
                    for (int o = -1; o <= 1; ++o)
                    {
                        const int n = noteIn (pc, near - 6) + o * 12;
                        if (std::abs (n - near) < bestD) { bestD = std::abs (n - near); best = n; }
                    }
                return best;
            };
            auto fold = [low, high] (int n) { while (n < low) n += 12; while (n > high) n -= 12; return n; };

            int note = fold (nearestChordTone (p.chords[0], (low + high) / 2));
            for (int block = 0; block < p.bars / 2; ++block)
            {
                for (size_t i = 0; i < onsets.size(); ++i)
                {
                    const int step = onsets[i];
                    const int bar = block * 2 + step / 16;
                    if (bar >= p.bars) break;
                    const auto& c = p.chords[(size_t) bar];
                    const bool strong = step % 8 == 0;
                    if (style == "riff")
                        note = fold (noteIn (c.root, low) + (i % 3 == 2 ? 7 : i % 3 == 1 ? (p.minor() ? 3 : 5) : 0));
                    else if (strong || i == 0)
                        note = fold (nearestChordTone (c, stepScale (note, moves[i])));
                    else
                        note = fold (stepScale (note, moves[i]));
                    const int nextStep = i + 1 < onsets.size() ? onsets[i + 1] : 32;
                    double len = (nextStep - step) * tick16 - 20.0;
                    if (style == "bells") len = std::min (len, 4.0 * tick16);
                    if (style == "swells") len = (nextStep - step) * tick16 - 10.0;
                    // the last phrase ends on the home note
                    const bool lastNote = block == p.bars / 2 - 1 && i + 1 == onsets.size();
                    if (lastNote && style != "riff")
                    {
                        note = fold (noteIn (p.tonic(), note - 6));
                        len = std::max (len, 4.0 * tick16);
                    }
                    w.note (note, stepTick (block * 2 + step / 16, step % 16, swing), len, strong ? 100 : 84);
                }
            }
            return w.done();
        }
    }

    //==============================================================================
    std::vector<int> Chord::pitchClasses() const
    {
        std::vector<int> v;
        for (auto i : intervals)
        {
            const int pc = mod12 (root + i);
            if (std::find (v.begin(), v.end(), pc) == v.end()) v.push_back (pc);
        }
        return v;
    }

    juce::String pitchName (int pc, bool flats)
    {
        static const char* sharps[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const char* flatNames[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        return (flats ? flatNames : sharps)[mod12 (pc)];
    }

    bool keyUsesFlats (int key)
    {
        // F, Bb, Eb, Ab, Db, Gb major, and D, G, C, F, Bb, Eb minor
        const int pc = key % 12;
        if (key < 12) return pc == 5 || pc == 10 || pc == 3 || pc == 8 || pc == 1 || pc == 6;
        return pc == 2 || pc == 7 || pc == 0 || pc == 5 || pc == 10 || pc == 3;
    }

    std::optional<Chord> chordFromName (const juce::String& text)
    {
        auto s = text.trim().upToFirstOccurrenceOf ("/", false, false);
        if (s.isEmpty()) return {};
        static const juce::String letters ("CDEFGAB");
        static const int naturals[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int li = letters.indexOfChar (juce::CharacterFunctions::toUpperCase (s[0]));
        if (li < 0) return {};
        int root = naturals[li];
        int pos = 1;
        bool flats = false;
        if (s.length() > 1 && (s[1] == '#' || s[1] == 'b'))
        {
            root += s[1] == '#' ? 1 : -1;
            flats = s[1] == 'b';
            pos = 2;
        }
        auto rest = s.substring (pos);
        bool minor = false;
        if (rest.startsWith ("m") && ! rest.startsWith ("maj")) { minor = true; rest = rest.substring (rest.startsWith ("min") ? 3 : 1); }
        else if (rest.startsWith ("-")) { minor = true; rest = rest.substring (1); }
        Chord c;
        c.root = mod12 (root);
        juce::String shown;
        c.intervals = intervalsFor (minor, rest, shown);
        c.name = pitchName (c.root, flats) + shown;
        return c;
    }

    std::optional<Chord> chordFromRoman (const juce::String& numeral, int tonic, bool minorMode, bool flats)
    {
        auto s = numeral.trim();
        int accidental = 0;
        if (s.startsWith ("b")) { accidental = -1; s = s.substring (1); }
        else if (s.startsWith ("#")) { accidental = 1; s = s.substring (1); }
        static const char* romans[] = { "VII", "VI", "IV", "V", "III", "II", "I" };
        static const int degrees[] = { 6, 5, 3, 4, 2, 1, 0 };
        for (int i = 0; i < 7; ++i)
        {
            const juce::String r (romans[i]);
            if (! s.startsWithIgnoreCase (r)) continue;
            const auto written = s.substring (0, r.length());
            const bool lower = written == written.toLowerCase();
            const int deg = degrees[i];
            Chord c;
            c.root = mod12 (tonic + (minorMode ? minorScale[deg] : majorScale[deg]) + accidental);
            juce::String shown;
            c.intervals = intervalsFor (lower, s.substring (r.length()), shown);
            c.name = pitchName (c.root, flats || accidental < 0) + shown;
            return c;
        }
        return {};
    }

    juce::String PartsPlan::chordLine() const
    {
        juce::StringArray names;
        for (size_t i = 0; i < chords.size() && i < 4; ++i) names.add (chords[i].name);
        bool repeats = chords.size() > 4;
        for (size_t i = 4; i < chords.size(); ++i) repeats = repeats && chords[i].name == chords[i % 4].name;
        if (! repeats) for (size_t i = 4; i < chords.size(); ++i) names.add (chords[i].name);
        return names.joinIntoString (" - ");
    }

    juce::String progressionInKey (const juce::StringArray& prog, int key)
    {
        if (prog.isEmpty()) return {};
        auto first = prog[0];
        if (first.startsWith ("b") || first.startsWith ("#")) first = first.substring (1);
        const bool progMinor = first.isNotEmpty() && juce::CharacterFunctions::isLowerCase (first[0]);
        int tonic = key % 12;
        if (progMinor && key < 12) tonic = mod12 (tonic + 9);
        if (! progMinor && key >= 12) tonic = mod12 (tonic + 3);
        juce::StringArray names;
        for (auto& n : prog)
            if (auto c = chordFromRoman (n, tonic, progMinor, keyUsesFlats (key))) names.add (c->name);
        return names.joinIntoString (" - ");
    }

    namespace
    {
        bool isMinorProgression (const juce::StringArray& prog)
        {
            auto first = prog[0];
            if (first.startsWith ("b") || first.startsWith ("#")) first = first.substring (1);
            return first.isNotEmpty() && juce::CharacterFunctions::isLowerCase (first[0]);
        }

        // the style's progressions, the ones in the song's mode first
        std::vector<const juce::StringArray*> orderedProgressions (const PartsPlan& p)
        {
            std::vector<const juce::StringArray*> fitting, others;
            for (auto& prog : p.guide.progressions) (isMinorProgression (prog) == p.minor() ? fitting : others).push_back (&prog);
            for (auto* o : others) fitting.push_back (o);
            return fitting;
        }

        const juce::StringArray& progressionFor (const PartsPlan& p, int index)
        {
            const auto list = orderedProgressions (p);
            return *list[(size_t) (((int) p.seed - 1 + index) % (int) list.size())];
        }
    }

    std::vector<Chord> progressionChords (const PartsPlan& p, int index, int bars)
    {
        std::vector<Chord> out;
        if (p.guide.progressions.empty()) return out;
        const auto& prog = progressionFor (p, index);
        // a minor progression in a major key is played from the relative minor (and the other way round)
        const bool progMinor = isMinorProgression (prog);
        int tonic = p.tonic();
        if (progMinor && ! p.minor()) tonic = mod12 (tonic + 9);
        if (! progMinor && p.minor()) tonic = mod12 (tonic + 3);
        for (int bar = 0; bar < bars; ++bar)
        {
            auto c = chordFromRoman (prog[bar % prog.size()], tonic, progMinor, keyUsesFlats (p.key));
            out.push_back (c ? *c : Chord { tonic, { 0, 4, 7 }, pitchName (tonic, keyUsesFlats (p.key)) });
        }
        return out;
    }

    PartsPlan planParts (const SongBrief& brief, const juce::StringArray& songChords, juce::uint32 seed)
    {
        PartsPlan p;
        p.seed = std::max<juce::uint32> (1, seed);
        p.guide = styleGuide (brief.genre, brief.styleId);
        double styleBpm = 0;
        if (auto* s = brief.style()) styleBpm = s->bpm;
        if (styleBpm <= 0) for (auto* s : stylesFor (brief.genre)) if (s->isGeneral()) styleBpm = s->bpm;
        p.bpm = brief.bpm > 0 ? brief.bpm : (styleBpm > 0 ? styleBpm : 120.0);


        // the key: the song's, or the one the style's progressions suit
        p.keyGiven = brief.key >= 0;
        if (p.keyGiven) p.key = brief.key;
        else
        {
            int minors = 0;
            for (auto& prog : p.guide.progressions) if (isMinorProgression (prog)) ++minors;
            p.key = minors * 2 >= (int) p.guide.progressions.size() ? 21 : 0;     // A minor or C major
        }
        const bool flats = keyUsesFlats (p.key);

        // the chords: from the Key Compass song when there is one
        for (auto& name : songChords)
            if (auto c = chordFromName (name)) p.chords.push_back (*c);
        if (! p.chords.empty())
        {
            const auto n = p.chords.size();
            for (size_t i = n; i < (size_t) p.bars; ++i) p.chords.push_back (p.chords[i % n]);
            p.chords.resize ((size_t) p.bars);
            p.chordSource = "your Key Compass song";
            return p;
        }

        // otherwise one of the style's progressions, preferring ones in the song's mode; each new idea takes the next
        p.chords = progressionChords (p, 0, p.bars);
        p.chordSource = p.guide.name + ": " + progressionFor (p, 0).joinIntoString (" - ");
        juce::ignoreUnused (flats);
        return p;
    }

    PartType partTypeFor (const PartSpec& part)
    {
        const auto role = part.role.toLowerCase();
        for (auto* w : { "lead", "melody", "hook", "fiddle", "sample" }) if (role.contains (w)) return PartType::hook;
        for (auto* w : { "pad", "atmosphere", "strings", "choir", "brass" }) if (role.contains (w)) return PartType::pad;
        if (role.contains ("texture") || role == "fx") return PartType::none;
        if (part.kinds.empty()) return PartType::none;
        switch (part.kinds.front())
        {
            case Kind::drums: case Kind::percussion:                         return PartType::drums;
            case Kind::bass:                                                 return PartType::bass;
            case Kind::pad: case Kind::strings: case Kind::brass:
            case Kind::woodwinds: case Kind::vocal:                          return PartType::pad;
            case Kind::lead: case Kind::pluck: case Kind::sequence:          return PartType::hook;
            case Kind::piano: case Kind::keys: case Kind::organ:
            case Kind::guitar: case Kind::synth:                             return PartType::chords;
            case Kind::fx: case Kind::world: case Kind::other:               return PartType::none;
        }
        return PartType::none;
    }

    juce::String partTypeName (PartType t)
    {
        switch (t)
        {
            case PartType::drums:  return "drums";
            case PartType::bass:   return "bass line";
            case PartType::chords: return "chords";
            case PartType::pad:    return "held chords";
            case PartType::hook:   return "hook idea";
            case PartType::none:   break;
        }
        return {};
    }

    int DrumNotes::note (const juce::String& lane) const
    {
        auto it = notes.find (lane);
        return it != notes.end() ? it->second : -1;
    }

    DrumNotes generalMidiDrums()
    {
        DrumNotes d;
        d.notes = { { "kick", 36 }, { "rim", 37 }, { "snare", 38 }, { "clap", 39 }, { "hat", 42 }, { "pedalhat", 44 },
                    { "openhat", 46 }, { "tomlow", 45 }, { "tommid", 47 }, { "tomhigh", 50 }, { "crash", 49 }, { "ride", 51 },
                    { "perc", 56 }, { "shaker", 70 } };
        return d;
    }

    std::optional<DrumNotes> drumNotesFromMap (const juce::File& file)
    {
        auto xml = juce::parseXML (file);
        if (xml == nullptr || ! xml->hasTagName ("DrumMap")) return {};
        std::vector<std::pair<int, juce::String>> pads;      // note, lower-case name
        for (auto* list : xml->getChildWithTagNameIterator ("list"))
            if (list->getStringAttribute ("name") == "Map")
                for (auto* item : list->getChildIterator())
                {
                    int in = -1;
                    juce::String name;
                    for (auto* f : item->getChildIterator())
                    {
                        if (f->getStringAttribute ("name") == "INote") in = f->getIntAttribute ("value", -1);
                        if (f->getStringAttribute ("name") == "Name") name = f->getStringAttribute ("value").toLowerCase();
                    }
                    if (in >= 0 && name.isNotEmpty()) pads.emplace_back (in, name);
                }
        std::sort (pads.begin(), pads.end());
        auto first = [&pads] (std::initializer_list<const char*> words) -> int
        {
            for (auto* w : words)
                for (auto& [n, name] : pads)
                    if (name.contains (w)) return n;
            return -1;
        };
        DrumNotes d;
        d.source = file.getFileNameWithoutExtension() + " drum map";
        const int kick = first ({ "kick", "808" });
        if (kick < 0) return {};
        const int snare = first ({ "snare", "clap", "rim" });
        const int hat = first ({ "closed hat", "hat", "shaker", "perc" });
        d.notes["kick"] = kick;
        d.notes["snare"] = snare;
        d.notes["clap"] = std::max (first ({ "clap" }), snare);
        d.notes["rim"] = std::max (first ({ "rim" }), snare);
        d.notes["hat"] = hat;
        d.notes["openhat"] = std::max (first ({ "open hat" }), hat);
        d.notes["pedalhat"] = std::max (first ({ "pedal" }), hat);
        std::vector<int> toms;
        for (auto& [n, name] : pads) if (name.contains ("tom")) toms.push_back (n);
        d.notes["tomlow"] = toms.empty() ? snare : toms.front();
        d.notes["tommid"] = toms.empty() ? snare : toms[toms.size() / 2];
        d.notes["tomhigh"] = toms.empty() ? snare : toms.back();
        d.notes["crash"] = first ({ "crash", "cymbal" });
        d.notes["ride"] = first ({ "ride", "cymbal" });
        d.notes["perc"] = first ({ "cowbell", "perc" });
        d.notes["shaker"] = first ({ "shaker", "tambourine", "perc" });
        return d;
    }

    juce::String chordStyleFor (const PartsPlan& p, Kind instrument)
    {
        const auto& s = p.guide.chordStyle;
        if (instrument == Kind::guitar)
        {
            if (s == "strum" || s == "power" || s == "pluck" || s == "stabs") return s;
            return s == "pad" || s == "ballad" ? "pluck" : "strum";      // ambient styles pick, others strum
        }
        if (instrument == Kind::piano || instrument == Kind::keys || instrument == Kind::organ)
            if (s == "strum" || s == "power") return "ballad";
        return s;
    }

    juce::MidiMessageSequence makePart (const PartsPlan& p, PartType t, const DrumNotes& drums, Kind instrument)
    {
        if (p.chords.empty()) return {};
        switch (t)
        {
            case PartType::drums:  return drumPart (p, drums);
            case PartType::bass:   return bassPart (p);
            case PartType::chords: return chordPart (p, chordStyleFor (p, instrument));
            case PartType::pad:    return padPart (p);
            case PartType::hook:   return hookPart (p);
            case PartType::none:   break;
        }
        return {};
    }

    juce::String describePart (const PartsPlan& p, PartType t, const DrumNotes& drums, Kind instrument)
    {
        const auto bars = juce::String (p.bars) + " bars";
        switch (t)
        {
            case PartType::drums:
            {
                auto s = bars + " of " + p.guide.name + " drums";
                if (p.guide.groove.swing > 52) s << " with swing";
                s << " (" << drums.source << " notes: kick " << noteName (drums.note ("kick")) << ", snare " << noteName (drums.note ("snare"))
                  << ", hat " << noteName (drums.note ("hat")) << ")";
                if (drums.note ("808") >= 0 && p.guide.bassStyle.startsWith ("808"))
                    s << ". The kit's 808 (" << noteName (drums.note ("808")) << ") plays with the kick";
                return s;
            }
            case PartType::bass:
            {
                const auto& b = p.guide.bassStyle;
                juce::String how = b.startsWith ("808") ? "an 808 line that follows the kick" : b == "rootfifth" ? "root and fifth, walking into each chord"
                                 : b == "eighths" ? "driving 8th notes" : b == "offbeat" ? "offbeat notes between the kicks" : b == "funk" ? "a funky, syncopated line"
                                 : b == "rnb" ? "a smooth line with passing notes" : b == "octave" ? "octave jumps" : b == "rolling" ? "a rolling 16th line"
                                 : b == "wobble" ? "pulsing 8ths" : b == "rootkick" ? "roots with the kick" : "long root notes";
                return bars + ": " + how;
            }
            case PartType::chords:
            {
                const auto c = chordStyleFor (p, instrument);
                juce::String how = c == "strum" ? "strummed" : c == "power" ? "power chords in 8ths" : c == "stabs" ? "short stabs" : c == "neo" ? "rich, syncopated voicings"
                                 : c == "organ" ? "held chords with gospel walk-ups" : c == "ballad" ? "a piano ballad pattern" : c == "pluck" ? "broken chords in 8ths"
                                 : c == "arp" ? "an arpeggio" : "held chords";
                return bars + " of chords, " + how + ": " + p.chordLine();
            }
            case PartType::pad:  return bars + " of held chords: " + p.chordLine();
            case PartType::hook:
            {
                const auto& m = p.guide.melodyStyle;
                juce::String how = m == "bells" ? "a bell melody" : m == "riff" ? "a riff" : m == "fills" ? "fills between the vocal lines"
                                 : m == "swells" ? "long, soaring notes" : m == "arp" ? "an arpeggio" : "a hook";
                return bars + ": " + how + " in " + keyName (p.key);
            }
            case PartType::none: break;
        }
        return {};
    }

    bool writeMidiFile (const PartsPlan& p, const std::vector<NamedPart>& parts, const juce::File& file,
                        const std::vector<std::pair<double, juce::String>>& markers, int bars)
    {
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ticksPerQuarter);
        const double end = (double) (bars > 0 ? bars : p.bars) * barTicks;

        juce::MidiMessageSequence conductor;
        conductor.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / p.bpm)), 0);
        conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
        {
            // sharps (+) or flats (-) in the key signature
            static const int majorAccidentals[] = { 0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5 };
            const int major = p.minor() ? mod12 (p.tonic() + 3) : p.tonic();
            conductor.addEvent (juce::MidiMessage::keySignatureMetaEvent (majorAccidentals[major], p.minor()), 0);
        }
        for (auto& [tick, name] : markers) conductor.addEvent (juce::MidiMessage::textMetaEvent (6, name), tick);
        conductor.addEvent (juce::MidiMessage::endOfTrack(), end);
        conductor.sort();
        mf.addTrack (conductor);

        for (auto& part : parts)
        {
            juce::MidiMessageSequence t;
            t.addEvent (juce::MidiMessage::textMetaEvent (3, part.name), 0);
            t.addSequence (part.notes, 0);
            t.addEvent (juce::MidiMessage::endOfTrack(), end);
            t.sort();
            mf.addTrack (t);
        }
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream out (file);
        return out.openedOk() && mf.writeTo (out, 1);
    }
}
