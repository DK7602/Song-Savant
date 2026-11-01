#include "SongSession.h"

namespace ss
{
    SongBrief SongSession::makeBrief() const
    {
        std::optional<Genre> g;
        if (form.genre >= 0 && form.genre < numGenres) g = (Genre) form.genre;
        auto b = readBrief (form.description, g, form.styleId);
        if (form.key >= 0) b.key = form.key;
        if (form.bpm > 0) b.bpm = form.bpm;
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
        const auto* saved = pending->getChildElement (juce::jlimit (0, juce::jmax (0, pending->getNumChildElements() - 1), current));
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
