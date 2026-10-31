// Song Savant - which plug-ins are installed, read from the files on disk (nothing is loaded).
// VST3 bundles usually carry Contents/moduleinfo.json with the maker, version and categories.
#pragma once

#include "SystemLocations.h"
#include <atomic>
#include <vector>

namespace ss
{
    struct PluginInfo
    {
        juce::String name, vendor, version, format;   // format: "VST3", "VST2", "AU"
        juce::StringArray categories;                  // VST3 sub-categories: "Instrument", "Synth", "Drum", "Fx"...
        bool categoriesKnown = false;                  // false when the plug-in has no moduleinfo.json
        bool isInstrument = false;
        bool bundle = false;                           // a .vst3 folder rather than a single file
        juce::File file;
    };

    std::vector<PluginInfo> scanInstalledPlugins (const SystemLocations&, const std::atomic<bool>* cancel = nullptr);

    // moduleinfo.json is JSON5: allow // comments and trailing commas
    juce::var parseLenientJson (const juce::String&);
    bool readModuleInfo (const juce::File& moduleInfoJson, PluginInfo&);
}
