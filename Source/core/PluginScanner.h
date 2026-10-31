// Song Savant - which plug-ins are installed, read from the files on disk (nothing is loaded).
// VST3 bundles usually carry Contents/moduleinfo.json with the maker, version and categories.
#pragma once

#include "SystemLocations.h"
#include <atomic>
#include <map>
#include <vector>

namespace ss
{
    struct PluginInfo
    {
        juce::String name, vendor, version, format;   // format: "VST3", "VST2", "AU"
        juce::StringArray categories;                  // VST3 sub-categories: "Instrument", "Synth", "Drum", "Fx"...
        bool categoriesKnown = false;                  // false when nothing on disk says what it is
        juce::String categorySource;                   // "moduleinfo" or "Cubase" (Cubase's own plug-in list)
        bool isInstrument = false;
        bool bundle = false;                           // a .vst3 folder rather than a single file
        juce::File file;
    };

    std::vector<PluginInfo> scanInstalledPlugins (const SystemLocations&, const std::atomic<bool>* cancel = nullptr);

    // Cubase's own list of the plug-ins it has scanned (VstPlugInfoV4.xml in its settings folder): name -> categories.
    // The layout isn't documented, so this looks for any element whose children name a plug-in and its category.
    std::map<juce::String, juce::StringArray> readCubasePluginList (const juce::File& xmlFile);

    // Fill in the type of plug-ins that didn't say, from Cubase's list (returns how many were filled in)
    int applyCubasePluginList (std::vector<PluginInfo>&, const std::map<juce::String, juce::StringArray>&);

    // moduleinfo.json is JSON5: allow // comments and trailing commas
    juce::var parseLenientJson (const juce::String&);
    bool readModuleInfo (const juce::File& moduleInfoJson, PluginInfo&);
}
