// Song Savant - the drums in the piano roll, and drum patterns passed between Song Savant, Groove Grabber and the DAW.
//
// A drum pattern you edit is kept by drum (kick, snare, closed hat...), not by the notes of one kit: Song Savant keeps
// it on General MIDI notes (the 808 on 35), and turns it into the drum card's own notes (Groove Grabber's pads, a
// Cubase drum map...) when it plays or writes MIDI. So changing the drum kit keeps your pattern on the right drums.
// A MIDI file dropped in can come from Groove Grabber (its pads), the DAW (General MIDI) or Song Savant: the drum map
// it says it was made with is used when there's one ("drums: General MIDI"), and the common layouts otherwise.
#pragma once

#include "StarterParts.h"
#include "Patterns.h"

namespace ss
{
    struct DrumLane
    {
        const char* id;                           // "openhat": the lanes the starter parts use
        const char* name;                         // "Open Hat"
        int note;                                 // how Song Savant keeps it
    };
    const std::vector<DrumLane>& drumLanes();     // top to bottom, the way the piano roll shows them
    int laneNote (const juce::String& id);        // -1 for none
    juce::String laneName (int note);             // "Closed Hat" for 42 ("" when it isn't a lane)

    // The drum a note means in a file made with a drum map (its `source`, as in "drums: General MIDI"): exact for
    // General MIDI and Groove Grabber's pads, and the usual layouts when the file doesn't say ("" when no drum fits)
    juce::String laneForNote (int note, const juce::String& source = {});

    std::vector<PatternNote> drumsToLanes (const std::vector<PatternNote>&, const DrumNotes& kit);    // a kit's notes -> lanes
    std::vector<PatternNote> drumsFromLanes (const std::vector<PatternNote>&, const DrumNotes& kit);  // lanes -> a kit's notes

    // A dropped MIDI file: its notes from the first beat (every track), and the drum map it names, if any
    struct DroppedMidi
    {
        std::vector<PatternNote> notes;
        juce::String drumSource;                  // "General MIDI", "Groove Grabber's pads"... ("" when it doesn't say)
        juce::String name;                        // its track or file name
        int bars = 0;                             // how many bars it covers
    };
    std::optional<DroppedMidi> readMidiFile (const juce::File&);
    bool isMidiFile (const juce::File&);

    // The text a drum track carries in Song Savant's MIDI files (and Groove Grabber's): "drums: <the drum map>"
    juce::String drumTag (const DrumNotes&);
}
