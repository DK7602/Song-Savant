#include "Music.h"
#include <algorithm>
#include <map>

namespace ss
{
    namespace
    {
        struct StyleRow { const char* genre; const char* id; const char* name; const char* keywords; const char* eras; double bpm; };
        #include "MusicData.inc"

        Genre genreFromGrooveGrabberEnum (const juce::String& g)
        {
            static const std::map<juce::String, Genre> m { { "trap", Genre::trap }, { "boomBap", Genre::hiphop }, { "rnb", Genre::rnb },
                                                            { "pop", Genre::pop }, { "rock", Genre::rock }, { "country", Genre::country },
                                                            { "worship", Genre::worship }, { "edm", Genre::edm }, { "lofi", Genre::lofi },
                                                            { "cinematic", Genre::cinematic } };
            auto it = m.find (g);
            return it != m.end() ? it->second : Genre::pop;
        }

        int eraFromToken (const juce::String& t)
        {
            static const char* names[] = { "60s", "70s", "80s", "90s", "2000s", "2010s", "now" };
            for (int i = 0; i < numEras; ++i) if (t == names[i]) return i;
            return -1;
        }
    }

    juce::String genreName (Genre g)
    {
        switch (g)
        {
            case Genre::trap:      return "Trap / Drill";
            case Genre::hiphop:    return "Hip-Hop";
            case Genre::rnb:       return "R&B / Soul";
            case Genre::pop:       return "Pop";
            case Genre::rock:      return "Rock";
            case Genre::country:   return "Country";
            case Genre::worship:   return "Worship / Gospel";
            case Genre::edm:       return "EDM / Dance";
            case Genre::lofi:      return "Lo-Fi";
            case Genre::cinematic: return "Cinematic";
            case Genre::count:     break;
        }
        return {};
    }

    juce::String genreId (Genre g)
    {
        static const char* ids[] = { "trap", "boombap", "rnb", "pop", "rock", "country", "worship", "edm", "lofi", "cinematic" };
        return (int) g >= 0 && (int) g < numGenres ? juce::String (ids[(int) g]) : juce::String();
    }

    std::optional<Genre> genreFromId (const juce::String& id)
    {
        for (int i = 0; i < numGenres; ++i)
            if (genreId ((Genre) i) == id) return (Genre) i;
        return std::nullopt;
    }

    juce::String eraName (int era)
    {
        static const char* names[] = { "60s", "70s", "80s", "90s", "2000s", "2010s", "Today" };
        return era >= 0 && era < numEras ? juce::String (names[era]) : juce::String();
    }

    const std::vector<Style>& allStyles()
    {
        static const std::vector<Style> styles = []
        {
            std::vector<Style> out;
            for (auto& r : styleRows)
            {
                Style s;
                s.genre = genreFromGrooveGrabberEnum (r.genre);
                s.id = r.id;
                s.name = r.name;
                s.keywords = juce::StringArray::fromTokens (r.keywords, "|", "");
                s.keywords.removeEmptyStrings();
                for (auto& e : juce::StringArray::fromTokens (r.eras, "|", ""))
                    if (const int era = eraFromToken (e); era >= 0) s.eras.push_back (era);
                s.bpm = r.bpm;
                out.push_back (s);
            }
            return out;
        }();
        return styles;
    }

    const Style* styleById (const juce::String& id)
    {
        if (id.isEmpty()) return nullptr;
        for (auto& s : allStyles())
            if (s.id == id) return &s;
        return nullptr;
    }

    std::vector<const Style*> stylesFor (Genre g)
    {
        std::vector<const Style*> out;
        for (auto& s : allStyles())
            if (s.genre == g) out.push_back (&s);
        std::stable_sort (out.begin(), out.end(), [] (auto* a, auto* b) { return a->isGeneral() && ! b->isGeneral(); });
        return out;
    }

    juce::String normaliseText (const juce::String& text)
    {
        juce::String out (" ");
        for (auto c : text.toLowerCase().replace ("'", "").replace (juce::String::charToString ((juce::juce_wchar) 0x2019), ""))
            out << (juce::CharacterFunctions::isLetterOrDigit (c) || c == '&' || c == '#' ? juce::String::charToString (c) : juce::String (" "));
        while (out.contains ("  ")) out = out.replace ("  ", " ");
        return out.endsWithChar (' ') ? out : out + " ";
    }

    //==============================================================================
    const ReferenceTake* Reference::takeFor (Genre g) const
    {
        for (auto& t : takes)
            if (auto* s = styleById (t.styleId); s != nullptr && s->genre == g) return &t;
        return nullptr;
    }

    const std::vector<Reference>& allReferences()
    {
        static const std::vector<Reference> refs = []
        {
            std::vector<Reference> out;
            for (auto* raw : referenceRows)
            {
                const auto line = juce::String::fromUTF8 (raw);
                if (! line.contains ("=")) continue;
                auto names = line.upToFirstOccurrenceOf ("=", false, false).trim();
                Reference r;
                const bool song = names.startsWithChar ('@');
                if (song) names = names.substring (1);
                auto parts = juce::StringArray::fromTokens (names, "|", "");
                for (int i = 0; i < parts.size(); ++i)
                {
                    auto n = parts[i].trim();
                    const bool cue = n.startsWithChar ('~');
                    if (cue) n = n.substring (1).trim();
                    if (i == 0)
                    {
                        if (song && n.contains (" / "))
                        {
                            r.by = n.fromLastOccurrenceOf (" / ", false, false).trim();
                            n = n.upToLastOccurrenceOf (" / ", false, false).trim();
                        }
                        r.name = n;
                    }
                    r.aliases.add (normaliseText (n).trim());
                    r.needsCue.add (cue);
                }
                for (auto& take : juce::StringArray::fromTokens (line.fromFirstOccurrenceOf ("=", false, false), ";", ""))
                {
                    auto words = juce::StringArray::fromTokens (take.trim(), " ", "");
                    words.removeEmptyStrings();
                    if (words.isEmpty()) continue;
                    ReferenceTake t;
                    t.styleId = words[0];
                    t.era = words.size() > 1 ? eraFromToken (words[1]) : -1;
                    for (int i = 3; i < words.size(); ++i) t.sound.add (words[i]);
                    r.takes.push_back (t);
                }
                if (! r.takes.empty()) out.push_back (r);
            }
            return out;
        }();
        return refs;
    }

    //==============================================================================
    int parseKey (const juce::String& text)
    {
        static const char* letters[] = { "c", "c#", "d", "eb", "e", "f", "f#", "g", "ab", "a", "bb", "b" };
        auto t = " " + text.toLowerCase().replace ("sharp", "#").replace (" flat", "b").replace ("\xe2\x99\xaf", "#").replace ("\xe2\x99\xad", "b") + " ";
        t = t.replace ("db", "c#").replace ("d#", "eb").replace ("gb", "f#").replace ("g#", "ab").replace ("a#", "bb");
        auto words = juce::StringArray::fromTokens (t.replaceCharacters (",.;:!?()", "        "), " ", "");
        words.removeEmptyStrings();
        for (int i = 0; i < words.size(); ++i)
        {
            auto w = words[i];
            const bool cue = i > 0 && (words[i - 1] == "in" || words[i - 1] == "of" || words[i - 1] == "key");
            for (int k = 0; k < 12; ++k)
            {
                const juce::String l (letters[k]);
                if (w == l + "m" || w == l + "min" || w == l + "minor") return 12 + k;
                if (w == l + "maj" || w == l + "major") return k;
                if (w == l)
                {
                    const auto next = words[i + 1];
                    if (next == "minor" || next == "min" || next == "m") return 12 + k;
                    if (next == "major" || next == "maj") return k;
                    if (cue && ! (l == "a" && words[i - 1] == "in")) return k;    // "in E", "key of A" ("in a" is ordinary English)
                }
            }
        }
        return -1;
    }

    juce::String keyName (int key)
    {
        static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        if (key < 0 || key > 23) return "no key";
        return juce::String (names[key % 12]) + (key < 12 ? " Major" : " Minor");
    }

    juce::StringArray soundWords()
    {
        return { "warm", "bright", "dark", "airy", "wide", "big", "huge", "punchy", "soft", "gentle", "gritty", "dirty", "clean", "vintage",
                 "retro", "modern", "ambient", "atmospheric", "lush", "dreamy", "uplifting", "happy", "sad", "emotional", "melancholy",
                 "energetic", "chill", "mellow", "smooth", "epic", "intimate", "acoustic", "electronic", "organic", "analog", "digital",
                 "heavy", "hard", "aggressive", "funky", "jazzy", "soulful", "dusty", "lofi", "glossy", "sparkly", "haunting", "triumphant",
                 "anthemic", "live", "sparse", "minimal", "cinematic", "orchestral", "tape", "crisp" };
    }

    //==============================================================================
    namespace
    {
        // Words that name a genre without naming a particular style
        std::optional<Genre> genreFromWords (const juce::String& t, juce::String& said)
        {
            static const std::vector<std::pair<const char*, Genre>> words {
                { " hip hop ", Genre::hiphop }, { " hiphop ", Genre::hiphop }, { " rap ", Genre::hiphop }, { " boom bap ", Genre::hiphop },
                { " trap ", Genre::trap }, { " drill ", Genre::trap },
                { " r&b ", Genre::rnb }, { " rnb ", Genre::rnb }, { " r & b ", Genre::rnb }, { " soul ", Genre::rnb }, { " neo soul ", Genre::rnb },
                { " pop ", Genre::pop },
                { " rock ", Genre::rock }, { " metal ", Genre::rock }, { " punk ", Genre::rock }, { " grunge ", Genre::rock },
                { " country ", Genre::country }, { " americana ", Genre::country }, { " bluegrass ", Genre::country },
                { " worship ", Genre::worship }, { " gospel ", Genre::worship }, { " christian ", Genre::worship }, { " praise ", Genre::worship },
                { " church ", Genre::worship },
                { " edm ", Genre::edm }, { " house ", Genre::edm }, { " techno ", Genre::edm }, { " dance ", Genre::edm }, { " dubstep ", Genre::edm },
                { " trance ", Genre::edm }, { " disco ", Genre::edm }, { " drum and bass ", Genre::edm }, { " dnb ", Genre::edm },
                { " lofi ", Genre::lofi }, { " lo fi ", Genre::lofi }, { " chillhop ", Genre::lofi },
                { " cinematic ", Genre::cinematic }, { " film score ", Genre::cinematic }, { " trailer ", Genre::cinematic },
                { " soundtrack ", Genre::cinematic }, { " orchestral ", Genre::cinematic } };
            for (auto& [w, g] : words)
                if (t.contains (w)) { said = juce::String (w).trim(); return g; }
            return std::nullopt;
        }

        int eraFromWords (const juce::String& t, juce::String& said)
        {
            static const std::vector<std::pair<const char*, int>> words {
                { " 60s ", 0 }, { " sixties ", 0 }, { " 1960s ", 0 }, { " 70s ", 1 }, { " seventies ", 1 }, { " 1970s ", 1 },
                { " 80s ", 2 }, { " eighties ", 2 }, { " 1980s ", 2 }, { " 90s ", 3 }, { " nineties ", 3 }, { " 1990s ", 3 },
                { " 2000s ", 4 }, { " 00s ", 4 }, { " 2010s ", 5 }, { " 10s ", 5 }, { " today ", 6 }, { " current ", 6 }, { " nowadays ", 6 } };
            for (auto& [w, e] : words)
                if (t.contains (w)) { said = juce::String (w).trim(); return e; }
            return -1;
        }

        double bpmFromWords (const juce::String& t)
        {
            auto words = juce::StringArray::fromTokens (t, " ", "");
            words.removeEmptyStrings();
            for (int i = 0; i < words.size(); ++i)
            {
                auto w = words[i];
                if (w.endsWith ("bpm") && w.length() > 3) w = w.dropLastCharacters (3);
                else if (words[i + 1] != "bpm") continue;
                const int v = w.getIntValue();
                if (w.containsOnly ("0123456789") && v >= 40 && v <= 240) return v;
            }
            return 0;
        }

        struct Hit { int ref, start, end; };

        std::vector<Hit> findReferences (const juce::String& t)
        {
            static const juce::StringArray cuesBefore { "like", "style of", "vibe of", "sound of", "feel of", "kind of", "sounds like", "inspired by" };
            static const juce::StringArray cuesAfter { "type beat", "vibe", "style", "type", "sound", "inspired", "feel" };
            std::vector<Hit> hits;
            const auto& refs = allReferences();
            for (int r = 0; r < (int) refs.size(); ++r)
                for (int a = 0; a < refs[(size_t) r].aliases.size(); ++a)
                {
                    const auto& alias = refs[(size_t) r].aliases[a];
                    if (alias.length() < 2) continue;
                    for (int pos = t.indexOf (" " + alias + " "); pos >= 0; pos = t.indexOf (pos + 1, " " + alias + " "))
                    {
                        const int start = pos + 1, end = start + alias.length();
                        if (refs[(size_t) r].needsCue[a])
                        {
                            bool cued = false;
                            const auto before = t.substring (0, start), after = t.substring (end);
                            for (auto& c : cuesBefore) if (before.endsWith (" " + c + " ")) cued = true;
                            for (auto& c : cuesAfter) if (after.startsWith (" " + c + " ")) cued = true;
                            if (! cued) continue;
                        }
                        hits.push_back ({ r, start, end });
                    }
                }
            // longest first, no overlaps, then in the order they were written
            std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return (a.end - a.start) > (b.end - b.start); });
            std::vector<Hit> kept;
            for (auto& h : hits)
            {
                bool overlap = false;
                for (auto& k : kept)
                    if (h.start < k.end && k.start < h.end) { overlap = true; break; }
                bool dup = false;
                for (auto& k : kept) if (k.ref == h.ref) dup = true;
                if (! overlap && ! dup) kept.push_back (h);
            }
            std::sort (kept.begin(), kept.end(), [] (const Hit& a, const Hit& b) { return a.start < b.start; });
            return kept;
        }
    }

    SongBrief readBrief (const juce::String& text, std::optional<Genre> pickedGenre, const juce::String& pickedStyle)
    {
        SongBrief b;
        b.text = text;
        const auto t = normaliseText (text);

        // artists and songs (their names are taken out before reading the other words: "Future" isn't the future)
        auto rest = t;
        for (auto& h : findReferences (t))
        {
            b.references.push_back (h.ref);
            b.understood.add (allReferences()[(size_t) h.ref].label());
            rest = rest.replace (" " + t.substring (h.start, h.end) + " ", "  ");
        }

        // a named style ("modern country", "boom bap"), else a genre word ("country")
        const Style* style = nullptr;
        int bestLen = 0;
        for (auto& s : allStyles())
            for (auto& k : s.keywords)
                if (k.length() > bestLen && rest.contains (" " + normaliseText (k).trim() + " "))
                {
                    style = &s;
                    bestLen = k.length();
                }
        juce::String said;
        if (style != nullptr)
        {
            b.genre = style->genre;
            b.styleId = style->id;
            b.genreSaid = true;
            b.understood.add (style->name);
        }
        else if (auto g = genreFromWords (rest, said))
        {
            b.genre = *g;
            b.genreSaid = true;
            b.understood.add (genreName (*g));
        }

        b.era = eraFromWords (rest, said);
        if (b.era >= 0) b.understood.add (eraName (b.era));
        b.bpm = bpmFromWords (rest);
        if (b.bpm > 0) b.understood.add (juce::String (juce::roundToInt (b.bpm)) + " BPM");
        b.key = parseKey (text);
        if (b.key >= 0) b.understood.add (keyName (b.key));
        for (auto& w : soundWords())
            if (rest.contains (" " + w + " ")) b.sound.addIfNotAlreadyThere (w);

        // what you picked wins; then the artist fills in what the words didn't say
        if (pickedGenre.has_value())
        {
            if (b.genre != *pickedGenre) b.styleId.clear();
            b.genre = *pickedGenre;
            b.genreSaid = true;
        }
        if (auto* ps = styleById (pickedStyle); ps != nullptr && ps->genre == b.genre) b.styleId = ps->id;

        if (! b.references.empty())
        {
            const auto& ref = allReferences()[(size_t) b.references.front()];
            const ReferenceTake* take = b.genreSaid ? ref.takeFor (b.genre) : &ref.takes.front();
            if (take != nullptr)
            {
                if (auto* s = styleById (take->styleId))
                {
                    if (! b.genreSaid) { b.genre = s->genre; b.genreSaid = true; }
                    if (b.styleId.isEmpty() && s->genre == b.genre) b.styleId = s->id;
                }
                if (b.era < 0) b.era = take->era;
                for (auto& w : take->sound) b.sound.addIfNotAlreadyThere (w);
            }
            else if (b.era < 0)
                b.era = ref.takes.front().era;   // another genre's artist: just their decade
        }
        if (b.bpm <= 0)
            if (auto* s = b.style() != nullptr ? b.style() : (stylesFor (b.genre).empty() ? nullptr : stylesFor (b.genre).front()))
                b.bpm = s->bpm;
        return b;
    }
}
