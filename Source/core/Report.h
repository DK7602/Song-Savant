// Song Savant - the setup report: a short summary (also shown on screen) and the full text file the owner sends
// back. Paths in the user's own folder start with "~" so their user name isn't in it.
#pragma once

#include "SetupScan.h"

namespace ss
{
    struct SummaryItem
    {
        juce::String title, detail;
        int status = 0;   // 1 = found and readable, 0 = not found / nothing to do, -1 = found but needs more work
    };

    // 0 = not tried, 1 = worked, -1 = didn't work
    struct DragTests
    {
        int userPreset = 0, factoryPreset = 0, midiClip = 0;
        juce::StringArray fromDaw;   // things dragged from Cubase onto Song Savant (describeDroppedFile lines)
        juce::String notes;
    };

    std::vector<SummaryItem> summarise (const ScanResult&);
    juce::String buildReport (const ScanResult&, const SystemLocations&, const DragTests&);
    juce::String withCommas (juce::int64);   // 12345 -> "12,345"
}
