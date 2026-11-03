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

    // hearing the parts: PLAY ALL and each card's play button on the Song page (the starter parts, looped), and the
    // transport on the Arrange page (the whole song)
    ss::SketchPlayer sketch;
    enum class SketchMode { loop, song };
    void playSketch (SketchMode);                       // the loop from its start; the song from the playhead
    void stopSketch()                                   { sketch.stop(); }
    bool isSketchPlaying (SketchMode m) const           { return sketch.isPlaying() && sketchMode == m; }
    void updateSketch (SketchMode, std::shared_ptr<const ss::SketchSong>);   // the latest parts for that button
    // The Song page: every part (an empty role) or one card's part on its own. It changes in time while it plays.
    // False (and no change) when that card has no starter part.
    bool setSketchSolo (const juce::String& role);
    const juce::String& getSketchSolo() const           { return soloRole; }
    bool sketchHasPart (const juce::String& role) const;   // the starter parts have one for this card

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
    std::shared_ptr<const ss::SketchSong> loopSketch, songSketch;
    juce::String soloRole;                               // the Song page's one part on its own ("" every part)
    double songCursor = 0.0;                             // where the song's playhead waits (beats)
    std::atomic<double> hostBpm { 0.0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongSavantProcessor)
};
