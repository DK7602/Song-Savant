#include "Palette.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <random>

namespace ss
{
    juce::String soundKey (const Sound& s)   { return (s.name + "|" + s.instrument + "|" + s.library).toLowerCase(); }

    int findSoundByKey (const Catalog& c, const juce::String& key)
    {
        for (int i = 0; i < (int) c.sounds.size(); ++i)
            if (soundKey (c.sounds[(size_t) i]) == key) return i;
        return -1;
    }

    namespace
    {
        // How each library tags the genres
        juce::StringArray genreTags (Genre g)
        {
            switch (g)
            {
                case Genre::country:   return { "country", "americana", "folk", "bluegrass" };
                case Genre::worship:   return { "gospel", "worship", "ambient", "pop", "soundtrack" };
                case Genre::pop:       return { "pop" };
                case Genre::rock:      return { "rock", "metal", "alternative", "indie" };
                case Genre::hiphop:    return { "hip hop", "hip-hop", "urban", "rap" };
                case Genre::trap:      return { "trap", "hip hop", "urban", "rap" };
                case Genre::rnb:       return { "r&b", "urban", "soul", "funk" };
                case Genre::edm:       return { "electronica", "dance", "edm", "house", "techno", "trance" };
                case Genre::lofi:      return { "lofi", "lo-fi", "chill", "downtempo", "jazz" };
                case Genre::cinematic: return { "cinematic", "soundtrack", "orchestral", "ambient" };
                case Genre::count:     break;
            }
            return {};
        }

        bool has (const Sound& s, const juce::String& word)
        {
            // whole words where it matters ("sub" shouldn't match "subtle", "ep" shouldn't match "deep")
            return word.length() <= 4 ? s.searchText.contains (" " + word + " ") : s.searchText.contains (word);
        }
    }

    DrumForm drumForm (const Sound& s)
    {
        if (s.kind != Kind::drums) return DrumForm::unknown;
        // Groove Agent and Drum Machine presets are always kits
        if (s.instrument.containsIgnoreCase ("Groove Agent") || s.instrument.containsIgnoreCase ("Drum Machine")) return DrumForm::kit;
        if (isGrooveGrabberKit (s)) return DrumForm::kit;
        const auto d = s.detail.toLowerCase();
        if (d.contains ("beats") || d.contains ("loop") || s.character.contains ("Loop", true) || has (s, "loop") || has (s, "loops"))
            return DrumForm::loop;
        if (d.contains ("drumset") || d.contains ("kit") || has (s, "kit") || has (s, "drumset") || has (s, "drum kit"))
            return DrumForm::kit;
        static const char* singles[] = { "kick", "kicks", "snare", "snares", "hihat", "hihats", "hi hat", "hats", "hat", "tom", "toms",
                                         "cymbal", "cymbals", "clap", "claps", "rim", "crash", "ride", "one shot" };
        for (auto* w : singles)
            if (d.contains (w) || has (s, w)) return DrumForm::single;
        // anything else that's a drum sound is played as a kit (Analog Lab and HALion kits too: if Cubase shows no pad
        // names, the Drum Maps page makes them)
        return DrumForm::kit;
    }

    float scoreSound (const Sound& s, const PartSpec& part, const SongBrief& brief, juce::StringArray* reasons)
    {
        if (! s.owned) return -1.0f;
        const auto kindIt = std::find (part.kinds.begin(), part.kinds.end(), s.kind);
        if (kindIt == part.kinds.end()) return -1.0f;

        float score = 10.0f - 3.0f * (float) std::distance (part.kinds.begin(), kindIt);
        if (s.guessed) score -= 1.5f;
        if (part.kinds.front() == Kind::drums)
            switch (drumForm (s))
            {
                case DrumForm::kit:     score += 4.0f; break;     // you play your own beat on it (808 included)
                case DrumForm::loop:    score -= 8.0f; break;     // plays its own groove: hard to use as your drum track
                case DrumForm::single:  score -= 8.0f; break;     // one drum, not a kit
                case DrumForm::unknown: break;
            }

        for (int i = 0; i < part.want.size(); ++i)
            if (has (s, part.want[i]))
            {
                score += i < brief.sound.size() ? 4.0f : i < brief.sound.size() + 4 ? 2.5f : 1.5f;
                if (reasons != nullptr && reasons->size() < 4) reasons->addIfNotAlreadyThere (part.want[i]);
            }
        for (auto& w : part.avoid)
            if (has (s, w)) score -= 4.0f;

        bool genreTagged = false;
        for (auto& tag : genreTags (brief.genre))
            if (s.stylesText.contains (tag))
            {
                score += 3.0f;
                genreTagged = true;
                if (reasons != nullptr) reasons->add ("tagged " + tag);
                break;
            }
        if (isGrooveGrabberKit (s))
        {
            // a kit you built in Groove Grabber from your own samples: made for this very style, it's the one to use;
            // made for another genre, it's a long shot
            score += genreTagged ? 1.0f : -3.0f;
            if (auto* st = brief.style(); st != nullptr && ! st->isGeneral() && s.styles.contains (st->name, true))
            {
                score += 3.0f;
                if (reasons != nullptr) reasons->add ("tagged " + st->name);
            }
        }
        if (s.source == "Plug-in") score -= 3.0f;    // a plug-in to explore is less specific than an actual preset
        score += 0.4f * (float) s.rating;
        if (s.yours) score += 0.5f;
        return score;
    }

    std::vector<Palette> makePalettes (const Catalog& c, const SongBrief& brief, const PaletteOptions& opt, const std::vector<PaletteSlot>& keep)
    {
        const auto parts = partsFor (brief);
        std::mt19937 rng (opt.seed);

        // every part's candidates, best first
        std::vector<std::vector<Pick>> ranked (parts.size());
        for (size_t p = 0; p < parts.size(); ++p)
        {
            for (int i = 0; i < (int) c.sounds.size(); ++i)
            {
                juce::StringArray why;
                const float sc = scoreSound (c.sounds[(size_t) i], parts[p], brief, &why);
                if (sc > 0.0f) ranked[p].push_back ({ i, sc, why });
            }
            // equal scores: a fair shuffle (the same for the same song and seed), not alphabetical order
            auto tie = [&c, &parts, p, &opt] (int i)
            {
                return (juce::uint64) (c.sounds[(size_t) i].name + "|" + parts[p].role + "|" + juce::String ((int) opt.seed)).hashCode64();
            };
            std::stable_sort (ranked[p].begin(), ranked[p].end(), [&tie] (const Pick& a, const Pick& b)
            {
                if (std::abs (a.score - b.score) > 0.001f) return a.score > b.score;
                return tie (a.sound) < tie (b.sound);
            });
            if (ranked[p].size() > 60) ranked[p].resize (60);
        }

        std::vector<Palette> out;
        std::vector<std::set<int>> usedPerPart (parts.size());
        for (int n = 0; n < opt.palettes; ++n)
        {
            Palette pal;
            pal.name = "Palette " + juce::String (n + 1);
            std::set<int> usedHere;
            for (size_t p = 0; p < parts.size(); ++p)
            {
                PaletteSlot slot;
                slot.part = parts[p];

                // a locked part keeps its sound
                for (auto& k : keep)
                    if (k.locked && k.part.role == parts[p].role)
                    {
                        const int idx = k.current().isValid() && k.current().sound < (int) c.sounds.size()
                                          && soundKey (c.sounds[(size_t) k.current().sound]) == k.keepKey
                                          ? k.current().sound : findSoundByKey (c, k.keepKey);
                        if (idx >= 0)
                        {
                            juce::StringArray why;
                            slot.pick = { idx, scoreSound (c.sounds[(size_t) idx], parts[p], brief, &why), why };
                            slot.locked = true;
                            slot.keepKey = k.keepKey;
                        }
                    }

                const auto& cands = ranked[p];
                if (! slot.pick.isValid() && ! cands.empty())
                {
                    // palette 1 takes the best; later palettes (and new seeds) pick among the strong ones not used yet
                    std::vector<const Pick*> fresh;
                    for (auto& cand : cands)
                        if (usedPerPart[p].count (cand.sound) == 0 && usedHere.count (cand.sound) == 0) fresh.push_back (&cand);
                    if (fresh.empty()) for (auto& cand : cands) if (usedHere.count (cand.sound) == 0) fresh.push_back (&cand);
                    if (! fresh.empty())
                    {
                        size_t choice = 0;
                        if (n > 0 || opt.seed != 1)
                        {
                            const float top = fresh.front()->score;
                            size_t pool = 1;
                            while (pool < fresh.size() && pool < 8 && fresh[pool]->score >= top - 4.0f) ++pool;
                            choice = std::uniform_int_distribution<size_t> (0, pool - 1) (rng);
                        }
                        slot.pick = *fresh[choice];
                    }
                }
                if (slot.pick.isValid())
                {
                    usedPerPart[p].insert (slot.pick.sound);
                    usedHere.insert (slot.pick.sound);
                    if (slot.keepKey.isEmpty()) slot.keepKey = soundKey (c.sounds[(size_t) slot.pick.sound]);
                }
                for (auto& cand : cands)
                {
                    if ((int) slot.alternatives.size() >= opt.alternatives) break;
                    if (cand.sound != slot.pick.sound) slot.alternatives.push_back (cand);
                }
                // keep up to two of the part's other kind of sound among the choices (deep subs for a bass above the 808)
                if (! slot.part.alsoOffer.isEmpty())
                {
                    auto isOther = [&] (const Pick& x)
                    {
                        if (! x.isValid()) return false;
                        for (auto& w : slot.part.alsoOffer) if (has (c.sounds[(size_t) x.sound], w)) return true;
                        return false;
                    };
                    int others = isOther (slot.pick) ? 1 : 0;
                    for (auto& a : slot.alternatives) if (isOther (a)) ++others;
                    for (auto& cand : cands)
                    {
                        if (others >= 2) break;
                        if (! isOther (cand) || cand.sound == slot.pick.sound) continue;
                        bool there = false;
                        for (auto& a : slot.alternatives) there = there || a.sound == cand.sound;
                        if (there) continue;
                        slot.alternatives.push_back (cand);
                        ++others;
                    }
                }
                // always offer up to two instruments whose presets Song Savant can't list (SSD Sampler for the drums,
                // even when there are dozens of Groove Agent kits ahead of it)
                {
                    auto isPlugin = [&c] (const Pick& x) { return x.isValid() && c.sounds[(size_t) x.sound].source == "Plug-in"; };
                    int plugins = isPlugin (slot.pick) ? 1 : 0;
                    for (auto& a : slot.alternatives) if (isPlugin (a)) ++plugins;
                    for (auto& cand : cands)
                    {
                        if (plugins >= 2) break;
                        if (! isPlugin (cand) || cand.sound == slot.pick.sound) continue;
                        bool there = false;
                        for (auto& a : slot.alternatives) there = there || a.sound == cand.sound;
                        if (there) continue;
                        slot.alternatives.push_back (cand);
                        ++plugins;
                    }
                }
                pal.slots.push_back (std::move (slot));
            }
            out.push_back (std::move (pal));
        }
        return out;
    }

    juce::String whyItWorks (const Catalog& c, const PaletteSlot& slot)
    {
        if (! slot.current().isValid()) return "Nothing in your library fits this part yet.";
        const auto& s = c.sounds[(size_t) slot.current().sound];
        juce::StringArray words, tags;
        for (auto& r : slot.current().reasons)
            (r.startsWith ("tagged ") ? tags : words).add (r.startsWith ("tagged ") ? r.substring (7) : r);
        juce::String text = (s.detail.isNotEmpty() ? s.detail : kindName (s.kind)) + " for the " + slot.part.role + " part";
        if (! words.isEmpty()) text << ": " << words.joinIntoString (", ");
        text << ".";
        if (! tags.isEmpty()) text << " Tagged " << tags.joinIntoString (", ") << " in your library.";
        if (isGrooveGrabberKit (s)) text << " Your own kit from Groove Grabber.";
        else if (s.yours) text << " One of your own presets.";
        if (s.guessed) text << " (Its kind is a guess from its name.)";
        if (slot.locked) text << " Locked: it stays when you make new palettes.";
        return text;
    }

    juce::StringArray whyItWorksPoints (const Catalog& c, const PaletteSlot& slot)
    {
        juce::StringArray out;
        if (! slot.current().isValid()) { out.add ("Nothing in your library fits this part yet"); return out; }
        const auto& s = c.sounds[(size_t) slot.current().sound];
        juce::StringArray words, tags;
        for (auto& r : slot.current().reasons)
            (r.startsWith ("tagged ") ? tags : words).add (r.startsWith ("tagged ") ? r.substring (7) : r);
        const auto what = s.detail.isNotEmpty() && s.source != "Plug-in" ? s.detail : kindName (s.kind);
        out.add (what + " for the " + slot.part.role + " part");
        if (slot.part.kinds.front() == Kind::drums)
            switch (drumForm (s))
            {
                case DrumForm::kit:     out.add ("A full kit: you play your own beat on it"); break;
                case DrumForm::loop:    out.add ("A ready-made loop: it plays its own beat"); break;
                case DrumForm::single:  out.add ("One drum sound, not a whole kit"); break;
                case DrumForm::unknown: break;
            }
        {
            bool other = false;
            for (auto& w : slot.part.alsoOffer) other = other || has (s, w);
            const auto& note = other ? slot.part.alsoOfferNote : slot.part.note;
            if (note.isNotEmpty()) out.add (note);
        }
        if (! words.isEmpty()) out.add ("Matches what the part needs: " + words.joinIntoString (", "));
        if (! tags.isEmpty()) out.add ("Tagged " + tags.joinIntoString (", ") + " in your library");
        if (s.source == "Plug-in")
        {
            if (s.description.isNotEmpty()) out.add (s.description);
            out.add (s.detail);
        }
        if (isGrooveGrabberKit (s))
            out.add ("Your own kit, built in Groove Grabber from your samples" + (s.styles.isEmpty() ? juce::String() : " (made for " + s.styles[0] + ")"));
        else if (s.yours) out.add ("One of your own presets");
        if (s.guessed) out.add ("Its kind is a guess from its name");
        if (slot.locked) out.add ("Locked: it stays when you make new palettes");
        return out;
    }
}
