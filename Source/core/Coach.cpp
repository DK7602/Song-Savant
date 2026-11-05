#include "Coach.h"
#include "DrumLanes.h"
#include <set>

namespace ss::coach
{
    juce::String areaName (Area a)
    {
        switch (a)
        {
            case Area::shape:    return "Song shape";
            case Area::energy:   return "Energy";
            case Area::tempoKey: return "Tempo and key";
            case Area::chords:   return "Chords";
            case Area::melody:   return "Melody";
            case Area::bass:     return "Bass";
            case Area::drums:    return "Drums";
        }
        return {};
    }

    juce::String timeText (double seconds)
    {
        const int s = juce::jmax (0, juce::roundToInt (seconds));
        return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
    }

    std::vector<std::pair<double, double>> tempoRanges (const juce::String& text)
    {
        // every "a-b" in the text: "130-160 BPM, felt in half time (like 65-80)" -> 130-160 and 65-80
        std::vector<std::pair<double, double>> out;
        const auto t = text.replaceCharacter (0x2013, '-');
        for (int i = 0; i < t.length();)
        {
            if (! juce::CharacterFunctions::isDigit (t[i])) { ++i; continue; }
            int j = i;
            while (j < t.length() && juce::CharacterFunctions::isDigit (t[j])) ++j;
            const double a = t.substring (i, j).getDoubleValue();
            int k = j;
            while (k < t.length() && t[k] == ' ') ++k;
            if (k < t.length() && t[k] == '-')
            {
                int m = k + 1;
                while (m < t.length() && t[m] == ' ') ++m;
                int n = m;
                while (n < t.length() && juce::CharacterFunctions::isDigit (t[n])) ++n;
                if (n > m)
                {
                    const double b = t.substring (m, n).getDoubleValue();
                    if (a >= 30 && b >= a && b <= 300) out.push_back ({ a, b });
                    i = n;
                    continue;
                }
            }
            i = j;
        }
        return out;
    }

    namespace
    {
        bool isChorus (const juce::String& n)
        {
            if (n.containsIgnoreCase ("pre") || n.containsIgnoreCase ("post")) return false;
            return n.containsIgnoreCase ("chorus") || n.containsIgnoreCase ("hook") || n.containsIgnoreCase ("drop");
        }
        bool isVerse (const juce::String& n)   { return n.startsWithIgnoreCase ("verse"); }
        bool isIntro (const juce::String& n)   { return n.startsWithIgnoreCase ("intro"); }
        bool isOutro (const juce::String& n)   { return n.startsWithIgnoreCase ("outro"); }

        bool inScale (int pitch, int key)
        {
            static const std::set<int> major { 0, 2, 4, 5, 7, 9, 11 }, minor { 0, 2, 3, 5, 7, 8, 10 };
            const int rel = ((pitch - key % 12) % 12 + 12) % 12;
            return (key >= 12 ? minor : major).count (rel) > 0;
        }

        juce::String noteName (int note)    { return juce::MidiMessage::getMidiNoteName (note, true, true, 3); }

        float energyOf (const ArrangementSection& s, const std::vector<ArrangementPart>& parts)
        {
            float e = 0.0f;
            for (auto& p : parts)
            {
                const auto m = s.modeFor (p.role);
                if (m.isEmpty()) continue;
                if (p.type == PartType::drums) e += m == "light" ? 0.5f : m == "groove" ? 1.0f : m == "build" ? 1.25f : 1.5f;
                else e += 1.0f;
            }
            return e;
        }

        int partsPlaying (const ArrangementSection& s, const std::vector<ArrangementPart>& parts)
        {
            int n = 0;
            for (auto& p : parts) if (s.modeFor (p.role).isNotEmpty()) ++n;
            return n;
        }

        bool everythingOn (const ArrangementSection& s, const std::vector<ArrangementPart>& parts)
        {
            for (auto& p : parts)
            {
                const auto m = s.modeFor (p.role);
                if (p.type == PartType::drums ? m != "full" : m.isEmpty()) return false;
            }
            return true;
        }

        const Palette* palette (const SongSession& s)
        {
            return s.current >= 0 && s.current < (int) s.palettes.size() ? &s.palettes[(size_t) s.current] : nullptr;
        }

        const ArrangementPart* partFor (const std::vector<ArrangementPart>& parts, const juce::String& role)
        {
            for (auto& p : parts) if (p.role == role) return &p;
            return nullptr;
        }

        // the style's accented snare / clap hits, as 16th steps in the bar
        std::set<int> styleBackbeat (const StyleGuide& g)
        {
            std::set<int> steps;
            for (auto lane : { "snare", "clap" })
            {
                auto it = g.groove.lanes.find (lane);
                if (it == g.groove.lanes.end()) continue;
                for (int i = 0; i < it->second.length(); ++i)
                    if (it->second[i] == 'x' || it->second[i] == 'X') steps.insert (i % 16);
            }
            return steps;
        }

        bool isBackbeatNote (const PatternNote& n)
        {
            return (n.note == laneNote ("snare") || n.note == laneNote ("clap")) && n.velocity >= 70;
        }

        // the steps your snare / clap hits in at least half the bars
        std::set<int> yourBackbeat (const Pattern& p)
        {
            std::map<int, std::set<int>> barsAt;
            for (auto& n : p.notes)
                if (isBackbeatNote (n))
                {
                    const int step = juce::roundToInt (n.start * 4.0);
                    barsAt[((step % 16) + 16) % 16].insert (step / 16);
                }
            std::set<int> out;
            const int need = juce::jmax (1, p.bars / 2);
            for (auto& [step, bars] : barsAt) if ((int) bars.size() >= need) out.insert (step);
            return out;
        }

        juce::String beatsText (const std::set<int>& steps)
        {
            juce::StringArray on, off;
            for (int s : steps) (s % 4 == 0 ? on : off).add (juce::String (s / 4 + 1));
            juce::String t;
            if (! on.isEmpty())
                t = (on.size() == 1 ? "beat " : "beats ") + (on.size() > 1 ? on.joinIntoString (", ", 0, on.size() - 1) + " and " + on[on.size() - 1] : on[0]);
            if (! off.isEmpty()) t << (t.isEmpty() ? "" : ", plus ") << "between beats " << off.joinIntoString (" and ");
            return t;
        }

        Fix fixOf (FixKind kind, int section, int bar, int value)
        {
            Fix f;
            f.kind = kind;
            f.section = section;
            f.bar = bar;
            f.value = value;
            return f;
        }

        double sectionSeconds (int bars, double bpm)   { return bars * 4.0 * 60.0 / juce::jmax (20.0, bpm); }

        struct Song
        {
            const SongSession& s;
            const Catalog* catalog;
            const Palette* pal = nullptr;
            std::vector<ArrangementPart> parts;
            PartsPlan plan;
            DrumNotes drums;
            SongBrief brief;
            const Arrangement& arr;
            std::vector<int> startBar;              // each section's first bar (0-based)

            Song (const SongSession& song, const Catalog* c) : s (song), catalog (c), arr (song.arrangement)
            {
                pal = palette (song);
                if (pal != nullptr)
                {
                    parts = arrangementParts (*pal, c);
                    drums = drumNotesFor (*pal, c);
                }
                plan = song.partsPlan();
                brief = song.makeBrief();
                int bar = 0;
                for (auto& sec : arr.sections) { startBar.push_back (bar); bar += sec.bars; }
            }

            int first (bool (*test) (const juce::String&)) const
            {
                for (int i = 0; i < (int) arr.sections.size(); ++i) if (test (arr.sections[(size_t) i].name)) return i;
                return -1;
            }
            int last (bool (*test) (const juce::String&)) const
            {
                for (int i = (int) arr.sections.size(); --i >= 0;) if (test (arr.sections[(size_t) i].name)) return i;
                return -1;
            }
            int count (bool (*test) (const juce::String&)) const
            {
                int n = 0;
                for (auto& sec : arr.sections) if (test (sec.name)) ++n;
                return n;
            }
            juce::String where (int section) const
            {
                if (section < 0 || section >= (int) arr.sections.size()) return {};
                return "the " + arr.sections[(size_t) section].name + " at bar " + juce::String (startBar[(size_t) section] + 1);
            }
            juce::String styleName() const
            {
                if (auto* st = brief.style(); st != nullptr && ! st->isGeneral()) return st->name;
                return genreName (brief.genre);
            }
            bool fastGenre() const
            {
                return brief.genre == Genre::pop || brief.genre == Genre::trap || brief.genre == Genre::hiphop
                       || brief.genre == Genre::edm || brief.genre == Genre::rnb;
            }
            bool followsDaw() const          { return s.hostBpm > 0 && s.form.bpm <= 0; }
        };

        void add (Report& r, Tip t)
        {
            if (t.id.isEmpty()) return;
            r.tips.push_back (std::move (t));
        }

        void good (Report& r, Area a, const juce::String& text)    { r.working.push_back ({ a, text }); }

        //==============================================================================
        void checkTempoAndKey (const Song& g, Report& r)
        {
            const double bpm = g.plan.bpm;
            const auto ranges = tempoRanges (g.plan.guide.tempo);
            if (! ranges.empty() && bpm > 0)
            {
                auto within = [&ranges] (double b)
                {
                    for (auto& [lo, hi] : ranges) if (b >= lo - 3 && b <= hi + 3) return true;
                    return false;
                };
                const auto usual = g.plan.guide.tempo.upToFirstOccurrenceOf (",", false, false).upToFirstOccurrenceOf ("(", false, false).trim();
                if (within (bpm))
                    good (r, Area::tempoKey, juce::String (juce::roundToInt (bpm)) + " BPM sits in " + g.styleName() + "'s usual " + usual + ".");
                else if (within (bpm * 2.0) || within (bpm / 2.0))
                    good (r, Area::tempoKey, juce::String (juce::roundToInt (bpm)) + " BPM works as " + (within (bpm * 2.0) ? "half" : "double")
                                             + " time for " + g.styleName() + " (usually " + usual + ").");
                else
                {
                    // the nearest tempo the style uses (not a half-time feel's: trap's "like 65-80" is still played at 130-160)
                    double best = bpm, gap = 1.0e9;
                    for (auto& [lo, hi] : ranges)
                    {
                        if (hi <= ranges.front().first * 0.65) continue;
                        const double c = juce::jlimit (lo, hi, bpm);
                        if (std::abs (c - bpm) < gap) { gap = std::abs (c - bpm); best = c; }
                    }
                    const int target = juce::roundToInt (best);
                    Tip t;
                    t.id = "tempo";
                    t.area = Area::tempoKey;
                    t.priority = 70;
                    t.title = juce::String ("The tempo is ") + (bpm < best ? "slow" : "fast") + " for " + g.styleName();
                    t.why = g.styleName() + " usually sits at " + g.plan.guide.tempo + ". Your song is at " + juce::String (juce::roundToInt (bpm))
                            + " BPM, so it may not feel like " + g.styleName() + " to a listener.";
                    if (g.followsDaw())
                        t.tryThis = "Set your DAW's tempo to about " + juce::String (target) + " BPM (the song follows it), or keep it if that feel is the point.";
                    else
                    {
                        t.tryThis = "Set it to about " + juce::String (target) + " BPM, or keep it if that feel is the point.";
                        t.fixLabel = "SET " + juce::String (target) + " BPM";
                        t.fix.kind = FixKind::setBpm;
                        t.fix.value = target;
                    }
                    add (r, t);
                }
            }

            // major or minor, when the style leans one way and you picked the key
            const auto keys = g.plan.guide.keys.toLowerCase().trim();
            const bool wantsMinor = keys.startsWith ("minor") || keys.startsWith ("mostly minor");
            const bool wantsMajor = keys.startsWith ("major keys") || keys.startsWith ("mostly major") || keys.startsWith ("singable major");
            if (g.plan.keyGiven && (wantsMinor || wantsMajor))
            {
                const bool minor = g.plan.minor();
                if (minor == wantsMinor)
                    good (r, Area::tempoKey, keyName (g.plan.key) + ": " + (minor ? "a minor" : "a major") + " key, the way " + g.styleName() + " usually goes.");
                else
                {
                    const int relative = minor ? (g.plan.tonic() + 3) % 12 : 12 + (g.plan.tonic() + 9) % 12;
                    Tip t;
                    t.id = "key.mode";
                    t.area = Area::tempoKey;
                    t.priority = 30;
                    t.title = juce::String ("A ") + (minor ? "minor" : "major") + " key for " + g.styleName();
                    t.why = g.styleName() + " is " + (wantsMinor ? "mostly in minor keys (darker)" : "mostly in major keys (brighter)") + "; yours is in "
                            + keyName (g.plan.key) + ". That can be the point of the song.";
                    t.tryThis = "If you want the usual colour, try " + keyName (relative) + " (the same notes, a different home) on the SONG page.";
                    add (r, t);
                }
            }
        }

        //==============================================================================
        void checkShape (const Song& g, Report& r)
        {
            const auto& a = g.arr;
            if (a.sections.size() < 2) return;
            const double bpm = g.plan.bpm;
            const auto& form = g.plan.guide.form;
            bool styleHasChorus = false;
            for (auto& f : form) styleHasChorus = styleHasChorus || isChorus (f.name);
            const bool loopsAllowed = g.brief.genre == Genre::lofi || g.brief.genre == Genre::cinematic;

            // when the chorus comes in
            const int chorus = g.first (isChorus);
            if (chorus < 0)
            {
                if (styleHasChorus && ! loopsAllowed && a.sections.size() >= 3)
                {
                    juce::String name = "Chorus";
                    for (auto& f : form) if (isChorus (f.name)) { name = f.name; break; }
                    const int verse = g.first (isVerse);
                    Tip t;
                    t.id = "shape.nochorus";
                    t.area = Area::shape;
                    t.priority = 60;
                    t.title = "There's no " + name.toLowerCase();
                    t.why = g.styleName() + " songs are built around a " + name.toLowerCase() + ": the part people remember and wait for.";
                    t.tryThis = "Add one after the first verse, and bring it back two or three times.";
                    t.fixLabel = "ADD A " + name.toUpperCase();
                    t.fix.kind = FixKind::addSection;
                    t.fix.name = name;
                    t.fix.value = verse >= 0 ? verse + 1 : (int) a.sections.size();
                    add (r, t);
                }
            }
            else
            {
                const double at = sectionSeconds (g.startBar[(size_t) chorus], bpm);
                double limit = g.fastGenre() ? 45.0 : 60.0;
                if (g.brief.era >= 0 && g.brief.era <= 3) limit += 15.0;     // the 90s and before took a little longer
                if (g.brief.genre == Genre::cinematic) limit = 1.0e9;
                const auto name = a.sections[(size_t) chorus].name.toLowerCase();
                if (at <= limit)
                    good (r, Area::shape, "The " + name + " arrives at " + timeText (at) + " (bar " + juce::String (g.startBar[(size_t) chorus] + 1) + "), in good time for "
                                          + g.styleName() + ".");
                else
                {
                    Tip t;
                    t.id = "shape.latechorus";
                    t.area = Area::shape;
                    t.priority = 65;
                    t.section = chorus;
                    t.title = "The " + name + " takes a while to arrive";
                    t.why = "Your first " + name + " starts at " + timeText (at) + " (bar " + juce::String (g.startBar[(size_t) chorus] + 1) + "). In "
                            + g.styleName() + (g.brief.era >= 5 || g.brief.era < 0 ? " today" : "") + ", listeners usually hear it within about "
                            + juce::String (juce::roundToInt (limit)) + " seconds.";
                    t.tryThis = "Shorten the intro or the first verse, or open the song with the " + name + ".";
                    // the fix: the intro down to 4 bars, else the first verse down to 8
                    const int intro = g.first (isIntro);
                    const int verse = g.first (isVerse);
                    if (intro >= 0 && intro < chorus && a.sections[(size_t) intro].bars > 4)
                    {
                        t.fixLabel = "SHORTEN THE INTRO TO 4 BARS";
                        t.fix = fixOf (FixKind::setBars, intro, -1, 4);
                    }
                    else if (verse >= 0 && verse < chorus && a.sections[(size_t) verse].bars > 8)
                    {
                        t.fixLabel = "SHORTEN THE FIRST VERSE TO 8 BARS";
                        t.fix = fixOf (FixKind::setBars, verse, -1, 8);
                    }
                    add (r, t);
                }
            }

            // a long intro (when the chorus check didn't already say so)
            const int intro = g.first (isIntro);
            int styleIntro = 0;
            for (auto& f : form) if (isIntro (f.name)) { styleIntro = f.bars; break; }
            bool introTip = false;
            for (auto& t : r.tips) introTip = introTip || (t.fix.kind == FixKind::setBars && t.fix.section == intro);
            if (intro >= 0 && styleIntro > 0 && ! introTip)
            {
                const int bars = a.sections[(size_t) intro].bars;
                if (bars >= 8 && bars >= styleIntro * 2)
                {
                    Tip t;
                    t.id = "shape.longintro";
                    t.area = Area::shape;
                    t.priority = 40;
                    t.section = intro;
                    t.title = "A long intro";
                    t.why = "Your intro is " + juce::String (bars) + " bars (" + timeText (sectionSeconds (bars, bpm)) + "); " + g.styleName()
                            + " usually takes " + juce::String (styleIntro) + ".";
                    t.tryThis = "Cut it to " + juce::String (styleIntro) + " bars so the song gets going.";
                    t.fixLabel = "SHORTEN THE INTRO TO " + juce::String (styleIntro) + " BARS";
                    t.fix = fixOf (FixKind::setBars, intro, -1, styleIntro);
                    add (r, t);
                }
            }

            // sections the style uses that the song doesn't have (once the song has its choruses)
            if (g.count (isChorus) >= 2)
            {
                const auto names = sectionNamesInSong (a);
                juce::StringArray seen;
                for (auto& f : form)
                {
                    if (seen.contains (f.name) || names.contains (f.name) || isIntro (f.name) || isChorus (f.name) || isVerse (f.name)) continue;
                    seen.add (f.name);
                    const bool outro = isOutro (f.name);
                    const bool pre = f.name.containsIgnoreCase ("pre");
                    if (outro)
                    {
                        if (isOutro (a.sections.back().name)) continue;
                        Tip t;
                        t.id = "shape.missing." + f.name;
                        t.area = Area::shape;
                        t.priority = 15;
                        t.title = "No outro";
                        t.why = "The song stops at the end of " + g.where ((int) a.sections.size() - 1) + ". " + g.styleName() + " songs usually wind down with an outro.";
                        t.tryThis = "Add a short outro, or end on a last chorus that's cut off on purpose.";
                        t.fixLabel = "ADD AN OUTRO";
                        t.fix.kind = FixKind::addSection;
                        t.fix.name = f.name;
                        t.fix.value = (int) a.sections.size();
                        add (r, t);
                        continue;
                    }
                    Tip t;
                    t.id = "shape.missing." + f.name;
                    t.area = Area::shape;
                    t.priority = pre ? 25 : 35;
                    t.title = "No " + f.name.toLowerCase();
                    if (pre)
                    {
                        t.why = g.styleName() + " songs often build into the chorus with a short pre-chorus, so the chorus feels like a release.";
                        t.tryThis = "Add a pre-chorus before each chorus.";
                        t.fixLabel = "ADD A PRE-CHORUS";
                        t.fix.beforeChoruses = true;
                    }
                    else
                    {
                        t.why = "Most " + g.styleName() + " songs have a " + f.name.toLowerCase() + " before the last chorus: something new (other chords, fewer parts) "
                                "so the last chorus feels fresh again.";
                        t.tryThis = "Add a " + f.name.toLowerCase() + " before the last chorus.";
                        t.fixLabel = "ADD A " + f.name.toUpperCase();
                        t.fix.value = g.last (isChorus);
                    }
                    t.fix.kind = FixKind::addSection;
                    t.fix.name = f.name;
                    add (r, t);
                }
            }

            // the length, against the style's usual song
            int styleBars = 0;
            for (auto& f : form) styleBars += f.bars;
            const double songTime = sectionSeconds (a.totalBars(), bpm), styleTime = sectionSeconds (styleBars, bpm);
            if (styleBars > 0 && a.sections.size() >= 4 && g.brief.genre != Genre::cinematic)
            {
                if (songTime > styleTime * 1.5 && songTime > 300.0)
                {
                    Tip t;
                    t.id = "shape.long";
                    t.area = Area::shape;
                    t.priority = 20;
                    t.title = "A long song";
                    t.why = "It runs " + timeText (songTime) + " at " + juce::String (juce::roundToInt (bpm)) + " BPM; " + g.styleName() + " songs are usually about "
                            + timeText (styleTime) + ".";
                    t.tryThis = "Look for a verse or a repeat that can go: the song will feel tighter.";
                    add (r, t);
                }
                else if (songTime >= styleTime * 0.6)
                    good (r, Area::shape, "It runs " + timeText (songTime) + ", about right for " + g.styleName() + " (usually " + timeText (styleTime) + ").");
            }
        }

        //==============================================================================
        void checkEnergy (const Song& g, Report& r)
        {
            const auto& a = g.arr;
            const int verse = g.first (isVerse), chorus = g.first (isChorus);
            if (verse >= 0 && chorus >= 0 && ! g.parts.empty())
            {
                const auto& v = a.sections[(size_t) verse];
                const auto& c = a.sections[(size_t) chorus];
                const float ev = energyOf (v, g.parts), ec = energyOf (c, g.parts);
                const auto cname = c.name.toLowerCase();
                if (ec <= ev && ! everythingOn (c, g.parts))
                {
                    Tip t;
                    t.id = "energy.chorus";
                    t.area = Area::energy;
                    t.priority = 55;
                    t.section = chorus;
                    t.title = "The " + cname + " doesn't lift";
                    t.why = "Your " + cname + " plays " + (ec < ev ? "fewer parts than" : "the same parts as") + " the verse. In " + g.styleName()
                            + ", the " + cname + " usually adds parts and bigger drums so it lands.";
                    t.tryThis = "Bring in the parts that rest in the verse, and give the drums more.";
                    t.fixLabel = "LIFT EVERY " + cname.toUpperCase();
                    t.fix.kind = FixKind::liftChoruses;
                    add (r, t);
                }
                else if (ec > ev)
                {
                    const int more = partsPlaying (c, g.parts) - partsPlaying (v, g.parts);
                    good (r, Area::energy, "The " + cname + " lifts above the verse" + (more > 0 ? juce::String (": it adds ") + juce::String (more) + (more == 1 ? " part." : " parts.")
                                                                                                : juce::String (" with bigger drums.")));
                }
            }

            // every section the same
            if (a.sections.size() >= 4 && ! g.parts.empty())
            {
                bool same = true;
                for (auto& sec : a.sections) same = same && sec.play == a.sections.front().play;
                if (same && partsPlaying (a.sections.front(), g.parts) >= 3)
                {
                    Tip t;
                    t.id = "energy.flat";
                    t.area = Area::energy;
                    t.priority = 45;
                    t.title = "Every section plays the same parts";
                    t.why = "From the first bar to the last, the same parts play. With nowhere to build from, the song can feel flat.";
                    t.tryThis = "Start smaller: an intro without drums or bass, a first verse with lighter drums. Then the chorus has somewhere to go.";
                    t.fixLabel = "LIGHTEN THE START";
                    t.fix.kind = FixKind::lightenStart;
                    add (r, t);
                }
            }

            // the last chorus: as big as the song gets
            const int lastChorus = g.last (isChorus);
            if (g.count (isChorus) >= 2 && lastChorus != chorus && ! g.parts.empty())
            {
                const auto& lc = a.sections[(size_t) lastChorus];
                const float e1 = energyOf (a.sections[(size_t) chorus], g.parts), e2 = energyOf (lc, g.parts);
                if (e2 <= e1 && ! everythingOn (lc, g.parts))
                {
                    Tip t;
                    t.id = "energy.lastchorus";
                    t.area = Area::energy;
                    t.priority = 25;
                    t.section = lastChorus;
                    t.title = "The last " + lc.name.toLowerCase() + " could go bigger";
                    t.why = "It plays the same as the first one. The last " + lc.name.toLowerCase() + " is usually the biggest moment of the song.";
                    t.tryThis = "Bring every part in and give the drums all they've got (or make it different: a key change, a new top line).";
                    t.fixLabel = "LIFT THE LAST " + lc.name.toUpperCase();
                    t.fix.kind = FixKind::liftLastChorus;
                    t.fix.section = lastChorus;
                    add (r, t);
                }
                else if (e2 > e1)
                    good (r, Area::energy, "The last " + lc.name.toLowerCase() + " is the biggest moment.");
            }
        }

        //==============================================================================
        void checkChords (const Song& g, Report& r)
        {
            const auto& a = g.arr;
            const int key = g.plan.key;
            const bool minor = g.plan.minor();
            // chords from outside the key that you picked (the ones that sound wrong more often than bold)
            const std::set<int> odd = minor ? std::set<int> { 4, 6, 9 } : std::set<int> { 1, 6 };
            juce::StringArray checked;
            int found = 0;
            for (int i = 0; i < (int) a.sections.size() && found < 2; ++i)
            {
                const auto& sec = a.sections[(size_t) i];
                if (! sec.ownChords || checked.contains (sec.name)) continue;
                checked.add (sec.name);
                const auto chords = sectionChords (g.plan, sec);
                const auto theirs = styleChords (g.plan, sec);
                for (int bar = 0; bar < (int) chords.size() && bar < sec.bars; ++bar)
                {
                    const int rel = ((chords[(size_t) bar].root - key % 12) % 12 + 12) % 12;
                    if (odd.count (rel) == 0) continue;
                    const auto instead = theirs.empty() ? juce::String() : theirs[(size_t) bar % theirs.size()].name;
                    Tip t;
                    t.id = "chords.outside." + sec.name + "." + juce::String (bar);
                    t.area = Area::chords;
                    t.priority = 35;
                    t.section = i;
                    t.title = chords[(size_t) bar].name + " is outside the key (" + sec.name + ", bar " + juce::String (bar + 1) + ")";
                    t.why = chords[(size_t) bar].name + " isn't in " + keyName (key) + ", so it sounds surprising. That can be the point.";
                    t.tryThis = instead.isNotEmpty() && instead != chords[(size_t) bar].name
                                  ? "If it sounds wrong, Song Savant's chord there is " + instead + "."
                                  : juce::String ("If it sounds wrong, pick a chord from the key there (click it along the top of the piano roll).");
                    if (instead.isNotEmpty() && instead != chords[(size_t) bar].name)
                    {
                        t.fixLabel = "USE " + instead.toUpperCase();
                        t.fix = fixOf (FixKind::chordBack, i, bar, 0);
                    }
                    add (r, t);
                    ++found;
                    break;
                }
            }

            // the chorus on the verse's chords
            const int verse = g.first (isVerse), chorus = g.first (isChorus);
            if (verse >= 0 && chorus >= 0)
            {
                const auto& v = a.sections[(size_t) verse];
                const auto& c = a.sections[(size_t) chorus];
                juce::StringArray vc, cc;
                for (auto& ch : sectionChords (g.plan, v)) vc.add (ch.name);
                for (auto& ch : sectionChords (g.plan, c)) cc.add (ch.name);
                const int n = juce::jmin (vc.size(), cc.size(), 4);
                bool same = n > 0;
                for (int i = 0; i < n; ++i) same = same && vc[i] == cc[i];
                if (same && (v.ownChords || c.ownChords || g.arr.shape == SongShape::blank))
                {
                    Tip t;
                    t.id = "chords.chorusverse";
                    t.area = Area::chords;
                    t.priority = 30;
                    t.section = chorus;
                    t.title = "The " + c.name.toLowerCase() + " uses the verse's chords";
                    t.why = "Both start " + vc.joinIntoString (" - ", 0, n) + ". New chords (or the same ones starting somewhere else) make the "
                            + c.name.toLowerCase() + " feel like a new place.";
                    t.tryThis = "Start the " + c.name.toLowerCase() + " on another chord of the key: the IV or the vi in a major key, the VI or the III in a minor one.";
                    juce::StringArray theirs;
                    for (auto& ch : styleChords (g.plan, c)) theirs.add (ch.name);
                    bool differ = false;
                    for (int i = 0; i < juce::jmin (n, theirs.size()); ++i) differ = differ || theirs[i] != vc[i];
                    if (c.ownChords && differ)
                    {
                        t.fixLabel = "USE SONG SAVANT'S CHORUS CHORDS";
                        t.fix = fixOf (FixKind::chorusChords, chorus, -1, 0);
                    }
                    add (r, t);
                }
                else if (! same && n > 0)
                    good (r, Area::chords, "The " + c.name.toLowerCase() + " moves to new chords (" + cc.joinIntoString (" - ", 0, n) + ").");
            }
        }

        //==============================================================================
        // your own notes: the first section that plays each of your patterns
        void checkNotes (const Song& g, Report& r)
        {
            const auto& a = g.arr;
            const int key = g.plan.key;
            int melodyTips = 0, bassTips = 0, drumTips = 0;
            bool melodyGood = false, bassGood = false, snareGood = false, swingGood = false;
            for (auto& part : g.parts)
            {
                if (part.type != PartType::hook && part.type != PartType::bass && part.type != PartType::drums) continue;
                juce::StringArray seen;
                for (int i = 0; i < (int) a.sections.size(); ++i)
                {
                    if (! g.s.isMine (i, part.role)) continue;
                    const auto& sec = a.sections[(size_t) i];
                    const auto patternName = sec.patternFor (part.role);
                    if (seen.contains (patternName)) continue;
                    seen.add (patternName);
                    const auto p = g.s.patternAt (i, part, g.drums);
                    if (p.notes.empty()) continue;
                    const auto label = part.role + " in the " + patternName;
                    const auto idBase = part.role + "." + patternName;

                    if (part.type == PartType::hook)
                    {
                        int lo = 127, hi = 0, outside = 0;
                        for (auto& n : p.notes) { lo = juce::jmin (lo, n.note); hi = juce::jmax (hi, n.note); if (! inScale (n.note, key)) ++outside; }
                        bool fine = true;
                        if (hi - lo > 19 && melodyTips < 2)
                        {
                            Tip t;
                            t.id = "melody.range." + idBase;
                            t.area = Area::melody;
                            t.priority = 30;
                            t.section = i;
                            t.role = part.role;
                            t.title = "A wide melody: " + label;
                            t.why = "It spans " + juce::String (hi - lo) + " semitones, from " + noteName (lo) + " to " + noteName (hi)
                                    + ". Most singers are comfortable within about an octave and a half, and a hook is easier to remember when it stays close.";
                            t.tryThis = "Bring the highest and lowest notes an octave closer to the rest.";
                            t.fixLabel = "BRING IT IN";
                            t.fix.kind = FixKind::foldMelody;
                            t.fix.section = i;
                            t.fix.role = part.role;
                            add (r, t);
                            ++melodyTips;
                            fine = false;
                        }
                        if (outside >= 2 && outside * 100 > (int) p.notes.size() * 15 && melodyTips < 2)
                        {
                            Tip t;
                            t.id = "melody.key." + idBase;
                            t.area = Area::melody;
                            t.priority = 28;
                            t.section = i;
                            t.role = part.role;
                            t.title = "Notes outside the key: " + label;
                            t.why = juce::String (outside) + " of its " + juce::String ((int) p.notes.size()) + " notes aren't in " + keyName (key)
                                    + ". A few passing ones add colour; many can sound like wrong notes.";
                            t.tryThis = "Move them a half step into the key, or keep the ones you like.";
                            t.fixLabel = "MOVE INTO THE KEY";
                            t.fix.kind = FixKind::notesInKey;
                            t.fix.section = i;
                            t.fix.role = part.role;
                            add (r, t);
                            ++melodyTips;
                            fine = false;
                        }
                        melodyGood = melodyGood || fine;
                    }
                    else if (part.type == PartType::bass)
                    {
                        const auto chords = sectionChords (g.plan, sec);
                        int bars = 0, roots = 0;
                        for (int b = 0; b < p.bars && ! chords.empty(); ++b)
                        {
                            const PatternNote* firstNote = nullptr;
                            for (auto& n : p.notes)
                                if (n.start >= b * 4.0 - 0.01 && n.start < b * 4.0 + 0.25 && (firstNote == nullptr || n.note < firstNote->note)) firstNote = &n;
                            if (firstNote == nullptr) continue;
                            ++bars;
                            if (firstNote->note % 12 == chords[(size_t) b % chords.size()].root % 12) ++roots;
                        }
                        if (bars >= 4 && roots * 2 < bars && bassTips < 2)
                        {
                            Tip t;
                            t.id = "bass.roots." + idBase;
                            t.area = Area::bass;
                            t.priority = 25;
                            t.section = i;
                            t.role = part.role;
                            t.title = "The bass rarely starts the bar on the chord's root: " + label;
                            t.why = "In " + juce::String (bars - roots) + " of " + juce::String (bars) + " bars, its first note isn't the chord's root. "
                                    "The bass landing on the root on the one is what makes the chords sound solid.";
                            t.tryThis = "Start each bar on the chord's root, then move around it.";
                            t.fixLabel = "ROOTS ON THE ONE";
                            t.fix.kind = FixKind::bassRoots;
                            t.fix.section = i;
                            t.fix.role = part.role;
                            add (r, t);
                            ++bassTips;
                        }
                        else if (bars >= 4) bassGood = true;
                    }
                    else if (part.type == PartType::drums && drumTips < 2)
                    {
                        const auto style = styleBackbeat (g.plan.guide);
                        const auto yours = yourBackbeat (p);
                        bool hit = false;
                        for (int s : style) hit = hit || yours.count (s) > 0;
                        if (! style.empty() && ! yours.empty() && ! hit)
                        {
                            Tip t;
                            t.id = "drums.backbeat." + idBase;
                            t.area = Area::drums;
                            t.priority = 40;
                            t.section = i;
                            t.role = part.role;
                            t.title = "The snare isn't where " + g.styleName() + " puts it";
                            t.why = g.styleName() + " puts the snare (or clap) on " + beatsText (style) + "; yours hits on " + beatsText (yours) + ". "
                                    "The snare's place is most of what makes a groove feel like its style.";
                            t.tryThis = "Move the snare to " + beatsText (style) + ", or keep yours if the different feel is the point.";
                            t.fixLabel = "SNARE ON " + beatsText (style).toUpperCase();
                            t.fix.kind = FixKind::styleSnare;
                            t.fix.section = i;
                            t.fix.role = part.role;
                            add (r, t);
                            ++drumTips;
                        }
                        else if (! style.empty() && hit) snareGood = true;

                        // swing: the style swings its 16ths and your hats are dead straight
                        const int swing = g.plan.guide.groove.swing;
                        if (swing >= 55)
                        {
                            int offbeats = 0;
                            double drift = 0.0;
                            for (auto& n : p.notes)
                            {
                                if (n.note != laneNote ("hat") && n.note != laneNote ("openhat")) continue;
                                const int step = juce::roundToInt (n.start * 4.0 - 0.15);
                                if (step % 2 == 0) continue;
                                ++offbeats;
                                drift += n.start * 4.0 - step;
                            }
                            if (offbeats >= 4)
                            {
                                const double mean = drift / offbeats;
                                if (mean < 0.04 && drumTips < 2)
                                {
                                    Tip t;
                                    t.id = "drums.swing." + idBase;
                                    t.area = Area::drums;
                                    t.priority = 20;
                                    t.section = i;
                                    t.role = part.role;
                                    t.title = "The hats are dead straight";
                                    t.why = g.styleName() + " swings its 16ths (about " + juce::String (swing) + "%); your hats sit exactly on the grid, so the groove can feel stiff.";
                                    t.tryThis = "Push the in-between hats a little late.";
                                    t.fixLabel = "SWING THE HATS";
                                    t.fix.kind = FixKind::swingHats;
                                    t.fix.section = i;
                                    t.fix.role = part.role;
                                    t.fix.value = swing;
                                    add (r, t);
                                    ++drumTips;
                                }
                                else if (mean >= 0.08) swingGood = true;
                            }
                        }
                    }
                }
            }
            if (melodyGood) good (r, Area::melody, "Your melody stays in the key and in an easy range.");
            if (bassGood) good (r, Area::bass, "Your bass lands on the chords' roots.");
            if (snareGood) good (r, Area::drums, "Your snare sits where " + g.styleName() + " puts it.");
            if (swingGood) good (r, Area::drums, "Your hats swing, the way " + g.styleName() + " does.");
        }

        bool sameSections (const Arrangement& a, const Arrangement& b)
        {
            if (a.sections.size() != b.sections.size()) return false;
            for (size_t i = 0; i < a.sections.size(); ++i)
            {
                const auto& x = a.sections[i];
                const auto& y = b.sections[i];
                if (x.name != y.name || x.bars != y.bars || x.play != y.play || x.patterns != y.patterns || x.chords != y.chords || x.ownChords != y.ownChords)
                    return false;
            }
            return true;
        }
    }

    //==============================================================================
    bool isYours (const SongSession& s, const Catalog* c)
    {
        if (s.songShape == SongShape::blank) return true;
        if (! s.patterns.empty()) return true;
        if (s.arrangement.empty()) return false;
        for (auto& sec : s.arrangement.sections) if (sec.ownChords) return true;
        const auto* pal = palette (s);
        if (pal == nullptr) return false;
        const auto parts = arrangementParts (*pal, c);
        const auto made = makeArrangement (s.partsPlan(), parts, s.keyCompassShape(), s.songShape);
        return ! sameSections (made, s.arrangement);
    }

    Report coachSong (const SongSession& s, const Catalog* c)
    {
        Report r;
        r.yours = isYours (s, c);
        Song g (s, c);
        {
            juce::String h = genreName (g.brief.genre);
            if (auto* st = g.brief.style(); st != nullptr && ! st->isGeneral()) h << ": " << st->name;
            if (g.brief.era >= 0) h << " (" << eraName (g.brief.era) << ")";
            h << "  |  " << juce::String (juce::roundToInt (g.plan.bpm)) << " BPM in " << keyName (g.plan.key);
            if (! g.arr.empty())
                h << "  |  " << juce::String ((int) g.arr.sections.size()) << (g.arr.sections.size() == 1 ? " section, " : " sections, ")
                  << timeText (sectionSeconds (g.arr.totalBars(), g.plan.bpm));
            r.headline = h;
        }
        if (! r.yours || g.pal == nullptr) return r;
        bool playing = false;
        for (auto& sec : g.arr.sections) playing = playing || partsPlaying (sec, g.parts) > 0;
        if (! playing && s.patterns.empty())
        {
            r.empty = true;
            return r;
        }

        checkTempoAndKey (g, r);
        checkShape (g, r);
        checkEnergy (g, r);
        checkChords (g, r);
        checkNotes (g, r);

        // the ones you meant are left out
        std::vector<Tip> kept;
        for (auto& t : r.tips)
        {
            if (s.coachDismissed.contains (t.id)) ++r.dismissed;
            else kept.push_back (std::move (t));
        }
        std::stable_sort (kept.begin(), kept.end(), [] (const Tip& a, const Tip& b) { return a.priority > b.priority; });
        r.tips = std::move (kept);
        return r;
    }

    //==============================================================================
    Snapshot snapshot (const SongSession& s)
    {
        return { s.form, s.brief.bpm, s.arrangement, s.patterns };
    }

    void restore (SongSession& s, const Snapshot& snap)
    {
        s.form = snap.form;
        s.brief.bpm = snap.briefBpm;
        s.arrangement = snap.arrangement;
        s.patterns = snap.patterns;
    }

    bool sameSong (const Snapshot& a, const Snapshot& b)
    {
        return std::abs (a.form.bpm - b.form.bpm) < 0.001 && a.form.key == b.form.key && a.patterns == b.patterns
               && a.arrangement.toXml()->isEquivalentTo (b.arrangement.toXml().get(), false);
    }

    bool applyFix (SongSession& s, const Catalog* c, const Fix& f)
    {
        const auto before = snapshot (s);
        const auto* pal = palette (s);
        const auto parts = pal != nullptr ? arrangementParts (*pal, c) : std::vector<ArrangementPart>();
        const auto drums = pal != nullptr ? drumNotesFor (*pal, c) : generalMidiDrums();
        const auto plan = s.partsPlan();
        auto& a = s.arrangement;
        const bool validSection = f.section >= 0 && f.section < (int) a.sections.size();
        auto liftAll = [&parts] (ArrangementSection& sec)
        {
            for (auto& p : parts) sec.play[p.role] = p.type == PartType::drums ? "full" : "on";
        };
        // a change to your own notes: the pattern the section plays, changed and kept as yours
        auto changeNotes = [&] (const std::function<void (Pattern&)>& change)
        {
            const auto* part = partFor (parts, f.role);
            if (part == nullptr || ! validSection) return;
            auto p = s.patternAt (f.section, *part, drums);
            change (p);
            sortNotes (p.notes);
            s.storePattern (f.section, *part, std::move (p));
        };
        const int key = plan.key;

        switch (f.kind)
        {
            case FixKind::none: break;
            case FixKind::setBpm:
                if (f.value > 0) { s.form.bpm = f.value; s.brief.bpm = f.value; }
                break;
            case FixKind::setBars:
                if (validSection) setSectionBars (a, f.section, f.value);
                break;
            case FixKind::addSection:
                if (f.beforeChoruses)
                {
                    for (int i = (int) a.sections.size(); --i >= 0;)
                        if (isChorus (a.sections[(size_t) i].name)) addSection (a, i, f.name, plan, parts);
                }
                else addSection (a, juce::jlimit (0, (int) a.sections.size(), f.value), f.name, plan, parts);
                break;
            case FixKind::liftChoruses:
                for (auto& sec : a.sections) if (isChorus (sec.name)) liftAll (sec);
                break;
            case FixKind::liftLastChorus:
                if (validSection) liftAll (a.sections[(size_t) f.section]);
                break;
            case FixKind::lightenStart:
            {
                bool first = true;
                for (auto& sec : a.sections)
                {
                    if (isIntro (sec.name))
                    {
                        for (auto& p : parts) if (p.type == PartType::drums || p.type == PartType::bass) sec.play[p.role] = "";
                        if (partsPlaying (sec, parts) == 0)
                            for (auto& p : parts) if (p.type == PartType::chords || p.type == PartType::pad) sec.play[p.role] = "on";
                    }
                    else if (isVerse (sec.name) && first)
                    {
                        first = false;
                        for (auto& p : parts)
                            if (p.type == PartType::drums && (sec.modeFor (p.role) == "full" || sec.modeFor (p.role) == "build")) sec.play[p.role] = "groove";
                    }
                }
                break;
            }
            case FixKind::chordBack:
                if (validSection) setSectionChord (a, plan, f.section, f.bar, {});
                break;
            case FixKind::chorusChords:
                if (validSection) clearSectionChords (a, f.section);
                break;
            case FixKind::foldMelody:
                changeNotes ([] (Pattern& p)
                {
                    std::vector<int> pitches;
                    for (auto& n : p.notes) pitches.push_back (n.note);
                    std::sort (pitches.begin(), pitches.end());
                    if (pitches.empty()) return;
                    const int middle = pitches[pitches.size() / 2];
                    for (auto& n : p.notes)
                    {
                        while (n.note > middle + 9) n.note -= 12;
                        while (n.note < middle - 9) n.note += 12;
                    }
                });
                break;
            case FixKind::notesInKey:
                changeNotes ([key] (Pattern& p)
                {
                    for (auto& n : p.notes)
                        if (! inScale (n.note, key)) n.note += inScale (n.note - 1, key) ? -1 : 1;
                });
                break;
            case FixKind::bassRoots:
            {
                const auto chords = validSection ? sectionChords (plan, a.sections[(size_t) f.section]) : std::vector<Chord>();
                if (chords.empty()) break;
                changeNotes ([&chords] (Pattern& p)
                {
                    for (int b = 0; b < p.bars; ++b)
                    {
                        PatternNote* firstNote = nullptr;
                        for (auto& n : p.notes)
                            if (n.start >= b * 4.0 - 0.01 && n.start < b * 4.0 + 0.25 && (firstNote == nullptr || n.note < firstNote->note)) firstNote = &n;
                        if (firstNote == nullptr) continue;
                        const int root = chords[(size_t) b % chords.size()].root % 12;
                        int d = ((root - firstNote->note % 12) % 12 + 12) % 12;
                        if (d > 6) d -= 12;
                        firstNote->note = juce::jlimit (0, 127, firstNote->note + d);
                    }
                });
                break;
            }
            case FixKind::styleSnare:
            {
                const auto steps = styleBackbeat (plan.guide);
                if (steps.empty()) break;
                changeNotes ([&steps] (Pattern& p)
                {
                    // the new hits on the drum you used most (the snare or the clap)
                    int snares = 0, claps = 0;
                    for (auto& n : p.notes) if (isBackbeatNote (n)) (n.note == laneNote ("clap") ? claps : snares)++;
                    const int lane = claps > snares ? laneNote ("clap") : laneNote ("snare");
                    p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [] (const PatternNote& n) { return isBackbeatNote (n); }), p.notes.end());
                    for (int b = 0; b < p.bars; ++b)
                        for (int step : steps) p.notes.push_back ({ b * 4.0 + step / 4.0, 0.25, lane, 112 });
                });
                break;
            }
            case FixKind::swingHats:
            {
                const double late = juce::jlimit (0.0, 0.2, (f.value / 100.0) * 0.5 - 0.25);
                changeNotes ([late] (Pattern& p)
                {
                    for (auto& n : p.notes)
                    {
                        if (n.note != laneNote ("hat") && n.note != laneNote ("openhat")) continue;
                        const int step = juce::roundToInt (n.start * 4.0);
                        if (step % 2 == 0 || std::abs (n.start * 4.0 - step) > 0.15) continue;
                        n.start = step / 4.0 + late;
                        n.length = juce::jmin (n.length, 0.25 - late * 0.5);
                    }
                });
                break;
            }
        }
        return ! sameSong (before, snapshot (s));
    }
}
