#include "Database.h"
#include "Paths.h"
#include <sqlite3.h>

namespace ss
{
    ReadOnlyDatabase::ReadOnlyDatabase (const juce::File& original)
    {
        if (! original.existsAsFile()) { lastError = "not found"; return; }
        copy = paths::tempDir().getChildFile ("db-copies").getNonexistentChildFile ("copy", ".db", false);
        copy.getParentDirectory().createDirectory();
        if (! original.copyFileTo (copy)) { lastError = "couldn't copy it"; copy = juce::File(); return; }
        if (auto wal = original.getSiblingFile (original.getFileName() + "-wal"); wal.existsAsFile())
            wal.copyFileTo (copy.getSiblingFile (copy.getFileName() + "-wal"));

        if (sqlite3_open_v2 (copy.getFullPathName().toRawUTF8(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
        {
            lastError = db != nullptr ? juce::String (sqlite3_errmsg (db)) : juce::String ("couldn't open it");
            if (db != nullptr) sqlite3_close (db);
            db = nullptr;
        }
        else
            sqlite3_busy_timeout (db, 2000);
    }

    ReadOnlyDatabase::~ReadOnlyDatabase()
    {
        if (db != nullptr) sqlite3_close (db);
        if (copy != juce::File())
        {
            copy.deleteFile();
            copy.getSiblingFile (copy.getFileName() + "-wal").deleteFile();
            copy.getSiblingFile (copy.getFileName() + "-shm").deleteFile();
        }
    }

    bool ReadOnlyDatabase::query (const juce::String& sql, const std::function<bool (sqlite3_stmt*)>& onRow)
    {
        if (db == nullptr) return false;
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2 (db, sql.toRawUTF8(), -1, &st, nullptr) != SQLITE_OK)
        {
            lastError = sqlite3_errmsg (db);
            return false;
        }
        while (sqlite3_step (st) == SQLITE_ROW)
            if (! onRow (st)) break;
        sqlite3_finalize (st);
        return true;
    }

    juce::StringArray ReadOnlyDatabase::columns (const juce::String& table)
    {
        juce::StringArray out;
        query ("PRAGMA table_info(\"" + table.replace ("\"", "\"\"") + "\")", [&] (sqlite3_stmt* st)
        {
            out.add (text (st, 1));
            return true;
        });
        return out;
    }

    juce::String ReadOnlyDatabase::text (sqlite3_stmt* st, int c)
    {
        auto* t = reinterpret_cast<const char*> (sqlite3_column_text (st, c));
        return t != nullptr ? juce::String::fromUTF8 (t, sqlite3_column_bytes (st, c)) : juce::String();
    }

    juce::int64 ReadOnlyDatabase::integer (sqlite3_stmt* st, int c)   { return (juce::int64) sqlite3_column_int64 (st, c); }
    double ReadOnlyDatabase::real (sqlite3_stmt* st, int c)           { return sqlite3_column_double (st, c); }
}
