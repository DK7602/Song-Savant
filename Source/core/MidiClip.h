// Song Savant - MIDI clips. Phase 0: a 4-bar chord clip for the drag-and-drop test. Phase 2 grows this into the
// starter parts (chords, bass, rhythm, hook).
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ss
{
    // C - G - Am - F, one bar each, at the given tempo. Returns true if the file was written.
    bool writeTestChordClip (const juce::File& dest, double bpm = 120.0);
}
