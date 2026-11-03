#include "DrumLanes.h"

namespace ss
{
    const std::vector<DrumLane>& drumLanes()
    {
        static const std::vector<DrumLane> lanes {
            { "crash", "Crash", 49 },    { "ride", "Ride", 51 },          { "openhat", "Open Hat", 46 }, { "hat", "Closed Hat", 42 },
            { "pedalhat", "Pedal Hat", 44 }, { "tomhigh", "High Tom", 50 }, { "tommid", "Mid Tom", 47 }, { "tomlow", "Low Tom", 45 },
            { "clap", "Clap", 39 },      { "snare", "Snare", 38 },        { "rim", "Rim", 37 },          { "kick", "Kick", 36 },
            { "808", "808", 35 },        { "shaker", "Shaker", 70 },      { "perc", "Perc", 56 } };
        return lanes;
    }

    int laneNote (const juce::String& id)
    {
        for (auto& l : drumLanes()) if (id == l.id) return l.note;
        return -1;
    }

    juce::String laneName (int note)
    {
        for (auto& l : drumLanes()) if (l.note == note) return l.name;
        return {};
    }

    juce::String laneForNote (int note, const juce::String& source)
    {
        const bool gm = source.containsIgnoreCase ("General MIDI");
        switch (note)
        {
            case 35: case 36:          return "kick";
            case 37:                   return "rim";
            case 38: case 40:          return "snare";
            case 39:                   return "clap";
            case 42:                   return "hat";
            case 44:                   return "pedalhat";
            case 46:                   return "openhat";
            case 41: case 43: case 45: return "tomlow";
            case 47: case 48:          return "tommid";
            case 50:                   return "tomhigh";
            case 49:                   return "crash";
            case 51: case 59:          return "ride";
            // above 51 the layouts differ: Groove Grabber's pads put the 808 and the percussion here
            case 52:                   return gm ? "crash" : "808";
            case 53:                   return gm ? "ride" : "perc";
            case 54:                   return "shaker";        // GM: tambourine; Groove Grabber: perc 2
            case 55:                   return gm ? "crash" : "perc";
            case 56:                   return "perc";          // GM: cowbell; Groove Grabber: FX 1
            case 57:                   return gm ? "crash" : "perc";
            case 69: case 70: case 82: return "shaker";
            default: break;
        }
        if (note >= 60 && note <= 81) return "perc";           // GM's hand percussion
        return {};
    }

    std::vector<PatternNote> drumsToLanes (const std::vector<PatternNote>& notes, const DrumNotes& kit)
    {
        std::vector<PatternNote> out;
        for (auto n : notes)
        {
            juce::String lane;
            for (auto& [id, note] : kit.notes) if (note == n.note && laneNote (id) >= 0) { lane = id; break; }   // the kit's own name first
            if (lane.isEmpty()) lane = laneForNote (n.note, kit.source);
            if (const int to = laneNote (lane); to >= 0)
            {
                n.note = to;
                out.push_back (n);
            }
        }
        sortNotes (out);
        return out;
    }

    std::vector<PatternNote> drumsFromLanes (const std::vector<PatternNote>& notes, const DrumNotes& kit)
    {
        // a drum the kit doesn't have plays on the nearest one it does
        static const std::map<juce::String, juce::StringArray> instead {
            { "808", { "kick" } }, { "shaker", { "perc", "hat" } }, { "perc", { "rim", "shaker" } }, { "pedalhat", { "hat" } },
            { "openhat", { "hat" } }, { "rim", { "snare" } }, { "clap", { "snare" } }, { "ride", { "hat", "crash" } },
            { "crash", { "ride" } }, { "tomhigh", { "tommid", "tomlow" } }, { "tommid", { "tomlow", "tomhigh" } }, { "tomlow", { "tommid" } } };
        std::vector<PatternNote> out;
        for (auto n : notes)
        {
            juce::String lane;
            for (auto& l : drumLanes()) if (l.note == n.note) { lane = l.id; break; }
            if (lane.isEmpty()) continue;
            int to = kit.note (lane);
            if (to < 0)
                if (auto it = instead.find (lane); it != instead.end())
                    for (auto& other : it->second)
                        if ((to = kit.note (other)) >= 0) break;
            if (to < 0) continue;
            n.note = to;
            out.push_back (n);
        }
        sortNotes (out);
        return out;
    }

    bool isMidiFile (const juce::File& f)
    {
        return f.hasFileExtension ("mid;midi;smf");
    }

    std::optional<DroppedMidi> readMidiFile (const juce::File& file)
    {
        juce::FileInputStream in (file);
        juce::MidiFile mf;
        if (! in.openedOk() || ! mf.readFrom (in)) return {};
        const int tpq = mf.getTimeFormat();
        if (tpq <= 0) return {};                               // SMPTE time: not a pattern
        DroppedMidi d;
        d.name = file.getFileNameWithoutExtension();
        double end = 0.0;
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            auto seq = *mf.getTrack (t);
            seq.updateMatchedPairs();
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                const auto& m = seq.getEventPointer (i)->message;
                if (m.isTextMetaEvent())
                {
                    const auto text = m.getTextFromTextMetaEvent();
                    if (text.startsWithIgnoreCase ("drums:")) d.drumSource = text.fromFirstOccurrenceOf (":", false, false).trim();
                }
                if (! m.isNoteOn()) continue;
                const auto* off = seq.getEventPointer (i)->noteOffObject;
                PatternNote n;
                n.start = m.getTimeStamp() / tpq;
                n.length = juce::jmax (0.02, ((off != nullptr ? off->message.getTimeStamp() : m.getTimeStamp() + tpq / 4.0) - m.getTimeStamp()) / tpq);
                n.note = m.getNoteNumber();
                n.velocity = juce::jmax (1, (int) m.getVelocity());
                d.notes.push_back (n);
                end = juce::jmax (end, n.start + n.length);
            }
        }
        if (d.notes.empty()) return {};
        // from its first bar: a part that starts at bar 5 of a song comes in at bar 1
        double first = 1.0e9;
        for (auto& n : d.notes) first = juce::jmin (first, n.start);
        const double shift = std::floor (first / 4.0 + 1.0e-6) * 4.0;
        for (auto& n : d.notes) n.start -= shift;
        sortNotes (d.notes);
        d.bars = juce::jlimit (1, 64, (int) std::ceil ((end - shift) / 4.0 - 1.0e-6));
        return d;
    }

    juce::String drumTag (const DrumNotes& kit)
    {
        return "drums: " + kit.source;
    }
}
