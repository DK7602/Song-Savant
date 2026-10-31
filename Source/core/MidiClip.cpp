#include "MidiClip.h"

namespace ss
{
    bool writeTestChordClip (const juce::File& dest, double bpm)
    {
        constexpr int ppq = 480, bar = ppq * 4;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Song Savant test chords"), 0);
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0);
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);

        const int chords[4][4] = { { 48, 60, 64, 67 },     // C
                                   { 43, 59, 62, 67 },     // G
                                   { 45, 60, 64, 69 },     // Am
                                   { 41, 60, 65, 69 } };   // F
        for (int b = 0; b < 4; ++b)
            for (int n : chords[b])
            {
                seq.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 96), b * bar);
                seq.addEvent (juce::MidiMessage::noteOff (1, n), b * bar + bar - 10);
            }
        seq.addEvent (juce::MidiMessage::endOfTrack(), 4 * bar);
        seq.updateMatchedPairs();

        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ppq);
        mf.addTrack (seq);

        dest.getParentDirectory().createDirectory();
        juce::FileOutputStream out (dest);
        if (! out.openedOk()) return false;
        out.setPosition (0);
        out.truncate();
        return mf.writeTo (out, 1);
    }
}
