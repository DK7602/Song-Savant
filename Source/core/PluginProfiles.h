// Song Savant - what well-known instruments are good for. When Song Savant can't read a plug-in's presets, it can
// still suggest the plug-in itself for a part ("SSD Sampler 5 for the drums: pick a kit"). Plug-ins without a profile
// fall back on the categories they declare (Instrument|Drum -> drums, Instrument|Piano -> piano...).
#pragma once

#include "PluginScanner.h"
#include "SoundKinds.h"
#include "Catalog.h"

namespace ss
{
    struct PluginProfile
    {
        const char* match;             // normalised name to look for ("ssdsampler", "mtpowerdrumkit")
        const char* name;              // how to call it ("SSD Sampler 5")
        std::vector<Kind> kinds;       // what it's good for, best first
        const char* words;             // describing words ("acoustic,rock,live,punchy")
        const char* note;              // a short description for the details
    };

    const PluginProfile* profileFor (const juce::String& pluginName);
    const PluginProfile* profileFor (const PluginInfo&);    // also tries the maker's name and folder ("Cymatics/Plinko")

    // Kinds from a plug-in's own categories: "Instrument|Drum" -> drums, "Synth" -> synth...
    std::vector<Kind> kindsFromCategories (const juce::StringArray& categories);

    // One entry per kind for each installed instrument whose presets aren't in the catalog already
    std::vector<Sound> pluginLevelSounds (const std::vector<PluginInfo>& installed, const std::vector<Sound>& alreadyRead);
}
