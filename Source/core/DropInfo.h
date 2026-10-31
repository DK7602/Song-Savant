// Song Savant - what arrived when something was dragged from the DAW onto Song Savant: a MIDI part, a preset,
// audio... Phase 0 records it in the report (it tells us what Cubase can hand to other apps); later phases use it
// to read a song's chords and key from a dropped MIDI part.
#pragma once

#include <juce_core/juce_core.h>

namespace ss
{
    // "Piano.mid: MIDI file, 1 track, 32 notes, 8 bars at 120 BPM"
    juce::String describeDroppedFile (const juce::File&);
}
