// Song Savant - a careful folder walk: depth, file-count and time limits, skips links and hidden folders,
// never goes inside plug-in bundles, and can be cancelled.
#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>

namespace ss
{
    struct WalkLimits
    {
        int maxDepth = 8;
        int maxFiles = 100000;
        double maxSeconds = 90.0;
    };

    struct WalkStats
    {
        int files = 0, folders = 0;
        juce::int64 bytes = 0;
        bool truncated = false;   // a limit was hit
    };

    // onFile is called for every file (return false to stop the walk).
    WalkStats walkFiles (const juce::File& root, const WalkLimits&,
                         const std::function<bool (const juce::File&, int depth)>& onFile,
                         const std::atomic<bool>* cancel = nullptr);

    // true for folders the walk should not enter: links/junctions, hidden or system folders, plug-in bundles
    bool skipFolder (const juce::File&);

    juce::String formatBytes (juce::int64);

    // Names that look like keys, passwords, licences or credentials: the scan never lists or opens them
    bool looksPrivate (const juce::String& fileOrFolderName);
}
