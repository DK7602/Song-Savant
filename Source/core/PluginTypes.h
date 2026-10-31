// Song Savant - finding out what a plug-in is when it doesn't say on disk: run "Song Savant Scanner" on it (one
// process per plug-in, with a time limit, the way DAWs check plug-ins), and remember the answer so each plug-in is
// only checked once (again only when it's updated).
#pragma once

#include "PluginScanner.h"
#include <atomic>
#include <functional>

namespace ss
{
    juce::File scannerProgram();                     // next to the app, or in Song Savant's install folder; may not exist

    struct CheckedPlugin { bool ok = false, instrument = false; juce::String category, vendor; };
    CheckedPlugin checkPlugin (const juce::File& plugin, const juce::File& scanner, const std::atomic<bool>& cancel,
                               int timeoutMs = 20000);

    // The installed plug-ins, with their types filled in from Cubase's list and from earlier checks (nothing is loaded)
    std::vector<PluginInfo> installedPlugins (const SystemLocations&, const std::atomic<bool>* cancel = nullptr);
    void applyKnownTypes (std::vector<PluginInfo>&);

    // VST3 plug-ins that still don't say what they are, and haven't been checked since they were installed or updated
    std::vector<PluginInfo> pluginsToCheck (const std::vector<PluginInfo>&);

    // Check them, each in its own process; progress (done, total) after each one. Returns how many were checked.
    int checkPlugins (const std::vector<PluginInfo>&, const std::atomic<bool>& cancel, int maxSeconds = 300,
                      std::function<void (int, int)> progress = {});
}
