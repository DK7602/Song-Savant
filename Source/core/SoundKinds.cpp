#include "SoundKinds.h"

namespace ss
{
    const std::vector<Kind>& allKinds()
    {
        static const std::vector<Kind> k { Kind::piano, Kind::keys, Kind::organ, Kind::guitar, Kind::bass, Kind::strings, Kind::brass,
                                           Kind::woodwinds, Kind::pad, Kind::lead, Kind::synth, Kind::pluck, Kind::sequence, Kind::drums,
                                           Kind::percussion, Kind::vocal, Kind::world, Kind::fx, Kind::other };
        return k;
    }

    juce::String kindName (Kind k)
    {
        switch (k)
        {
            case Kind::piano:      return "Piano";
            case Kind::keys:       return "Keys";
            case Kind::organ:      return "Organ";
            case Kind::guitar:     return "Guitar";
            case Kind::bass:       return "Bass";
            case Kind::strings:    return "Strings";
            case Kind::brass:      return "Brass";
            case Kind::woodwinds:  return "Woodwinds";
            case Kind::pad:        return "Pad";
            case Kind::lead:       return "Lead";
            case Kind::synth:      return "Synth";
            case Kind::pluck:      return "Pluck & Bells";
            case Kind::sequence:   return "Arp & Sequence";
            case Kind::drums:      return "Drums";
            case Kind::percussion: return "Percussion";
            case Kind::vocal:      return "Vocal & Choir";
            case Kind::world:      return "World";
            case Kind::fx:         return "FX";
            case Kind::other:      break;
        }
        return "Other";
    }

    std::optional<Kind> kindFromName (const juce::String& n)
    {
        for (auto k : allKinds())
            if (kindName (k).equalsIgnoreCase (n.trim())) return k;
        return std::nullopt;
    }

    namespace
    {
        bool hasWord (const juce::String& text, const juce::String& word)
        {
            // whole-word match, so "Bass" doesn't match "Bassoon" and "EP" doesn't match "Deep"
            const auto t = " " + text.toLowerCase().replaceCharacters ("_-./()[]|&+,:", "             ") + " ";
            return t.contains (" " + word.toLowerCase() + " ");
        }

        bool anyWord (const juce::String& text, std::initializer_list<const char*> words)
        {
            for (auto* w : words) if (hasWord (text, w)) return true;
            return false;
        }
    }

    Kind guessKind (const juce::String& w)
    {
        if (anyWord (w, { "kick", "snare", "hihat", "hi hat", "hat", "hats", "clap", "drum", "drums", "kit", "808", "909", "tom", "cymbal", "crash", "ride", "beat", "breakbeat", "loop" }))
            return anyWord (w, { "808" }) && anyWord (w, { "bass" }) ? Kind::bass : Kind::drums;
        if (anyWord (w, { "perc", "percussion", "shaker", "conga", "bongo", "tambourine", "cowbell", "timpani" })) return Kind::percussion;
        if (anyWord (w, { "bass", "sub", "basses", "reese" })) return Kind::bass;
        if (anyWord (w, { "pad", "pads", "atmosphere", "atmos", "ambient", "drone", "texture", "swell", "evolving" })) return Kind::pad;
        if (anyWord (w, { "piano", "grand", "upright" })) return Kind::piano;
        if (anyWord (w, { "ep", "rhodes", "wurli", "wurlitzer", "e-piano", "epiano", "clav", "clavinet", "keys", "harpsichord", "celesta" })) return Kind::keys;
        if (anyWord (w, { "organ", "b3", "drawbar", "hammond", "farfisa" })) return Kind::organ;
        if (anyWord (w, { "guitar", "gtr", "strat", "tele", "nylon", "acoustic", "banjo", "mandolin", "steel" })) return Kind::guitar;
        if (anyWord (w, { "strings", "string", "violin", "viola", "cello", "orchestra", "ensemble", "pizzicato", "spiccato" })) return Kind::strings;
        if (anyWord (w, { "brass", "trumpet", "horn", "horns", "trombone", "tuba" })) return Kind::brass;
        if (anyWord (w, { "flute", "sax", "saxophone", "clarinet", "oboe", "bassoon", "woodwind", "woodwinds", "reed" })) return Kind::woodwinds;
        if (anyWord (w, { "vox", "vocal", "vocals", "voice", "voices", "choir", "aah", "ooh", "chant" })) return Kind::vocal;
        if (anyWord (w, { "pluck", "plucks", "bell", "bells", "mallet", "mallets", "marimba", "vibraphone", "glockenspiel", "kalimba", "music box", "harp" })) return Kind::pluck;
        if (anyWord (w, { "arp", "arpeggio", "seq", "sequence", "sequencer" })) return Kind::sequence;
        if (anyWord (w, { "lead", "leads", "solo" })) return Kind::lead;
        if (anyWord (w, { "fx", "sfx", "riser", "impact", "hit", "noise", "sweep", "whoosh", "stab" })) return Kind::fx;
        if (anyWord (w, { "synth", "chord", "chords", "stab", "poly", "saw", "square" })) return Kind::synth;
        return Kind::other;
    }

    Kind kindFromMediaBay (const juce::String& category, const juce::String& instrument)
    {
        const auto c = category.trim();
        if (c.isEmpty()) return guessKind (instrument);
        if (c == "Piano") return Kind::piano;
        if (c == "Keyboard" || c == "Accordion") return Kind::keys;
        if (c == "Organ") return Kind::organ;
        if (c.startsWith ("Guitar")) return Kind::guitar;
        if (c == "Bass") return Kind::bass;
        if (c == "Strings") return Kind::strings;
        if (c == "Brass") return Kind::brass;
        if (c == "Woodwinds") return Kind::woodwinds;
        if (c == "Synth Pad") return Kind::pad;
        if (c == "Synth Lead") return instrument.containsIgnoreCase ("Arpeggio") ? Kind::sequence : Kind::lead;
        if (c == "Synth Comp") return Kind::synth;
        if (c == "Chromatic Perc") return Kind::pluck;
        if (c == "Drum&Perc") return instrument.containsIgnoreCase ("Percussion") ? Kind::percussion : Kind::drums;
        if (c == "Percussion") return Kind::percussion;
        if (c == "Vocal") return Kind::vocal;
        if (c == "Ethnic") return Kind::world;
        if (c == "Musical FX" || c == "Sound FX" || c == "Cinematic") return Kind::fx;
        return guessKind (c + " " + instrument);
    }

    Kind kindFromArturia (const juce::String& type, const juce::String& subtype)
    {
        const auto t = type.trim();
        if (t == "Pad") return Kind::pad;
        if (t == "Sequence") return Kind::sequence;
        if (t == "Keys") return subtype.containsIgnoreCase ("Synth") ? Kind::synth : Kind::keys;
        if (t == "Lead") return Kind::lead;
        if (t == "Bass") return Kind::bass;
        if (t == "Sound Effects") return Kind::fx;
        if (t == "Organ") return Kind::organ;
        if (t == "Strings") return Kind::strings;
        if (t == "Drums") return Kind::drums;
        if (t == "Brass & Winds")
            return (subtype.containsIgnoreCase ("Wood") || subtype.containsIgnoreCase ("Flute") || subtype.containsIgnoreCase ("Reed")
                    || subtype.containsIgnoreCase ("Sax")) ? Kind::woodwinds : Kind::brass;
        if (t == "Electric Piano") return Kind::keys;
        if (t == "Piano") return Kind::piano;
        if (t == "Vocal") return Kind::vocal;
        return guessKind (t + " " + subtype);
    }
}
