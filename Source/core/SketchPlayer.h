// Song Savant - hearing the parts before you drag them: the sketch player. It plays the starter parts (an 8-bar loop)
// or the whole arranged song with simple built-in sounds (synthesised drums, a bass, keys or guitar, a pad, a lead),
// so you hear the idea straight away. Inside a DAW it follows the DAW: its tempo, and its play button and position.
#pragma once

#include "Arrangement.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <deque>
#include <memory>

namespace ss
{
    struct SketchNote
    {
        double start = 0.0, length = 0.0;    // in beats from the start
        int note = 60, velocity = 100;
        int part = 0;                         // index into SketchSong::parts
    };

    struct SketchPart
    {
        juce::String name;                    // "Bass - Warm Analog Bass"
        PartType type = PartType::none;
        Kind instrument = Kind::other;
        juce::String role;                    // "Bass": the card it comes from
    };

    struct SketchSong
    {
        std::vector<SketchPart> parts;
        std::vector<SketchNote> notes;        // in order of start
        double lengthBeats = 32.0;
        double bpm = 120.0;
        bool loop = true;                     // the starter parts loop; the whole song plays once
    };

    // The starter parts, one 8-bar loop (what ALL PARTS drags), or the whole song (what WHOLE SONG drags). The drums are
    // played on General MIDI notes, which is what the sketch's drum sounds follow.
    SketchSong sketchOfParts (const PartsPlan&, const std::vector<ArrangementPart>&);
    SketchSong sketchOfSong (const PartsPlan&, const Arrangement&, const std::vector<ArrangementPart>&);

    class SketchPlayer
    {
    public:
        SketchPlayer();
        ~SketchPlayer();
        void prepare (double sampleRate);

        // ---- the message thread
        void setSong (std::shared_ptr<const SketchSong>);    // takes over at the next block; what's sounding carries on
        // From `fromBeat` (the start, or where the playhead is); inside a DAW that's playing, along with the DAW. Playing
        // already: it jumps there (notes held across that point sound from there on).
        void play (double fromBeat = 0.0);
        void stop();
        // One part on its own (an index into SketchSong::parts), or every part (-1). Changes as it plays, in time.
        void setSolo (int part)               { solo = part; }
        int getSolo() const                   { return solo.load(); }
        bool isPlaying() const                { return armed.load(); }
        bool isFollowingHost() const          { return followingHost.load(); }   // the DAW is playing: it decides where
        double getPosition() const            { return position.load(); }   // beats into the song (-1: silent)
        std::atomic<float> volume { 0.8f };

        // ---- the audio thread
        struct Host { bool known = false, playing = false; double ppq = 0.0, bpm = 0.0; };
        void process (juce::AudioBuffer<float>&, const Host&);

    private:
        struct Voice
        {
            bool active = false, drum = false, releasing = false;
            int note = 60, part = 0, gate = 0;
            PartType type = PartType::none;
            Kind instrument = Kind::other;
            float vel = 1.0f, env = 0.0f, lp = 0.0f, hpIn = 0.0f, hpOut = 0.0f;
            double phase = 0.0, phase2 = 0.0, freq = 440.0, t = 0.0;
            juce::uint32 seed = 1;
        };
        void startVoice (const SketchNote&, const SketchSong&, double lengthSamples, int offset);
        void render (juce::AudioBuffer<float>&, int from, int to);
        float toneSample (Voice&);
        float drumSample (Voice&);
        void release (Voice&);
        void releaseAll();
        void schedule (const SketchSong&, double from, double to, double samplesPerBeat, int blockLength);
        void chase (const SketchSong&, double at, double samplesPerBeat, int except);   // the notes already held at `at`
        bool heard (int part) const           { const int s = solo.load(); return s < 0 || s == part; }

        double sampleRate = 44100.0;
        std::array<Voice, 64> voices;
        std::atomic<bool> armed { false }, restart { false }, followingHost { false };
        std::atomic<double> position { -1.0 }, startAt { 0.0 };
        std::atomic<int> solo { -1 };
        int lastSolo = -1;
        bool ownRunning = false, wasFollowing = false, wasSounding = false;
        double ownPos = 0.0, expectedPos = -1.0;
        juce::SpinLock songLock;
        const SketchSong* song = nullptr;                    // the song to play (set on the message thread)
        const SketchSong* cachedSong = nullptr;              // what the audio thread plays
        std::deque<std::shared_ptr<const SketchSong>> keep;  // the current song and the last few: the audio thread may
                                                             // still be reading one it picked up a moment ago
    };
}
