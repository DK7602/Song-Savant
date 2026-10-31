// Song Savant - where Song Savant keeps its own files.
#pragma once

#include <juce_core/juce_core.h>

namespace ss::paths
{
    juce::File documentsDir();   // Documents/Song Savant (reports; later: palettes, exports)
    juce::File appDataDir();     // .../Song Savant (settings, caches)
    juce::File reportFile();     // Documents/Song Savant/Setup Report.txt
    juce::File tempDir();        // a scratch folder for drag-and-drop files and database copies
}
