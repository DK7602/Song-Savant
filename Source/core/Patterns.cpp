#include "Patterns.h"
#include <algorithm>
#include <cmath>

namespace ss
{
    namespace
    {
        constexpr double ticksPerBeat = 480.0;
    }

    void sortNotes (std::vector<PatternNote>& notes)
    {
        std::stable_sort (notes.begin(), notes.end(), [] (const PatternNote& a, const PatternNote& b)
        {
            return std::abs (a.start - b.start) > 1.0e-9 ? a.start < b.start : a.note < b.note;
        });
    }

    //==============================================================================
    const Pattern* PatternBook::find (const juce::String& role, const juce::String& name) const
    {
        auto it = patterns.find (keyFor (role, name));
        return it != patterns.end() ? &it->second : nullptr;
    }

    void PatternBook::set (const juce::String& role, const juce::String& name, Pattern p)
    {
        sortNotes (p.notes);
        patterns[keyFor (role, name)] = std::move (p);
        ++changes;
    }

    void PatternBook::remove (const juce::String& role, const juce::String& name)
    {
        if (patterns.erase (keyFor (role, name)) > 0) ++changes;
    }

    bool PatternBook::hasAny (const juce::String& role) const
    {
        const auto prefix = role + "|";
        for (auto& [k, p] : patterns)
            if (k.startsWith (prefix)) return true;
        return false;
    }

    std::unique_ptr<juce::XmlElement> PatternBook::toXml() const
    {
        auto x = std::make_unique<juce::XmlElement> ("Patterns");
        for (auto& [k, p] : patterns)
        {
            auto* px = x->createNewChildElement ("Pattern");
            px->setAttribute ("role", k.upToFirstOccurrenceOf ("|", false, false));
            px->setAttribute ("name", k.fromFirstOccurrenceOf ("|", false, false));
            px->setAttribute ("bars", p.bars);
            px->setAttribute ("key", p.key);
            if (p.drums) px->setAttribute ("drums", true);
            // the notes, compact: "start length note velocity;" (beats to 1/1000)
            juce::String notes;
            for (auto& n : p.notes)
                notes << juce::String (n.start, 4) << " " << juce::String (n.length, 4) << " " << n.note << " " << n.velocity << ";";
            px->setAttribute ("notes", notes);
        }
        return x;
    }

    PatternBook PatternBook::fromXml (const juce::XmlElement& x)
    {
        PatternBook b;
        for (auto* px : x.getChildWithTagNameIterator ("Pattern"))
        {
            Pattern p;
            p.bars = juce::jlimit (1, 64, px->getIntAttribute ("bars", 8));
            p.key = juce::jlimit (0, 23, px->getIntAttribute ("key", 0));
            p.drums = px->getBoolAttribute ("drums", false);
            for (auto& item : juce::StringArray::fromTokens (px->getStringAttribute ("notes"), ";", ""))
            {
                const auto f = juce::StringArray::fromTokens (item, " ", "");
                if (f.size() < 4) continue;
                PatternNote n;
                n.start = juce::jmax (0.0, f[0].getDoubleValue());
                n.length = juce::jlimit (0.01, 256.0, f[1].getDoubleValue());
                n.note = juce::jlimit (0, 127, f[2].getIntValue());
                n.velocity = juce::jlimit (1, 127, f[3].getIntValue());
                p.notes.push_back (n);
            }
            const auto role = px->getStringAttribute ("role"), name = px->getStringAttribute ("name");
            if (role.isNotEmpty() && name.isNotEmpty()) b.set (role, name, std::move (p));
        }
        return b;
    }

    //==============================================================================
    std::vector<PatternNote> notesFromSequence (const juce::MidiMessageSequence& in)
    {
        auto seq = in;
        seq.updateMatchedPairs();
        std::vector<PatternNote> out;
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            const auto* e = seq.getEventPointer (i);
            if (! e->message.isNoteOn()) continue;
            const double on = e->message.getTimeStamp();
            const double off = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : on + ticksPerBeat / 4.0;
            PatternNote n;
            n.start = on / ticksPerBeat;
            n.length = juce::jmax (0.02, (off - on) / ticksPerBeat);
            n.note = e->message.getNoteNumber();
            n.velocity = juce::jmax (1, (int) e->message.getVelocity());
            out.push_back (n);
        }
        sortNotes (out);
        return out;
    }

    juce::MidiMessageSequence sequenceFromNotes (const std::vector<PatternNote>& notes, double offsetBeats, int channel)
    {
        juce::MidiMessageSequence seq;
        for (auto& n : notes)
        {
            const double on = (offsetBeats + n.start) * ticksPerBeat;
            const double off = on + juce::jmax (0.02, n.length) * ticksPerBeat;
            seq.addEvent (juce::MidiMessage::noteOn (channel, n.note, (juce::uint8) juce::jlimit (1, 127, n.velocity)), on);
            seq.addEvent (juce::MidiMessage::noteOff (channel, n.note), off);
        }
        seq.sort();
        seq.updateMatchedPairs();
        return seq;
    }

    //==============================================================================
    int keyShift (int fromKey, int toKey)
    {
        int d = ((toKey % 12) - (fromKey % 12) + 12) % 12;
        if (d > 5) d -= 12;
        return d;
    }

    std::vector<PatternNote> moveToKey (std::vector<PatternNote> notes, int fromKey, int toKey)
    {
        if (fromKey == toKey) return notes;
        const int shift = keyShift (fromKey, toKey);
        const bool fromMinor = fromKey >= 12, toMinor = toKey >= 12;
        const int tonic = toKey % 12;
        for (auto& n : notes)
        {
            n.note += shift;
            if (fromMinor != toMinor)
            {
                // the scale's 3rd, 6th and 7th: up a semitone into major, down into minor
                const int degree = ((n.note - tonic) % 12 + 12) % 12;
                if (toMinor && (degree == 4 || degree == 9 || degree == 11)) n.note -= 1;
                if (! toMinor && (degree == 3 || degree == 8 || degree == 10)) n.note += 1;
            }
            n.note = juce::jlimit (0, 127, n.note);
        }
        return notes;
    }

    std::vector<PatternNote> playPattern (const Pattern& p, int bars, int key)
    {
        const auto notes = p.drums ? p.notes : moveToKey (p.notes, p.key, key);
        const double len = juce::jmax (1, p.bars) * 4.0, total = juce::jmax (1, bars) * 4.0;
        std::vector<PatternNote> out;
        for (double offset = 0.0; offset < total - 1.0e-9; offset += len)
            for (auto& n : notes)
            {
                if (n.start >= len - 1.0e-9) continue;          // past the pattern's end (kept, but not played)
                auto m = n;
                m.start += offset;
                if (m.start >= total - 1.0e-9) continue;
                m.length = juce::jmin (m.length, total - m.start);
                out.push_back (m);
            }
        sortNotes (out);
        return out;
    }

    Pattern unrolled (Pattern p, int bars)
    {
        if (bars <= p.bars) return p;
        const double len = p.bars * 4.0;
        std::vector<PatternNote> notes;
        for (auto& n : p.notes) if (n.start < len - 1.0e-9) notes.push_back (n);
        auto out = notes;
        for (double offset = len; offset < bars * 4.0 - 1.0e-9; offset += len)
            for (auto n : notes)
            {
                n.start += offset;
                if (n.start < bars * 4.0 - 1.0e-9) out.push_back (n);
            }
        p.notes = out;
        p.bars = bars;
        sortNotes (p.notes);
        return p;
    }
}
