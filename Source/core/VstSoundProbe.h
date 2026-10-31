// Song Savant - a first look inside Steinberg .vstsound containers (HALion, Groove Agent, Padshop, Retrologue
// content). The format isn't documented, so this only looks for things we'd recognise: preset file names,
// embedded VST3 presets and their MediaBay tags. The report shows what was found, to decide how to read them.
#pragma once

#include <juce_core/juce_core.h>
#include <map>

namespace ss
{
    struct VstSoundProbeResult
    {
        juce::File file;
        juce::int64 size = 0;
        juce::String headerHex, headerText;       // the first 32 bytes
        juce::int64 bytesScanned = 0;
        bool scannedWhole = false;
        int presetNames = 0;                      // ".vstpreset" names (ASCII or UTF-16)
        int embeddedPresets = 0;                  // "VST3" preset headers
        int embeddedWithTags = 0;                 // ...that we could read the Info chunk of
        int metaInfoBlocks = 0;                   // loose "<MetaInfo" XML blocks
        juce::StringArray sampleNames;            // a few preset names
        std::map<juce::String, int> plugins;      // PlugInName -> count
        std::map<juce::String, int> categories;   // MusicalCategory -> count
        juce::StringArray sampleTags;             // a few "Category / Instrument / Style / Character" lines

        struct Embedded { juce::int64 offset = -1, size = 0; juce::String plugin, label; };
        Embedded example;                         // the first embedded preset with tags (offset in the file)
    };

    // Copy an embedded preset out of its container into a .vstpreset file
    bool extractEmbeddedPreset (const juce::File& container, juce::int64 offset, juce::int64 size, const juce::File& dest);

    // budget: bytes this probe may still read (it is reduced by what's read). Files over 64 MB are read
    // at the start and the end only.
    VstSoundProbeResult probeVstSound (const juce::File&, juce::int64& budget);
}
