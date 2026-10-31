// Song Savant - VST3 preset files (.vstpreset): the header, the chunk list, and the "Info" chunk, an XML block
// of attributes. Steinberg's MediaBay tags (category, instrument, style, character...) live there.
//
//   "VST3" | int32 version | 32-char class ID | int64 offset of the chunk list
//   ... chunks ...
//   "List" | int32 count | count x { 4-char id, int64 offset, int64 size }      ids: Comp, Cont, Prog, Info
// All numbers little-endian.
#pragma once

#include <juce_core/juce_core.h>

namespace ss
{
    struct VstPresetInfo
    {
        bool valid = false;
        juce::String classId;            // which plug-in the preset belongs to
        juce::StringArray chunkIds;      // e.g. Comp, Cont, Info
        bool hasInfo = false;
        juce::StringPairArray attributes; // from the Info chunk: PlugInName, MusicalCategory, MusicalStyle...
        juce::int64 totalSize = 0;        // where the preset ends (its last chunk or the chunk list)
        juce::String error;
    };

    VstPresetInfo readVstPreset (const juce::File&);                          // reads only the header, list and Info
    VstPresetInfo parseVstPreset (const juce::uint8* data, size_t size);      // a preset held in memory

    juce::StringPairArray parseMetaInfo (const juce::String& xml);
    juce::String makeMetaInfoXml (const juce::StringPairArray&);

    // A complete preset (used by the tests, and later to hand presets to a DAW)
    juce::MemoryBlock makeVstPreset (const juce::String& classId, const juce::MemoryBlock& componentState,
                                     const juce::StringPairArray& attributes);

    // The tags that say what a preset sounds like, in reading order
    const juce::StringArray& soundTagKeys();   // MusicalCategory, MusicalInstrument, MusicalStyle, MusicalCharacter
}
