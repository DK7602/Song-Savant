#include "Arrangement.h"
#include "KitMap.h"
#include "GrooveGrabberLink.h"
#include "DrumLanes.h"

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
                if (isGrooveGrabberKit (snd) || snd.instrument == "Groove Grabber") return grooveGrabberDrums();
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

    juce::String ArrangementSection::patternFor (const juce::String& role) const
    {
        auto it = patterns.find (role);
        return it != patterns.end() && it->second.isNotEmpty() ? it->second : name;
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
            if (s.ownChords) sx->setAttribute ("ownChords", true);
            for (auto& [role, mode] : s.play)
            {
                auto* px = sx->createNewChildElement ("Play");
                px->setAttribute ("role", role);
                px->setAttribute ("mode", mode);
            }
            for (auto& [role, pattern] : s.patterns)
            {
                auto* px = sx->createNewChildElement ("OwnPattern");
                px->setAttribute ("role", role);
                px->setAttribute ("name", pattern);
            }
        }
        return x;
    }

    Arrangement Arrangement::fromXml (const juce::XmlElement& x)
    {
        Arrangement a;
        a.shape = (SongShape) juce::jlimit (0, 3, x.getIntAttribute ("shape", 0));
        a.signature = x.getStringAttribute ("signature");
        for (auto* sx : x.getChildWithTagNameIterator ("Section"))
        {
            ArrangementSection s;
            s.name = sx->getStringAttribute ("name");
            s.bars = juce::jlimit (1, 64, sx->getIntAttribute ("bars", 8));
            s.progression = sx->getIntAttribute ("progression", 0);
            s.chords = juce::StringArray::fromTokens (sx->getStringAttribute ("chords"), " ", "");
            s.chords.removeEmptyStrings();
            s.ownChords = sx->getBoolAttribute ("ownChords", false);
            for (auto* px : sx->getChildWithTagNameIterator ("Play")) s.play[px->getStringAttribute ("role")] = px->getStringAttribute ("mode");
            for (auto* px : sx->getChildWithTagNameIterator ("OwnPattern")) s.patterns[px->getStringAttribute ("role")] = px->getStringAttribute ("name");
            a.sections.push_back (s);
        }
        return a;
    }

    juce::StringArray songShapeNames (const KeyCompassShape& kc)
    {
        juce::StringArray names { "The full song", "A short song" };
        if (! kc.empty()) names.add ("Key Compass: " + kc.name);
        names.add ("A blank song");
        return names;
    }

    std::vector<SongShape> songShapes (const KeyCompassShape& kc)
    {
        std::vector<SongShape> shapes { SongShape::full, SongShape::shortSong };
        if (! kc.empty()) shapes.push_back (SongShape::keyCompass);
        shapes.push_back (SongShape::blank);
        return shapes;
    }

    juce::String arrangementSignature (const SongBrief& b, const std::vector<ArrangementPart>& parts, const KeyCompassShape& kc, SongShape shape)
    {
        // a blank song is yours: a new style or new parts leave it alone (a part that wasn't there before plays nowhere yet)
        if (shape == SongShape::blank) return "blank";
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

        if (shape == SongShape::blank)
        {
            // one empty section to start from: you add the rest, switch the parts on and write their notes
            ArrangementSection s;
            s.name = "Verse";
            s.bars = 8;
            const auto* f = formFor (g, "verse");
            s.progression = f != nullptr ? f->progression : 1;
            for (auto& p : parts) s.play[p.role] = {};
            a.sections.push_back (s);
            return a;
        }

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

    void moveSectionTo (Arrangement& a, int section, int gap)
    {
        const int n = (int) a.sections.size();
        if (section < 0 || section >= n || gap < 0 || gap > n || gap == section || gap == section + 1) return;
        auto moving = a.sections[(size_t) section];
        a.sections.erase (a.sections.begin() + section);
        a.sections.insert (a.sections.begin() + (gap > section ? gap - 1 : gap), std::move (moving));
    }

    void addSection (Arrangement& a, int at, const juce::String& name, const PartsPlan& plan, const std::vector<ArrangementPart>& parts)
    {
        at = juce::jlimit (0, (int) a.sections.size(), at);
        const auto trimmed = name.trim();
        if (trimmed.isEmpty()) return;
        // one more of a section the song has: like the first of them (so it plays the same pattern)
        for (auto& existing : a.sections)
            if (existing.name.equalsIgnoreCase (trimmed))
            {
                auto copy = existing;
                a.sections.insert (a.sections.begin() + at, std::move (copy));
                return;
            }
        // a new kind: what the style does in one
        ArrangementSection s;
        s.name = trimmed;
        const auto* f = formFor (plan.guide, sectionKind (trimmed));
        const auto kind = sectionKind (trimmed);
        s.bars = f != nullptr ? f->bars : 8;
        if ((kind == "intro" || kind == "outro") && (f == nullptr || sectionKind (f->name) != kind)) s.bars = 4;
        s.progression = f != nullptr ? f->progression : 0;
        if (kind == "bridge" || kind == "break") s.progression = 2;
        if (a.shape == SongShape::blank) for (auto& p : parts) s.play[p.role] = {};     // a blank song: nothing plays yet
        else fillPlay (s, f != nullptr ? f->play : juce::String ("all"), parts);
        a.sections.insert (a.sections.begin() + at, std::move (s));
    }

    juce::StringArray sectionNamesInSong (const Arrangement& a)
    {
        juce::StringArray names;
        for (auto& s : a.sections) names.addIfNotAlreadyThere (s.name, true);
        return names;
    }

    juce::StringArray newSectionNames (const StyleGuide& g, const Arrangement& a)
    {
        // the style's own names first ("Hook" in rap, "Drop" in dance music), then the usual ones it doesn't name
        juce::StringArray styleNames, out;
        for (auto& f : g.form) styleNames.addIfNotAlreadyThere (f.name, true);
        juce::StringArray candidates = styleNames;
        for (auto* usual : { "Intro", "Verse", "Pre-Chorus", "Chorus", "Post-Chorus", "Bridge", "Breakdown", "Build", "Drop", "Solo", "Outro" })
        {
            const juce::String name (usual);
            bool named = false;                         // the style calls it something else
            if (! name.containsIgnoreCase ("post"))
                for (auto& sn : styleNames) if (sectionKind (sn) == sectionKind (name) && ! sn.containsIgnoreCase ("post")) named = true;
            if (! named) candidates.addIfNotAlreadyThere (name, true);
        }
        const auto inSong = sectionNamesInSong (a);
        for (auto& c : candidates) if (! inSong.contains (c, true)) out.add (c);
        return out;
    }

    std::vector<Chord> styleChords (const PartsPlan& plan, const ArrangementSection& s)
    {
        auto plain = s;
        if (plain.ownChords) plain.chords.clear();
        return sectionChords (plan, plain);
    }

    void setSectionChord (Arrangement& a, const PartsPlan& plan, int section, int bar, const juce::String& chord)
    {
        if (section < 0 || section >= (int) a.sections.size()) return;
        const auto name = a.sections[(size_t) section].name;
        for (auto& s : a.sections)
        {
            if (s.name != name || bar < 0 || bar >= s.bars) continue;
            if (s.chords.size() != s.bars)
            {
                // the bars as they are now, so only this one changes
                juce::StringArray now;
                for (auto& c : sectionChords (plan, s)) now.add (c.name);
                s.chords = now;
            }
            if (chord.isNotEmpty() && chordFromName (chord).has_value()) s.chords.set (bar, chordFromName (chord)->name);
            else
            {
                const auto theirs = styleChords (plan, s);
                if (bar < (int) theirs.size()) s.chords.set (bar, theirs[(size_t) bar].name);
            }
            s.ownChords = true;
        }
    }

    void clearSectionChords (Arrangement& a, int section)
    {
        if (section < 0 || section >= (int) a.sections.size()) return;
        const auto name = a.sections[(size_t) section].name;
        for (auto& s : a.sections)
            if (s.name == name && s.ownChords) { s.chords.clear(); s.ownChords = false; }
    }

    bool hasOwnChords (const Arrangement& a, int section)
    {
        return section >= 0 && section < (int) a.sections.size() && a.sections[(size_t) section].ownChords;
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

    //==============================================================================
    juce::StringArray patternNames (const Arrangement& a, const juce::String& role)
    {
        juce::StringArray names;
        for (auto& s : a.sections) names.addIfNotAlreadyThere (s.patternFor (role));
        return names;
    }

    std::vector<int> sectionsUsing (const Arrangement& a, const juce::String& role, const juce::String& pattern)
    {
        std::vector<int> out;
        for (size_t i = 0; i < a.sections.size(); ++i)
            if (a.sections[i].patternFor (role) == pattern) out.push_back ((int) i);
        return out;
    }

    juce::String makeSectionDifferent (Arrangement& a, int section, const juce::String& role)
    {
        if (section < 0 || section >= (int) a.sections.size()) return {};
        auto& s = a.sections[(size_t) section];
        // "Chorus 3": the third chorus (the next free number if that's taken)
        int nth = 0;
        for (int i = 0; i <= section; ++i) if (a.sections[(size_t) i].name == s.name) ++nth;
        const auto taken = patternNames (a, role);
        auto name = s.name + " " + juce::String (nth);
        for (int k = nth + 1; taken.contains (name); ++k) name = s.name + " " + juce::String (k);
        s.patterns[role] = name;
        return name;
    }

    void setPatternBars (Arrangement& a, const juce::String& role, const juce::String& pattern, int bars)
    {
        for (int i : sectionsUsing (a, role, pattern)) setSectionBars (a, i, bars);
    }

    int mainSection (const Arrangement& a, const juce::String& role)
    {
        int first = -1;
        for (size_t i = 0; i < a.sections.size(); ++i)
        {
            const auto& s = a.sections[i];
            if (s.modeFor (role).isEmpty()) continue;
            const auto kind = sectionKind (s.name);
            if (kind == "chorus" || kind == "drop") return (int) i;
            if (first < 0) first = (int) i;
        }
        return first;
    }

    std::vector<PatternNote> suggestedNotes (const PartsPlan& plan, const ArrangementSection& s, const ArrangementPart& part, const DrumNotes& drums)
    {
        auto sp = plan;
        sp.bars = s.bars;
        sp.chords = sectionChords (plan, s);
        if (part.type == PartType::drums)
        {
            const auto mode = s.modeFor (part.role);
            applyDrumMode (sp.guide.groove, mode.isNotEmpty() ? mode : juce::String ("groove"));
        }
        return notesFromSequence (makePart (sp, part.type, drums, part.instrument));
    }

    std::vector<PatternNote> sectionNotes (const PartsPlan& plan, const Arrangement& a, int section, const ArrangementPart& part,
                                           const DrumNotes& drums, const PatternBook* book)
    {
        if (section < 0 || section >= (int) a.sections.size()) return {};
        const auto& s = a.sections[(size_t) section];
        if (s.modeFor (part.role).isEmpty()) return {};
        if (book != nullptr)
            if (const auto* p = book->find (part.role, s.patternFor (part.role)))
            {
                auto notes = playPattern (*p, s.bars, plan.key);
                return part.type == PartType::drums ? drumsFromLanes (notes, drums) : notes;    // your drums, on this kit's notes
            }
        if (a.shape == SongShape::blank) return {};                  // a blank song: only what you write
        return suggestedNotes (plan, s, part, drums);
    }

    juce::MidiMessageSequence starterPart (const PartsPlan& plan, const Arrangement& a, const PatternBook* book,
                                           const ArrangementPart& part, const DrumNotes& drums)
    {
        if (book != nullptr && ! book->empty())
            if (const int main = mainSection (a, part.role); main >= 0)
            {
                const auto& s = a.sections[(size_t) main];
                if (const auto* p = book->find (part.role, s.patternFor (part.role)))
                {
                    auto notes = playPattern (*p, s.bars, plan.key);
                    return sequenceFromNotes (part.type == PartType::drums ? drumsFromLanes (notes, drums) : notes);
                }
            }
        return makePart (plan, part.type, drums, part.instrument);
    }

    int starterBars (const PartsPlan& plan, const Arrangement& a, const PatternBook* book, const std::vector<ArrangementPart>& parts)
    {
        int bars = juce::jmax (1, plan.bars);
        if (book == nullptr || book->empty()) return bars;
        for (auto& p : parts)
            if (const int main = mainSection (a, p.role); main >= 0)
                if (book->find (p.role, a.sections[(size_t) main].patternFor (p.role)) != nullptr)
                    bars = juce::jmax (bars, a.sections[(size_t) main].bars);
        return bars;
    }

    std::vector<NamedPart> arrangePart (const PartsPlan& plan, const Arrangement& a, const ArrangementPart& part, const DrumNotes& drums,
                                        const PatternBook* book, int section)
    {
        NamedPart np { part.label, {}, part.type == PartType::drums ? drumTag (drums) : juce::String() };
        int bar = 0;
        for (int i = 0; i < (int) a.sections.size(); ++i)
        {
            const auto& s = a.sections[(size_t) i];
            if (section < 0 || section == i)
                np.notes.addSequence (sequenceFromNotes (sectionNotes (plan, a, i, part, drums, book), section < 0 ? bar * 4.0 : 0.0), 0.0);
            bar += s.bars;
        }
        np.notes.sort();
        np.notes.updateMatchedPairs();
        return { np };
    }

    std::vector<NamedPart> arrangeParts (const PartsPlan& plan, const Arrangement& a, const std::vector<ArrangementPart>& parts,
                                         const DrumNotes& drums, const PatternBook* book)
    {
        std::vector<NamedPart> out;
        for (auto& part : parts)
            out.push_back (arrangePart (plan, a, part, drums, book).front());
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
