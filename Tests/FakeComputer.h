// Song Savant tests - a fake Windows computer on disk: plug-ins, Steinberg presets with tags, a VST Sound
// container, an Arturia-style database, a MediaBay database, Cymatics folders and the sibling apps' files.
#pragma once

#include "core/SystemLocations.h"

namespace sstest
{
    struct FakeComputer
    {
        juce::File root;
        ss::SystemLocations loc;
        juce::File halionPreset, untaggedPreset, vstSound, arturiaDb, mediaBayDb;
    };

    FakeComputer makeFakeComputer (const juce::File& root);
}
