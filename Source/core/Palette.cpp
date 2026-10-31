#include "Palette.h"
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

    float scoreSound (const Sound& s, const PartSpec& part, const SongBrief& brief, juce::StringArray* reasons)
    {
        if (! s.owned) return -1.0f;
        const auto kindIt = std::find (part.kinds.begin(), part.kinds.end(), s.kind);
        if (kindIt == part.kinds.end()) return -1.0f;

        float score = 10.0f - 3.0f * (float) std::distance (part.kinds.begin(), kindIt);
        if (s.guessed) score -= 1.5f;

        for (int i = 0; i < part.want.size(); ++i)
            if (has (s, part.want[i]))
            {
                score += i < brief.sound.size() ? 4.0f : i < brief.sound.size() + 4 ? 2.5f : 1.5f;
                if (reasons != nullptr && reasons->size() < 4) reasons->addIfNotAlreadyThere (part.want[i]);
            }
        for (auto& w : part.avoid)
            if (has (s, w)) score -= 4.0f;

        for (auto& tag : genreTags (brief.genre))
            if (s.stylesText.contains (tag))
            {
                score += 3.0f;
                if (reasons != nullptr) reasons->add ("tagged " + tag);
                break;
            }
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
            std::stable_sort (ranked[p].begin(), ranked[p].end(), [] (const Pick& a, const Pick& b) { return a.score > b.score; });
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
        if (s.yours) text << " One of your own presets.";
        if (s.guessed) text << " (Its kind is a guess from its name.)";
        if (slot.locked) text << " Locked: it stays when you make new palettes.";
        return text;
    }
}
