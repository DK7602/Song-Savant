#include "SongSession.h"
#include "DrumLanes.h"

namespace ss
{
    SongBrief SongSession::makeBrief() const
    {
        std::optional<Genre> g;
        if (form.genre >= 0 && form.genre < numGenres) g = (Genre) form.genre;
        auto b = readBrief (form.description, g, form.styleId);
        if (form.key >= 0) b.key = form.key;
        if (form.bpm > 0) b.bpm = form.bpm;
        else if (hostBpm > 0) b.bpm = hostBpm;          // inside a DAW: its tempo, unless the BPM box says otherwise
        return b;
    }

    void SongSession::find (const Catalog& c)
    {
        brief = makeBrief();
        seed = 1;
        partsSeed = 1;
        palettes = makePalettes (c, brief, { 3, 6, seed });
        current = 0;
        pending.reset();
    }

    void SongSession::newPalettes (const Catalog& c)
    {
        brief = makeBrief();
        std::vector<PaletteSlot> keep;
        if (auto* p = currentPalette())
            for (auto& s : p->slots)
                if (s.locked) keep.push_back (s);
        ++seed;
        palettes = makePalettes (c, brief, { 3, 6, seed }, keep);
        current = 0;
    }

    void SongSession::forgetSound (const Catalog& c, const juce::String& key)
    {
        auto isIt = [&c, &key] (const Pick& x)
        {
            return x.isValid() && x.sound < (int) c.sounds.size() && soundKey (c.sounds[(size_t) x.sound]) == key;
        };
        for (auto& p : palettes)
            for (auto& slot : p.slots)
            {
                std::vector<Pick> all;
                if (slot.pick.isValid()) all.push_back (slot.pick);
                for (auto& a : slot.alternatives) all.push_back (a);
                const bool wasCurrent = isIt (slot.current());
                std::vector<Pick> keep;
                int choice = 0;
                for (int i = 0; i < (int) all.size(); ++i)
                {
                    if (i == slot.choice) choice = (int) keep.size();     // (the one after it, when it goes)
                    if (! isIt (all[(size_t) i])) keep.push_back (all[(size_t) i]);
                }
                if (keep.size() == all.size()) continue;
                if (wasCurrent) { slot.locked = false; slot.keepKey.clear(); }
                slot.pick = keep.empty() ? Pick() : keep.front();
                slot.alternatives.assign (keep.empty() ? keep.end() : keep.begin() + 1, keep.end());
                slot.choice = keep.empty() ? 0 : juce::jlimit (0, (int) keep.size() - 1, choice);
            }
    }

    bool SongSession::putSound (const Catalog& c, int sound, const juce::String& role)
    {
        auto* pal = currentPalette();
        if (pal == nullptr || sound < 0 || sound >= (int) c.sounds.size()) return false;
        for (auto& slot : pal->slots)
        {
            if (slot.part.role != role) continue;
            // already one of its choices: that one; otherwise first among them
            int at = -1;
            if (slot.pick.sound == sound) at = 0;
            for (size_t i = 0; i < slot.alternatives.size() && at < 0; ++i)
                if (slot.alternatives[i].sound == sound) at = (int) i + 1;
            if (at < 0)
            {
                Pick pk;
                pk.sound = sound;
                juce::StringArray why;
                pk.score = scoreSound (c.sounds[(size_t) sound], slot.part, brief, &why);
                pk.reasons = why;
                pk.reasons.insert (0, "One of your favorites");
                if (! slot.pick.isValid()) { slot.pick = pk; at = 0; }
                else { slot.alternatives.insert (slot.alternatives.begin(), pk); at = 1; }
            }
            slot.choice = at;
            slot.locked = true;
            slot.keepKey = soundKey (c.sounds[(size_t) sound]);
            return true;
        }
        return false;
    }

    void SongSession::resetArrangement (const std::vector<ArrangementPart>& parts)
    {
        auto shape = songShape;
        if (shape == SongShape::keyCompass && form.sections.isEmpty()) shape = SongShape::full;
        arrangement = makeArrangement (partsPlan(), parts, keyCompassShape(), shape);
        arrangement.signature = arrangementSignature (makeBrief(), parts, keyCompassShape(), shape);
    }

    void SongSession::updateArrangement (const std::vector<ArrangementPart>& parts)
    {
        auto shape = songShape;
        if (shape == SongShape::keyCompass && form.sections.isEmpty()) shape = SongShape::full;
        if (arrangement.empty() || arrangement.signature != arrangementSignature (makeBrief(), parts, keyCompassShape(), shape))
            resetArrangement (parts);
    }

    //==============================================================================
    void SongSession::setSongShape (SongShape shape)
    {
        if (shape == songShape) return;
        if ((shape == SongShape::blank) != (songShape == SongShape::blank))
        {
            std::swap (patterns, patternsAside);
            std::swap (arrangement, arrangementAside);
        }
        songShape = shape;
    }

    bool SongSession::isMine (int section, const juce::String& role) const
    {
        if (section < 0 || section >= (int) arrangement.sections.size()) return false;
        return patterns.find (role, arrangement.sections[(size_t) section].patternFor (role)) != nullptr;
    }

    Pattern SongSession::patternAt (int section, const ArrangementPart& part, const DrumNotes& drums) const
    {
        const auto plan = partsPlan();
        Pattern p;
        p.key = plan.key;
        if (section < 0 || section >= (int) arrangement.sections.size()) return p;
        const auto& s = arrangement.sections[(size_t) section];
        if (const auto* mine = patterns.find (part.role, s.patternFor (part.role)))
        {
            p = *mine;
            if (! p.drums) p.notes = moveToKey (p.notes, mine->key, plan.key);
            p.key = plan.key;
            return p;
        }
        p.bars = s.bars;
        p.drums = part.type == PartType::drums;
        if (arrangement.shape == SongShape::blank) return p;       // a blank song: an empty grid to write your own
        p.notes = suggestedNotes (plan, s, part, drums);
        if (p.drums) p.notes = drumsToLanes (p.notes, drums);      // drums are kept by drum, not by the kit's notes
        return p;
    }

    void SongSession::storePattern (int section, const ArrangementPart& part, Pattern p)
    {
        if (section < 0 || section >= (int) arrangement.sections.size()) return;
        const auto name = arrangement.sections[(size_t) section].patternFor (part.role);
        p.key = partsPlan().key;
        p.drums = part.type == PartType::drums;
        patterns.set (part.role, name, std::move (p));
        for (int i : sectionsUsing (arrangement, part.role, name))
        {
            auto& mode = arrangement.sections[(size_t) i].play[part.role];
            if (mode.isEmpty()) mode = part.type == PartType::drums ? "groove" : "on";
        }
    }

    void SongSession::forgetPattern (int section, const juce::String& role)
    {
        if (section < 0 || section >= (int) arrangement.sections.size()) return;
        patterns.remove (role, arrangement.sections[(size_t) section].patternFor (role));
    }

    juce::String SongSession::makeDifferent (int section, const ArrangementPart& part, const DrumNotes& drums)
    {
        if (section < 0 || section >= (int) arrangement.sections.size()) return {};
        const bool mine = isMine (section, part.role);
        const auto copy = patternAt (section, part, drums);
        const auto name = makeSectionDifferent (arrangement, section, part.role);
        if (mine) patterns.set (part.role, name, copy);     // your notes come with it; the suggestion stays the suggestion
        return name;
    }

    std::unique_ptr<juce::XmlElement> SongSession::toXml() const
    {
        auto x = std::make_unique<juce::XmlElement> ("Song");
        x->setAttribute ("description", form.description);
        x->setAttribute ("genre", form.genre);
        x->setAttribute ("style", form.styleId);
        x->setAttribute ("key", form.key);
        x->setAttribute ("bpm", form.bpm);
        x->setAttribute ("fromSong", form.fromSong);
        x->setAttribute ("chords", form.chords.joinIntoString (" "));
        x->setAttribute ("partsSeed", (int) partsSeed);
        x->setAttribute ("songShape", (int) songShape);
        if (savedAs.isNotEmpty()) x->setAttribute ("savedAs", savedAs);
        if (! arrangement.empty()) x->addChildElement (arrangement.toXml().release());
        if (! patterns.empty()) x->addChildElement (patterns.toXml().release());
        if (! arrangementAside.empty())
        {
            auto aside = arrangementAside.toXml();
            aside->setTagName ("ArrangementAside");
            x->addChildElement (aside.release());
        }
        if (! patternsAside.empty())
        {
            auto aside = patternsAside.toXml();
            aside->setTagName ("PatternsAside");
            x->addChildElement (aside.release());
        }
        for (int i = 0; i < form.sections.size(); ++i)
        {
            auto* sx = x->createNewChildElement ("SongSection");
            sx->setAttribute ("name", form.sections[i]);
            sx->setAttribute ("chords", i < (int) form.sectionChords.size() ? form.sectionChords[(size_t) i].joinIntoString (" ") : juce::String());
        }
        x->setAttribute ("seed", (int) seed);
        x->setAttribute ("current", current);
        x->setAttribute ("made", ! palettes.empty() || (pending != nullptr && pending->getBoolAttribute ("made")));
        if (! palettes.empty())
            for (auto& p : palettes)
            {
                auto* px = x->createNewChildElement ("Palette");
                for (auto& s : p.slots)
                {
                    auto* sx = px->createNewChildElement ("Slot");
                    sx->setAttribute ("role", s.part.role);
                    sx->setAttribute ("choice", s.choice);
                    sx->setAttribute ("locked", s.locked);
                    sx->setAttribute ("keep", s.keepKey);
                }
            }
        else if (pending != nullptr)
            for (auto* px : pending->getChildWithTagNameIterator ("Palette"))
                x->addChildElement (new juce::XmlElement (*px));
        return x;
    }

    void SongSession::fromXml (const juce::XmlElement& x)
    {
        form.description = x.getStringAttribute ("description");
        form.genre = juce::jlimit (-1, numGenres - 1, x.getIntAttribute ("genre", -1));
        form.styleId = x.getStringAttribute ("style");
        form.key = juce::jlimit (-1, 23, x.getIntAttribute ("key", -1));
        form.bpm = juce::jlimit (0.0, 300.0, x.getDoubleAttribute ("bpm", 0.0));
        form.fromSong = x.getStringAttribute ("fromSong");
        form.chords = juce::StringArray::fromTokens (x.getStringAttribute ("chords"), " ", "");
        form.chords.removeEmptyStrings();
        partsSeed = (juce::uint32) juce::jmax (1, x.getIntAttribute ("partsSeed", 1));
        songShape = (SongShape) juce::jlimit (0, 3, x.getIntAttribute ("songShape", 0));
        savedAs = x.getStringAttribute ("savedAs");
        arrangement = {};
        if (auto* ax = x.getChildByName ("Arrangement")) arrangement = Arrangement::fromXml (*ax);
        arrangementAside = {};
        if (auto* ax = x.getChildByName ("ArrangementAside")) arrangementAside = Arrangement::fromXml (*ax);
        patterns.clear();
        if (auto* px = x.getChildByName ("Patterns")) patterns = PatternBook::fromXml (*px);
        patternsAside.clear();
        if (auto* px = x.getChildByName ("PatternsAside")) patternsAside = PatternBook::fromXml (*px);
        form.sections.clear();
        form.sectionChords.clear();
        for (auto* sx : x.getChildWithTagNameIterator ("SongSection"))
        {
            form.sections.add (sx->getStringAttribute ("name"));
            auto c = juce::StringArray::fromTokens (sx->getStringAttribute ("chords"), " ", "");
            c.removeEmptyStrings();
            form.sectionChords.push_back (c);
        }
        seed = (juce::uint32) juce::jmax (1, x.getIntAttribute ("seed", 1));
        current = x.getIntAttribute ("current", 0);
        palettes.clear();
        pending = x.getBoolAttribute ("made") ? std::make_unique<juce::XmlElement> (x) : nullptr;
    }

    void SongSession::restore (const Catalog& c)
    {
        if (pending == nullptr) return;
        brief = makeBrief();

        // the same song and seed make the same palettes; then put back what you'd locked and switched
        std::vector<PaletteSlot> keep;
        const juce::XmlElement* saved = nullptr;          // the palette that was showing (by its place among the palettes)
        {
            int i = 0;
            for (auto* px : pending->getChildWithTagNameIterator ("Palette"))
            {
                saved = px;                           // ends on the current one, or the last one
                if (i++ == current) break;
            }
        }
        if (saved != nullptr)
            for (auto* sx : saved->getChildWithTagNameIterator ("Slot"))
                if (sx->getBoolAttribute ("locked"))
                {
                    PaletteSlot s;
                    s.part.role = sx->getStringAttribute ("role");
                    s.keepKey = sx->getStringAttribute ("keep");
                    s.locked = true;
                    keep.push_back (s);
                }
        palettes = makePalettes (c, brief, { 3, 6, seed }, keep);

        int pi = 0;
        for (auto* px : pending->getChildWithTagNameIterator ("Palette"))
        {
            if (pi >= (int) palettes.size()) break;
            auto& pal = palettes[(size_t) pi++];
            for (auto* sx : px->getChildWithTagNameIterator ("Slot"))
                for (auto& s : pal.slots)
                    if (s.part.role == sx->getStringAttribute ("role") && ! s.locked)
                    {
                        // the switched-to sound, found by name in case the library changed
                        const auto key = sx->getStringAttribute ("keep");
                        for (int i = 0; i < s.numOptions(); ++i)
                        {
                            s.choice = i;
                            if (s.current().isValid() && soundKey (c.sounds[(size_t) s.current().sound]) == key) break;
                            if (i == s.numOptions() - 1) s.choice = 0;
                        }
                    }
        }
        current = juce::jlimit (0, juce::jmax (0, (int) palettes.size() - 1), current);
        pending.reset();
    }

    void SongSession::moveTo (const Catalog& c)
    {
        if (palettes.empty()) return;
        const auto saved = toXml();      // sounds are saved by name, so this finds them again in the new catalog
        fromXml (*saved);
        restore (c);
    }
}
