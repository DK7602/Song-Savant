// Song Savant - opens another app's SQLite database (MediaBay, Arturia) read-only from a private copy, so the
// original is never locked or changed, and runs queries on it.
#pragma once

#include <juce_core/juce_core.h>
#include <functional>

struct sqlite3;
struct sqlite3_stmt;

namespace ss
{
    class ReadOnlyDatabase
    {
    public:
        explicit ReadOnlyDatabase (const juce::File& original);
        ~ReadOnlyDatabase();

        bool isOpen() const                { return db != nullptr; }
        const juce::String& error() const  { return lastError; }

        // Calls onRow for each row (return false to stop). Returns false if the query failed.
        bool query (const juce::String& sql, const std::function<bool (sqlite3_stmt*)>& onRow);

        juce::StringArray columns (const juce::String& table);   // empty if the table doesn't exist
        bool hasTable (const juce::String& table)                 { return ! columns (table).isEmpty(); }

        // Reading a column of the current row
        static juce::String text (sqlite3_stmt*, int column);
        static juce::int64 integer (sqlite3_stmt*, int column);
        static double real (sqlite3_stmt*, int column);

    private:
        sqlite3* db = nullptr;
        juce::File copy;
        juce::String lastError;
        JUCE_DECLARE_NON_COPYABLE (ReadOnlyDatabase)
    };
}
