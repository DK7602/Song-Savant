// Song Savant - instrument makers' folders (Documents/Cymatics, ProgramData/Arturia, ...): what kinds of files
// they keep, so we can tell how each maker stores its presets. While walking, files we know how to look into
// (.vstpreset, .vstsound, databases) are collected for the other probes.
#pragma once

#include "FileWalk.h"
#include "PluginScanner.h"
#include <map>

namespace ss
{
    struct FoundFiles
    {
        juce::Array<juce::File> vstPresets, vstSounds, databases;
        void addUnique (juce::Array<juce::File>& list, const juce::File& f) { if (! list.contains (f)) list.add (f); }
    };

    struct VendorFolder
    {
        juce::String vendor;
        juce::File folder;
        WalkStats stats;
        std::vector<std::pair<juce::String, int>> extensions;   // most common first ("(none)" = no extension)
        std::map<juce::String, juce::StringArray> samples;       // extension -> a few paths inside the folder
        std::vector<std::pair<juce::String, int>> subfolders;    // first-level sub-folders and their file counts
    };

    // Makers worth looking for: the ones the plug-ins name, plus well-known instrument makers
    juce::StringArray vendorsToLookFor (const std::vector<PluginInfo>&);

    // "Arturia" -> every <vendor root>/Arturia (also "Steinberg Media Technologies" for "Steinberg", etc.)
    juce::Array<juce::File> findVendorFolders (const SystemLocations&, const juce::String& vendor);

    VendorFolder summariseFolder (const juce::String& vendor, const juce::File& folder, FoundFiles&,
                                  const std::atomic<bool>* cancel, const WalkLimits& = {});

    juce::String normaliseName (const juce::String&);   // "Native Instruments" -> "nativeinstruments"
}
