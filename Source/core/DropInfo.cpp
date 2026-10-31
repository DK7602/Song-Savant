#include "DropInfo.h"
#include "VstPreset.h"
#include "FileWalk.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace ss
{
    namespace
    {
        juce::String describeMidi (const juce::File& f)
        {
            juce::FileInputStream in (f);
            juce::MidiFile mf;
            if (! in.openedOk() || ! mf.readFrom (in)) return "MIDI file (couldn't be read)";

            int notes = 0, tracksWithNotes = 0;
            double bpm = 0, lastTick = 0;
            for (int t = 0; t < mf.getNumTracks(); ++t)
            {
                int n = 0;
                for (auto* e : *mf.getTrack (t))
                {
                    if (e->message.isNoteOn()) ++n;
                    if (e->message.isTempoMetaEvent() && bpm <= 0) bpm = 60.0 / e->message.getTempoSecondsPerQuarterNote();
                    lastTick = juce::jmax (lastTick, e->message.getTimeStamp());
                }
                notes += n;
                if (n > 0) ++tracksWithNotes;
            }
            juce::String s = "MIDI file, " + juce::String (tracksWithNotes) + (tracksWithNotes == 1 ? " track" : " tracks")
                             + " with notes, " + juce::String (notes) + (notes == 1 ? " note" : " notes");
            if (const auto tpq = mf.getTimeFormat(); tpq > 0 && lastTick > 0)
                s << ", about " << juce::String (lastTick / tpq / 4.0, 1) << " bars";
            if (bpm > 0) s << " at " << juce::String (juce::roundToInt (bpm)) << " BPM";
            return s;
        }

        juce::String describePreset (const juce::File& f)
        {
            const auto info = readVstPreset (f);
            if (! info.valid) return "preset file (couldn't be read: " + info.error + ")";
            juce::String s = "VST3 preset";
            if (auto p = info.attributes["PlugInName"]; p.isNotEmpty()) s << " for " << p;
            juce::StringArray tags;
            for (auto& k : soundTagKeys())
                if (info.attributes[k].isNotEmpty()) tags.add (info.attributes[k]);
            s << (tags.isEmpty() ? juce::String (", no sound tags") : ", tags: " + tags.joinIntoString (" / "));
            return s;
        }
    }

    juce::String describeDroppedFile (const juce::File& f)
    {
        const auto name = f.getFileName();
        if (f.isDirectory()) return name + ": a folder";
        if (! f.existsAsFile()) return name + ": (the file wasn't there)";

        const auto ext = f.getFileExtension().toLowerCase();
        juce::String what;
        if (ext == ".mid" || ext == ".midi")             what = describeMidi (f);
        else if (ext == ".vstpreset")                    what = describePreset (f);
        else if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".mp3" || ext == ".flac" || ext == ".ogg")
                                                         what = "audio file";
        else if (ext == ".trackpreset")                  what = "Cubase track preset";
        else if (ext == ".xml")                          what = "XML file";
        else                                             what = (ext.isEmpty() ? juce::String ("file with no extension") : ext + " file");
        return name + ": " + what + " (" + formatBytes (f.getSize()) + ")";
    }
}
