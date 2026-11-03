// Song Savant - Phase 0 "setup check": everything we can learn about this computer's instruments without
// loading a single plug-in. It only reads: nothing on the computer is changed, moved or locked.
#pragma once

#include "PluginScanner.h"
#include "VstPreset.h"
#include "VstSoundProbe.h"
#include "SqliteProbe.h"
#include "VendorProbe.h"
#include "Siblings.h"
#include <functional>

namespace ss
{
    struct PresetFiles
    {
        int files = 0, readable = 0, withTags = 0;
        std::map<juce::String, int> byPlugin;        // the plug-in each preset is for
        std::map<juce::String, int> attributeKeys;   // which Info attributes appear, and how often
        std::map<juce::String, int> categories;      // MusicalCategory values
        juce::StringArray samples;                   // "HALion Sonic: Grand Piano (Piano / A. Piano / Pop / Warm)"
        bool truncated = false;
    };

    struct FolderListing
    {
        juce::File folder;
        juce::StringArray entries;   // "Defaults.xml (12 KB)", "Presets/"
    };

    struct ScanResult
    {
        juce::Time started;
        double seconds = 0;
        bool cancelled = false;

        std::vector<PluginInfo> plugins;
        PresetFiles presets;
        int vstSoundFiles = 0;
        juce::int64 vstSoundBytes = 0;
        std::vector<VstSoundProbeResult> vstSounds;   // the ones looked into
        std::vector<SqliteProbeResult> databases;
        std::vector<VendorFolder> vendorFolders;
        std::vector<FolderListing> cubase;           // Cubase installs and settings folders
        juce::File cubasePluginList;                 // Cubase's VstPlugInfoV4.xml, if found
        int pluginTypesFromCubase = 0;               // plug-ins whose type came from it
        juce::StringArray cubasePluginListSample;    // its first lines, to learn its layout
        std::vector<std::pair<juce::String, int>> cubasePluginListKeys;   // name="..." values used in it
        SiblingApps siblings;
        juce::StringArray notes;                     // limits hit, things skipped
    };

    using ScanProgress = std::function<void (float fraction, const juce::String& stage)>;

    ScanResult runSetupScan (const SystemLocations&, const std::atomic<bool>& cancel, const ScanProgress& = {});
}
