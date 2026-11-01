// Song Savant - mapping a drum kit in the background: work out which plug-in a saved preset belongs to, then run
// Song Savant Scanner on it (it plays every key and listens, in its own process) and pick up the kit map it sends back.
#pragma once

#include "KitMap.h"
#include "PluginScanner.h"
#include <atomic>
#include <memory>

namespace ss
{
    struct PresetPlugin
    {
        juce::String name;       // "Analog Lab V"
        juce::File file;         // the plug-in on disk
        juce::String problem;    // why it couldn't be found
    };

    // Which installed plug-in a .vstpreset is for: the plug-in whose own info lists the preset's class ID, or the
    // plug-in named in the preset (or the folder it's saved in: VST3 Presets/<maker>/<plug-in>/)
    PresetPlugin pluginForPreset (const juce::File& preset, const std::vector<PluginInfo>& installed);

    class KitMapJob : private juce::Thread
    {
    public:
        KitMapJob() : juce::Thread ("Song Savant kit mapping") {}
        ~KitMapJob() override                               { cancelFlag = true; stopThread (10000); }

        void start (const juce::File& preset, const SystemLocations&);   // ignored while a kit is being mapped
        bool isMapping() const                              { return isThreadRunning(); }
        juce::String status() const;                        // "Playing key C1 (13 of 73)"
        juce::File preset() const;
        int generation() const                              { return gen.load(); }
        std::shared_ptr<const KitMap> result() const;
        void setResult (const juce::File& preset, KitMap);  // tests and screenshots

        int timeoutSeconds = 240;                           // a plug-in that takes longer is given up on

    private:
        void run() override;
        void setStatus (const juce::String&);

        mutable juce::CriticalSection lock;
        juce::File presetFile;
        SystemLocations loc;
        juce::String statusText;
        std::shared_ptr<const KitMap> latest;
        std::atomic<bool> cancelFlag { false };
        std::atomic<int> gen { 0 };
    };
}
