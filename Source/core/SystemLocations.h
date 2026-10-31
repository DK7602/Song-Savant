// Song Savant - the places on this computer where plug-ins, presets and instrument content live.
// Built once for the real computer, or with the same Windows layout under a fake root for the tests.
#pragma once

#include <juce_core/juce_core.h>

namespace ss
{
    struct SystemLocations
    {
        juce::Array<juce::File> vst3Folders;     // VST3 plug-ins (.vst3 bundles or single files)
        juce::Array<juce::File> vst2Folders;     // older VST2 plug-ins (names only)
        juce::Array<juce::File> auFolders;       // Audio Units (Mac, names only)
        juce::Array<juce::File> presetRoots;     // VST3 preset folders: <root>/<maker>/<plug-in>/*.vstpreset
        juce::Array<juce::File> vendorRoots;     // where instrument makers keep their content and settings
        juce::Array<juce::File> cubaseInstalls;  // e.g. C:/Program Files/Steinberg/Cubase 14
        juce::Array<juce::File> cubaseSettings;  // e.g. %APPDATA%/Steinberg/Cubase 14_64
        juce::Array<juce::File> extraFolders;    // folders the user added ("also look in...")

        juce::File home;                          // shown as "~" in the report
        juce::File keyCompassSongs;               // Key Compass's "My Songs" (*.kcsong)
        juce::File grooveGrabberKits;             // Documents/Groove Grabber/Kits (*.ggkit)
        juce::File sampleSnaggerLibrary;          // Documents/Sample Snagger/Samples

        static SystemLocations forThisComputer();

        // The Windows layout under a fake root: <root>/Program Files, <root>/ProgramData,
        // <root>/Users/me/{Documents, AppData/Roaming, AppData/Local}, <root>/Users/Public/Documents
        static SystemLocations windowsLayoutUnder (const juce::File& root);

        juce::Array<juce::File> driveRoots;      // tops of the drives: makers' folders must match exactly there

        // Replaces the user's home folder with "~" anywhere in the text, in every form it appears in:
        // C:\Users\name, C:/Users/name, /Users/name, \Users\name (keeps the user's name out of the report)
        juce::String tidy (const juce::File&) const;
        juce::String tidy (const juce::String& text) const;
    };
}
