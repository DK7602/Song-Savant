#include "SqliteProbe.h"
#include "Paths.h"
#include <sqlite3.h>
#include <algorithm>

namespace ss
{
    bool looksLikeSqlite (const juce::File& f)
    {
        if (! f.existsAsFile() || f.getSize() < 512) return false;
        juce::FileInputStream in (f);
        char h[16] = {};
        return in.openedOk() && in.read (h, 16) == 16 && std::memcmp (h, "SQLite format 3\0", 16) == 0;
    }

    namespace
    {
        // file:///C:/Users/... with anything unusual %-escaped
        juce::String fileUri (const juce::File& f)
        {
            juce::String out ("file://");
           #if JUCE_WINDOWS
            out << "/";
           #endif
            const auto utf8 = f.getFullPathName().replaceCharacter ('\\', '/').toStdString();
            for (const char ch : utf8)
            {
                const auto c = static_cast<unsigned char> (ch);
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~')
                    out << (char) c;
                else
                    out << "%" << juce::String::toHexString ((int) c).paddedLeft ('0', 2);
            }
            return out + "?mode=ro&immutable=1";
        }

        juce::String quoteId (const juce::String& s)   { return "\"" + s.replace ("\"", "\"\"") + "\""; }

        int relevanceOf (const juce::String& table, const juce::StringArray& cols)
        {
            static const juce::StringArray words { "preset", "sound", "patch", "program", "bank", "instrument", "category", "type",
                                                   "character", "tag", "genre", "style", "mood", "name", "author", "favorite", "media" };
            int score = 0;
            for (auto& w : words)
            {
                if (table.containsIgnoreCase (w)) score += 3;
                for (auto& c : cols)
                    if (c.containsIgnoreCase (w)) { ++score; break; }
            }
            return score;
        }

        juce::String cell (sqlite3_stmt* st, int i)
        {
            switch (sqlite3_column_type (st, i))
            {
                case SQLITE_NULL:    return "null";
                case SQLITE_INTEGER: return juce::String ((juce::int64) sqlite3_column_int64 (st, i));
                case SQLITE_FLOAT:   return juce::String (sqlite3_column_double (st, i), 3);
                case SQLITE_BLOB:    return "<blob " + juce::String (sqlite3_column_bytes (st, i)) + " bytes>";
                default:
                {
                    auto* t = reinterpret_cast<const char*> (sqlite3_column_text (st, i));
                    auto s = juce::String::fromUTF8 (t != nullptr ? t : "", sqlite3_column_bytes (st, i))
                                 .replaceCharacters ("\r\n\t", "   ");
                    return s.length() > 60 ? s.substring (0, 57) + "..." : s;
                }
            }
        }

        struct Db
        {
            sqlite3* db = nullptr;
            ~Db() { if (db != nullptr) sqlite3_close (db); }
        };

        template <typename Fn>
        bool query (sqlite3* db, const juce::String& sql, Fn&& onRow)
        {
            sqlite3_stmt* st = nullptr;
            if (sqlite3_prepare_v2 (db, sql.toRawUTF8(), -1, &st, nullptr) != SQLITE_OK) return false;
            while (sqlite3_step (st) == SQLITE_ROW)
                if (! onRow (st)) break;
            sqlite3_finalize (st);
            return true;
        }
    }

    SqliteProbeResult probeSqlite (const juce::File& original, int tablesToSample, int rowsPerTable)
    {
        SqliteProbeResult r;
        r.file = original;
        r.size = original.getSize();

        // work on a copy (plus its -wal file, if the app that owns it is open right now)
        auto copy = paths::tempDir().getChildFile ("db-probe").getNonexistentChildFile ("probe", ".db", false);
        copy.getParentDirectory().createDirectory();
        const bool copied = r.size < 1024ll * 1024 * 1024 && original.copyFileTo (copy);
        if (copied)
            if (auto wal = original.getSiblingFile (original.getFileName() + "-wal"); wal.existsAsFile())
                wal.copyFileTo (copy.getSiblingFile (copy.getFileName() + "-wal"));

        Db db;
        int rc;
        if (copied)
        {
            rc = sqlite3_open_v2 (copy.getFullPathName().toRawUTF8(), &db.db, SQLITE_OPEN_READONLY, nullptr);
        }
        else
        {
            // couldn't copy it: open the original read-only, telling SQLite it won't change (takes no locks)
            rc = sqlite3_open_v2 (fileUri (original).toRawUTF8(), &db.db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
        }

        if (rc != SQLITE_OK)
        {
            r.error = db.db != nullptr ? juce::String (sqlite3_errmsg (db.db)) : juce::String ("couldn't open");
        }
        else
        {
            sqlite3_busy_timeout (db.db, 2000);
            juce::StringArray names;
            const bool ok = query (db.db, "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name",
                                   [&] (sqlite3_stmt* st)
                                   {
                                       names.add (juce::String::fromUTF8 (reinterpret_cast<const char*> (sqlite3_column_text (st, 0))));
                                       return names.size() < 120;
                                   });
            if (! ok) r.error = sqlite3_errmsg (db.db);
            r.opened = ok;

            for (auto& n : names)
            {
                SqliteTable t;
                t.name = n;
                query (db.db, "PRAGMA table_info(" + quoteId (n) + ")", [&] (sqlite3_stmt* st)
                {
                    t.columns.add (juce::String::fromUTF8 (reinterpret_cast<const char*> (sqlite3_column_text (st, 1))));
                    return true;
                });
                query (db.db, "SELECT COUNT(*) FROM " + quoteId (n), [&] (sqlite3_stmt* st)
                {
                    t.rows = (juce::int64) sqlite3_column_int64 (st, 0);
                    return false;
                });
                t.relevance = relevanceOf (n, t.columns);
                r.tables.push_back (t);
            }

            std::stable_sort (r.tables.begin(), r.tables.end(), [] (const SqliteTable& a, const SqliteTable& b)
            {
                if (a.relevance != b.relevance) return a.relevance > b.relevance;
                return a.rows > b.rows;
            });

            int sampled = 0;
            for (auto& t : r.tables)
            {
                if (sampled >= tablesToSample) break;
                if (t.rows <= 0) continue;
                ++sampled;
                query (db.db, "SELECT * FROM " + quoteId (t.name) + " LIMIT " + juce::String (rowsPerTable), [&] (sqlite3_stmt* st)
                {
                    juce::StringArray cells;
                    const int nc = std::min (sqlite3_column_count (st), 14);
                    for (int i = 0; i < nc; ++i)
                        cells.add (juce::String::fromUTF8 (sqlite3_column_name (st, i)) + "=" + cell (st, i));
                    if (sqlite3_column_count (st) > nc) cells.add ("...");
                    t.sampleRows.add (cells.joinIntoString (" | "));
                    return true;
                });
            }
        }

        if (copied)
        {
            sqlite3_close (db.db);
            db.db = nullptr;
            copy.deleteFile();
            copy.getSiblingFile (copy.getFileName() + "-wal").deleteFile();
            copy.getSiblingFile (copy.getFileName() + "-shm").deleteFile();
        }
        return r;
    }
}
