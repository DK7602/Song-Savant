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
    ss::DragTests dragTests;
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

    // hearing the parts: PLAY on the Song page (the starter parts, looped) and on the Arrange page (the whole song)
    ss::SketchPlayer sketch;
    enum class SketchMode { loop, song };
    void playSketch (SketchMode);
    void stopSketch()                                   { sketch.stop(); }
    bool isSketchPlaying (SketchMode m) const           { return sketch.isPlaying() && sketchMode == m; }
    void updateSketch (SketchMode, std::shared_ptr<const ss::SketchSong>);   // the latest parts for that button
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
    SketchMode sketchMode = SketchMode::loop;
    std::shared_ptr<const ss::SketchSong> loopSketch, songSketch;
    std::atomic<double> hostBpm { 0.0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongSavantProcessor)
};
