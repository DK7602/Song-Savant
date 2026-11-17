// Song Savant - the plug-in / app: the setup scan, the library, the song, and the sketch player that plays the
// starter parts (or the whole song) with simple built-in sounds, in time with the DAW when it runs inside one.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "core/ScanJob.h"
#include "core/CatalogJob.h"
#include "core/SongSession.h"
#include "core/KitMapJob.h"
#include "core/Report.h"
#include "core/SketchPlayer.h"
#include "core/PatternEdit.h"

class SongSavantProcessor : public juce::AudioProcessor
{
public:
    SongSavantProcessor();
    ~SongSavantProcessor() override = default;

    // the setup check
    ss::ScanJob scan;
    juce::Array<juce::File> extraFolders;
    std::function<ss::SystemLocations()> locationProvider;   // the real computer, or a fake one in the tests

    void startScan();

    // the catalog of every sound you own (built in the background when the window first opens)
    ss::CatalogJob catalog;
    void startCatalog();
    int page = 0;                                          // the page that was showing (0 Song, 1 Library, 2 Setup Check, 3 Drum Maps)

    // mapping a drum kit (the Drum Maps page)
    ss::KitMapJob kitMap;
    juce::String dawName() const;     // the DAW Song Savant runs in, or (as an app) the one on this computer; empty if unknown

    // the song you're working on and its palettes
    ss::SongSession song;
    juce::File saveReport();                 // writes Documents/Song Savant/Setup Report.txt (if there's a result)
    juce::String reportText() const;

    // hearing the parts: PLAY ALL and each card's play button on the Song page (the starter parts, looped), the
    // transport on the Arrange page (the whole song), and the piano roll (the section being edited, looped)
    ss::SketchPlayer sketch;
    enum class SketchMode { loop, song, section };
    void playSketch (SketchMode);                       // a loop from its start; the song from the playhead
    void stopSketch()                                   { sketch.stop(); }
    bool isSketchPlaying (SketchMode m) const           { return sketch.isPlaying() && sketchMode == m; }
    void updateSketch (SketchMode, std::shared_ptr<const ss::SketchSong>);   // the latest parts for that button
    // The Song page: every part (an empty role) or one card's part on its own. It changes in time while it plays.
    // False (and no change) when that card has no starter part.
    bool setSketchSolo (const juce::String& role);
    const juce::String& getSketchSolo() const           { return soloRole; }
    bool sketchHasPart (const juce::String& role) const;   // the starter parts have one for this card
    void setSectionSolo (const juce::String& role)      { sectionSolo = role; applySolo(); }   // the piano roll: ALONE (or "")
    ss::NoteClip noteClip;                              // what the piano roll copied (Ctrl+C), for any part or section

    // The Arrange page's transport, for the whole song. PLAY plays from the playhead (from the top when it's at the
    // end); STOP stops and the playhead goes back to where it started; PAUSE stops where it is. Moving the playhead
    // while it plays carries on from there (and STOP then goes back to there).
    void songPlay();
    void songStop()                                     { if (isSketchPlaying (SketchMode::song)) sketch.stop(); }
    void songPause();
    void songMoveTo (double beats);
    void songStep (int direction);                      // rewind (-1) / fast forward (+1): to the bar line before / after
    double songPosition() const;                        // the playhead, in beats from the start (playing or not)
    double songLength() const                           { return songSketch != nullptr ? songSketch->lengthBeats : 0.0; }
    bool songFollowsDaw() const                         { return isSketchPlaying (SketchMode::song) && sketch.isFollowingHost(); }
    double getHostBpm() const                           { return hostBpm.load(); }   // the DAW's tempo; 0 in the app
    bool isInDaw() const                                { return wrapperType != wrapperType_Standalone && wrapperType != wrapperType_Undefined; }

    // ---- a MIDI keyboard (the app: Options > Audio/MIDI Settings; in a DAW, the track's input). Its notes play the
    // highlighted card's sketch sound (or the piano roll's part), and reach the piano roll for RECORD and STEP INPUT.
    // The drums: a key plays the drum the kit has there (its drum map, Groove Grabber's pads, or General MIDI).
    void setKeyboardPart (ss::PartType, ss::Kind, const ss::DrumNotes* kit = nullptr);   // what the keys play now
    juce::String keyboardFor;                           // (the message thread) who set it last: "song:Bass:...", "roll:Keys:..." 
    int keyboardDrum (int note) const                   { return note >= 0 && note < 128 ? drumLane[(size_t) note].load() : -1; }  // its drum row (-1: none)
    struct KeyEvent
    {
        int note = 60, velocity = 0;                    // velocity 0: let go
        double beat = -1.0;                             // where the sketch was when it came (below 0: a count-in; -1000: not playing)
    };
    bool nextKeyEvent (KeyEvent&);                      // the message thread: the next note played (false: none waiting)
    int keyboardActivity() const                        { return keyActivity.load(); }   // goes up with every note played
    void keyboardEvent (const juce::MidiMessage&, int sampleOffset);   // (the audio thread; also the tests)
    void playSection (double countIn);                  // RECORD: the piano roll's section from its start, after a count-in
    void setMetronome (bool on);                        // METRONOME (kept for next time)
    bool getMetronome() const                           { return sketch.metronome.load(); }

    //==============================================================================
    void prepareToPlay (double sampleRate, int) override   { sketch.prepare (sampleRate); }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }

    const juce::String getName() const override     { return "Song Savant"; }
    bool acceptsMidi() const override               { return true; }
    bool producesMidi() const override              { return true; }
    bool isMidiEffect() const override              { return false; }
    double getTailLengthSeconds() const override    { return 0.0; }

    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override           {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    void applySolo();
    SketchMode sketchMode = SketchMode::loop;
    std::shared_ptr<const ss::SketchSong> loopSketch, songSketch, sectionSketch;
    std::shared_ptr<const ss::SketchSong>& sketchFor (SketchMode m)   { return m == SketchMode::loop ? loopSketch : m == SketchMode::song ? songSketch : sectionSketch; }
    juce::String soloRole;                               // the Song page's one part on its own ("" every part)
    juce::String sectionSolo;                            // the piano roll's part, when it plays ALONE
    double songCursor = 0.0;                             // where the song's playhead waits (beats)
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<int> keyType { (int) ss::PartType::chords }, keyKind { (int) ss::Kind::piano }, keyActivity { 0 };
    std::array<std::atomic<int>, 128> drumLane, drumSound;   // a key -> its drum row (lane note) and the sketch's drum note
    std::array<KeyEvent, 512> keyEvents;
    juce::AbstractFifo keyFifo { 512 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongSavantProcessor)
};
