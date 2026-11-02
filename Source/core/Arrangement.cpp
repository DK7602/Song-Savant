#include "Arrangement.h"
#include "KitMap.h"

namespace ss
{
    namespace
    {
        constexpr int barTicks = ticksPerQuarter * 4;

        // what kind of section a name means, to match a Key Compass song's sections with the style's shape
        juce::String sectionKind (const juce::String& name)
        {
            const auto n = name.toLowerCase();
            if (n.contains ("pre")) return "pre";
            if (n.contains ("chorus") || n.contains ("hook") || n.contains ("refrain") || n == "b" || n.contains ("climax")) return "chorus";
            if (n.contains ("intro") || n.contains ("opening")) return "intro";
            if (n.contains ("outro") || n.contains ("end") || n.contains ("tag") || n.contains ("release") || n.contains ("coda")) return "outro";
            if (n.contains ("bridge")) return "bridge";
            if (n.contains ("drop")) return "drop";
            if (n.contains ("build") || n.contains ("rise")) return "build";
            if (n.contains ("break")) return "break";
            if (n.contains ("solo") || n.contains ("instrumental")) return "solo";
            return "verse";
        }

        const FormSection* formFor (const StyleGuide& g, const juce::String& kind)
        {
            for (auto& f : g.form) if (sectionKind (f.name) == kind) return &f;
            // a drop is the style's chorus, a chorus is its drop
            const juce::String alias = kind == "drop" ? "chorus" : kind == "chorus" ? "drop" : kind == "solo" ? "verse" : kind == "break" ? "bridge" : juce::String();
            for (auto& f : g.form) if (alias.isNotEmpty() && sectionKind (f.name) == alias) return &f;
            for (auto& f : g.form) if (sectionKind (f.name) == "verse") return &f;
            return g.form.empty() ? nullptr : &g.form.front();
        }

        // who plays, from the shape's "drums:light bass chords" or "all"
        void fillPlay (ArrangementSection& s, const juce::String& play, const std::vector<ArrangementPart>& parts)
        {
            const auto tokens = juce::StringArray::fromTokens (play, " ", "");
            const bool all = tokens.contains ("all");
            for (auto& p : parts)
            {
                juce::String mode;
                if (all) mode = p.type == PartType::drums ? "full" : "on";
                else if (! p.extra)
                    for (auto& t : tokens)
                    {
                        if (p.type == PartType::drums && t.startsWith ("drums:")) mode = t.fromFirstOccurrenceOf (":", false, false);
                        if ((p.type == PartType::bass && t == "bass") || (p.type == PartType::chords && t == "chords")
                            || (p.type == PartType::pad && t == "pad") || (p.type == PartType::hook && t == "hook")) mode = "on";
                    }
                s.play[p.role] = mode;
            }
        }

        // the drums of a section, lighter or heavier
        void applyDrumMode (Groove& g, const juce::String& mode)
        {
            if (mode == "full") { g.crash = true; return; }
            if (mode == "groove") { g.crash = false; return; }
            if (mode == "light")
            {
                std::map<juce::String, juce::String> keep;
                for (auto& [lane, pat] : g.lanes)
                    if (lane == "kick" || lane == "hat" || lane == "rim" || lane == "shaker" || lane == "pedalhat" || lane == "tomlow")
                        keep[lane] = lane == "hat" ? pat.replaceCharacters ("xXrt", "oooo") : pat;
                g.lanes = keep;
                g.fill.clear();
                g.crash = false;
                return;
            }
            if (mode == "build")      // a build-up: kick on every beat, snare 8ths, a snare roll at the end
            {
                g.steps = 16;
                g.lanes = { { "kick", "x...x...x...x..." }, { "snare", "o.o.o.o.x.x.x.x." }, { "hat", "x.x.x.x.x.x.x.x." } };
                g.fill = { { "snare", "x.x.x.x.xxxxxxxx" }, { "hat", "................" } };
                g.crash = false;
            }
        }
    }

    std::vector<ArrangementPart> arrangementParts (const Palette& pal, const Catalog* cat)
    {
        std::vector<ArrangementPart> out;
        for (auto& slot : pal.slots)
        {
            const auto type = partTypeFor (slot.part);
            if (type == PartType::none) continue;
            ArrangementPart p;
            p.role = slot.part.role;
            p.type = type;
            p.extra = slot.part.optional;
            p.instrument = slot.part.kinds.empty() ? Kind::other : slot.part.kinds.front();
            p.label = p.role;
            if (cat != nullptr && slot.current().isValid() && slot.current().sound < (int) cat->sounds.size())
            {
                const auto& s = cat->sounds[(size_t) slot.current().sound];
                p.instrument = s.kind;
                p.label << " - " << s.name;
            }
            out.push_back (p);
        }
        return out;
    }

    DrumNotes drumNotesFor (const Palette& pal, const Catalog* cat)
    {
        for (auto& slot : pal.slots)
            if (partTypeFor (slot.part) == PartType::drums && cat != nullptr && slot.current().isValid() && slot.current().sound < (int) cat->sounds.size())
            {
                const auto& snd = cat->sounds[(size_t) slot.current().sound];
                const auto drm = drumMapsFolder().getChildFile (juce::File::createLegalFileName (snd.name).trim() + ".drm");
                if (drm.existsAsFile())
                    if (auto mapped = drumNotesFromMap (drm)) return *mapped;
            }
        return generalMidiDrums();
    }

    juce::String ArrangementSection::modeFor (const juce::String& role) const
    {
        auto it = play.find (role);
        return it != play.end() ? it->second : juce::String();
    }

    int Arrangement::totalBars() const
    {
        int n = 0;
        for (auto& s : sections) n += s.bars;
        return n;
    }

    std::unique_ptr<juce::XmlElement> Arrangement::toXml() const
    {
        auto x = std::make_unique<juce::XmlElement> ("Arrangement");
        x->setAttribute ("shape", (int) shape);
        x->setAttribute ("signature", signature);
        for (auto& s : sections)
        {
            auto* sx = x->createNewChildElement ("Section");
            sx->setAttribute ("name", s.name);
            sx->setAttribute ("bars", s.bars);
            sx->setAttribute ("progression", s.progression);
            sx->setAttribute ("chords", s.chords.joinIntoString (" "));
            for (auto& [role, mode] : s.play)
            {
                auto* px = sx->createNewChildElement ("Play");
                px->setAttribute ("role", role);
                px->setAttribute ("mode", mode);
            }
        }
        return x;
    }

    Arrangement Arrangement::fromXml (const juce::XmlElement& x)
    {
        Arrangement a;
        a.shape = (SongShape) juce::jlimit (0, 2, x.getIntAttribute ("shape", 0));
        a.signature = x.getStringAttribute ("signature");
        for (auto* sx : x.getChildWithTagNameIterator ("Section"))
        {
            ArrangementSection s;
            s.name = sx->getStringAttribute ("name");
            s.bars = juce::jlimit (1, 64, sx->getIntAttribute ("bars", 8));
            s.progression = sx->getIntAttribute ("progression", 0);
            s.chords = juce::StringArray::fromTokens (sx->getStringAttribute ("chords"), " ", "");
            s.chords.removeEmptyStrings();
            for (auto* px : sx->getChildWithTagNameIterator ("Play")) s.play[px->getStringAttribute ("role")] = px->getStringAttribute ("mode");
            a.sections.push_back (s);
        }
        return a;
    }

    juce::StringArray songShapeNames (const KeyCompassShape& kc)
    {
        juce::StringArray names { "The full song", "A short song" };
        if (! kc.empty()) names.add ("Key Compass: " + kc.name);
        return names;
    }

    juce::String arrangementSignature (const SongBrief& b, const std::vector<ArrangementPart>& parts, const KeyCompassShape& kc, SongShape shape)
    {
        juce::StringArray roles;
        for (auto& p : parts) roles.add (p.role);
        return genreId (b.genre) + "|" + b.styleId + "|" + roles.joinIntoString (",") + "|" + juce::String ((int) shape)
               + "|" + (shape == SongShape::keyCompass ? kc.name + ":" + kc.sections.joinIntoString (",") : juce::String());
    }

    Arrangement makeArrangement (const PartsPlan& plan, const std::vector<ArrangementPart>& parts, const KeyCompassShape& kc, SongShape shape)
    {
        Arrangement a;
        a.shape = shape;
        const auto& g = plan.guide;

        if (shape == SongShape::keyCompass && ! kc.empty())
        {
            // the Key Compass song's own sections, with the style deciding who plays in each kind of section
            for (int i = 0; i < kc.sections.size(); ++i)
            {
                ArrangementSection s;
                s.name = kc.sections[i];
                const auto* f = formFor (g, sectionKind (s.name));
                const auto chords = i < (int) kc.chords.size() ? kc.chords[(size_t) i] : juce::StringArray();
                if (chords.isEmpty()) s.bars = f != nullptr ? f->bars : 8;
                else s.bars = chords.size() <= 4 ? 8 : (chords.size() + 3) / 4 * 4;
                for (int bar = 0; bar < s.bars && ! chords.isEmpty(); ++bar) s.chords.add (chords[bar % chords.size()]);
                s.progression = f != nullptr ? f->progression : 0;
                fillPlay (s, f != nullptr ? f->play : juce::String ("all"), parts);
                a.sections.push_back (s);
            }
            return a;
        }

        std::vector<FormSection> form = g.form;
        if (form.empty()) form = { { "Intro", 4, "chords pad", 0 }, { "Verse", 8, "drums:groove bass chords", 1 }, { "Chorus", 8, "all", 0 },
                                   { "Verse", 8, "drums:groove bass chords", 1 }, { "Chorus", 8, "all", 0 }, { "Outro", 4, "chords pad", 0 } };
        if (shape == SongShape::shortSong)
        {
            // the radio edit: the second verse (and what follows it up to the next big section) goes; a shorter intro and outro
            std::map<juce::String, int> seen;
            for (size_t i = 0; i < form.size(); ++i)
            {
                const auto kind = sectionKind (form[i].name);
                if ((kind == "verse" || kind == "build") && ++seen[kind] == 2)
                {
                    size_t j = i + 1;
                    while (j < form.size() && sectionKind (form[j].name) != "chorus" && sectionKind (form[j].name) != "drop") ++j;
                    form.erase (form.begin() + (long) i, form.begin() + (long) std::min (j + 1, form.size()));
                    break;
                }
            }
            for (auto& f : form)
                if (sectionKind (f.name) == "intro" || sectionKind (f.name) == "outro") f.bars = std::max (4, f.bars / 2);
        }
        for (auto& f : form)
        {
            ArrangementSection s;
            s.name = f.name;
            s.bars = f.bars;
            s.progression = f.progression;
            fillPlay (s, f.play, parts);
            a.sections.push_back (s);
        }
        return a;
    }

    const juce::StringArray& drumModes()
    {
        static const juce::StringArray modes { "", "light", "groove", "build", "full" };
        return modes;
    }

    void toggleArrangementCell (Arrangement& a, int section, const ArrangementPart& part)
    {
        if (section < 0 || section >= (int) a.sections.size()) return;
        auto& mode = a.sections[(size_t) section].play[part.role];
        if (part.type == PartType::drums)
            mode = drumModes()[(drumModes().indexOf (mode) + 1) % drumModes().size()];
        else
            mode = mode.isEmpty() ? "on" : "";
    }

    void setSectionBars (Arrangement& a, int section, int bars)
    {
        if (section < 0 || section >= (int) a.sections.size()) return;
        auto& s = a.sections[(size_t) section];
        s.bars = juce::jlimit (1, 64, bars);
        if (! s.chords.isEmpty())                      // a Key Compass section keeps its chords, repeated or cut to fit
        {
            juce::StringArray c;
            for (int i = 0; i < s.bars; ++i) c.add (s.chords[i % s.chords.size()]);
            s.chords = c;
        }
    }

    void copySection (Arrangement& a, int section)
    {
        if (section < 0 || section >= (int) a.sections.size()) return;
        const auto copy = a.sections[(size_t) section];
        a.sections.insert (a.sections.begin() + section + 1, copy);
    }

    void removeSection (Arrangement& a, int section)
    {
        if (section < 0 || section >= (int) a.sections.size() || a.sections.size() <= 1) return;
        a.sections.erase (a.sections.begin() + section);
    }

    void moveSection (Arrangement& a, int section, int by)
    {
        const int to = section + by;
        if (section < 0 || section >= (int) a.sections.size() || to < 0 || to >= (int) a.sections.size()) return;
        std::swap (a.sections[(size_t) section], a.sections[(size_t) to]);
    }

    std::vector<Chord> sectionChords (const PartsPlan& plan, const ArrangementSection& s)
    {
        std::vector<Chord> out;
        for (auto& name : s.chords)
            if (auto c = chordFromName (name)) out.push_back (*c);
        if (! out.empty())
        {
            const auto n = out.size();
            for (size_t i = n; i < (size_t) s.bars; ++i) out.push_back (out[i % n]);
            out.resize ((size_t) s.bars);
            return out;
        }
        // the song's own chords (a Key Compass song picked for the starter parts) stand for every section
        if (! plan.chordSource.contains ("Key Compass")) return progressionChords (plan, s.progression, s.bars);
        for (int bar = 0; bar < s.bars; ++bar) out.push_back (plan.chords[(size_t) bar % plan.chords.size()]);
        return out;
    }

    std::vector<NamedPart> arrangeParts (const PartsPlan& plan, const Arrangement& a, const std::vector<ArrangementPart>& parts, const DrumNotes& drums)
    {
        std::vector<NamedPart> out;
        for (auto& part : parts)
        {
            NamedPart np { part.label, {} };
            int bar = 0;
            for (auto& s : a.sections)
            {
                const auto mode = s.modeFor (part.role);
                if (mode.isNotEmpty())
                {
                    auto sp = plan;
                    sp.bars = s.bars;
                    sp.chords = sectionChords (plan, s);
                    if (part.type == PartType::drums) applyDrumMode (sp.guide.groove, mode);
                    np.notes.addSequence (makePart (sp, part.type, drums, part.instrument), (double) bar * barTicks);
                }
                bar += s.bars;
            }
            np.notes.sort();
            np.notes.updateMatchedPairs();
            out.push_back (std::move (np));
        }
        return out;
    }

    bool writeArrangement (const PartsPlan& plan, const Arrangement& a, const std::vector<NamedPart>& parts, const juce::File& file)
    {
        std::vector<std::pair<double, juce::String>> markers;
        int bar = 0;
        for (auto& s : a.sections)
        {
            markers.emplace_back ((double) bar * barTicks, s.name);
            bar += s.bars;
        }
        return writeMidiFile (plan, parts, file, markers, a.totalBars());
    }
}
