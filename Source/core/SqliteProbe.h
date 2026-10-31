// Song Savant - a read-only look at an SQLite database (Arturia's preset library, Steinberg's MediaBay, ...):
// its tables, their columns and row counts, and a few sample rows from the ones that look like presets.
// The database is copied first and the copy is opened, so the original is never touched or locked.
#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace ss
{
    struct SqliteTable
    {
        juce::String name;
        juce::StringArray columns;
        juce::int64 rows = -1;
        juce::StringArray sampleRows;    // "col=value | col=value" (long text cut short, blobs as <blob N bytes>)
        int relevance = 0;               // how much it looks like preset data
    };

    struct SqliteProbeResult
    {
        juce::File file;
        juce::int64 size = 0;
        bool opened = false;
        juce::String error;
        std::vector<SqliteTable> tables;   // most preset-like first
    };

    bool looksLikeSqlite (const juce::File&);    // checks the 16-byte "SQLite format 3" header
    SqliteProbeResult probeSqlite (const juce::File&, int tablesToSample = 8, int rowsPerTable = 3);
}
