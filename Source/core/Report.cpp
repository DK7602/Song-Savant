#include "Report.h"
#include "FileWalk.h"
#include <algorithm>

namespace ss
{
    juce::String withCommas (juce::int64 n)
    {
        auto s = juce::String (n < 0 ? -n : n);
        juce::String out;
        for (int i = 0; i < s.length(); ++i)
        {
            if (i > 0 && (s.length() - i) % 3 == 0) out << ",";
            out << s[i];
        }
        return n < 0 ? "-" + out : out;
    }

    namespace
    {
        juce::String plural (juce::int64 n, const juce::String& one, const juce::String& many)
        {
            return withCommas (n) + " " + (n == 1 ? one : many);
        }

        bool pathHas (const juce::File& f, const juce::String& word)   { return f.getFullPathName().containsIgnoreCase (word); }

        const VendorFolder* firstVendor (const ScanResult& r, const juce::String& vendor, int& folders, juce::int64& files)
        {
            const VendorFolder* first = nullptr;
            folders = 0;
            files = 0;
            for (auto& v : r.vendorFolders)
                if (normaliseName (v.vendor).startsWith (normaliseName (vendor)) || pathHas (v.folder, vendor))
                {
                    if (first == nullptr) first = &v;
                    ++folders;
                    files += v.stats.files;
                }
            return first;
        }

        juce::String topExtensions (const ScanResult& r, const juce::String& vendor, int maxN = 4)
        {
            std::map<juce::String, int> ext;
            for (auto& v : r.vendorFolders)
                if (normaliseName (v.vendor).startsWith (normaliseName (vendor)) || pathHas (v.folder, vendor))
                    for (auto& [e, n] : v.extensions) ext[e] += n;
            std::vector<std::pair<juce::String, int>> list (ext.begin(), ext.end());
            std::sort (list.begin(), list.end(), [] (auto& a, auto& b) { return a.second > b.second; });
            juce::StringArray s;
            for (size_t i = 0; i < list.size() && (int) i < maxN; ++i) s.add (list[i].first + " (" + withCommas (list[i].second) + ")");
            return s.joinIntoString (", ");
        }

        const SqliteTable* biggestPresetTable (const SqliteProbeResult& db)
        {
            const SqliteTable* best = nullptr;
            for (auto& t : db.tables)
                if (t.relevance > 0 && (best == nullptr || t.rows > best->rows)) best = &t;
            return best;
        }

        SummaryItem databaseItem (const ScanResult& r, const juce::String& title, std::function<bool (const juce::File&)> match,
                                  const juce::String& notFound)
        {
            SummaryItem it { title, notFound, 0 };
            for (auto& db : r.databases)
            {
                if (! match (db.file)) continue;
                if (! db.opened)
                {
                    it.detail = "Database found (" + db.file.getFileName() + ") but it couldn't be opened: " + db.error;
                    it.status = -1;
                    continue;
                }
                juce::String d = "Database found and readable (" + db.file.getFileName() + ", " + plural ((int) db.tables.size(), "table", "tables");
                if (auto* t = biggestPresetTable (db)) d << "; \"" << t->name << "\" has " << plural (t->rows, "row", "rows");
                it.detail = d + ")";
                it.status = 1;
                return it;
            }
            return it;
        }

        juce::String status (int s)   { return s > 0 ? "[OK]  " : s < 0 ? "[!!]  " : "[--]  "; }
        juce::String dragResult (int s) { return s > 0 ? "WORKED" : s < 0 ? "DIDN'T WORK" : "not tried"; }

        void heading (juce::String& o, const juce::String& title)
        {
            o << "\n\n" << title.toUpperCase() << "\n" << juce::String::repeatedString ("=", title.length()) << "\n";
        }
    }

    std::vector<SummaryItem> summarise (const ScanResult& r)
    {
        std::vector<SummaryItem> out;

        int vst3 = 0, inst = 0, unknown = 0;
        for (auto& p : r.plugins)
            if (p.format == "VST3")
            {
                ++vst3;
                if (p.isInstrument) ++inst;
                if (! p.categoriesKnown) ++unknown;
            }
        out.push_back ({ "Plug-ins",
                         vst3 == 0 ? juce::String ("No VST3 plug-ins found")
                                   : plural (vst3, "VST3 plug-in", "VST3 plug-ins") + ", " + withCommas (inst) + " of them instruments"
                                       + (unknown > 0 ? " (" + withCommas (unknown) + " didn't say what they are)" : juce::String()),
                         vst3 > 0 ? 1 : -1 });

        {
            SummaryItem it { "Preset files (.vstpreset)", "None found as separate files", 0 };
            if (r.presets.files > 0)
            {
                it.detail = plural (r.presets.files, "file", "files") + ", " + withCommas (r.presets.withTags) + " with sound tags";
                it.status = r.presets.withTags > 0 ? 1 : -1;
            }
            out.push_back (it);
        }

        {
            SummaryItem it { "Steinberg sound libraries (.vstsound)", "None found", 0 };
            if (r.vstSoundFiles > 0)
            {
                int names = 0, embedded = 0, tagged = 0, meta = 0;
                for (auto& p : r.vstSounds) { names += p.presetNames; embedded += p.embeddedPresets; tagged += p.embeddedWithTags; meta += p.metaInfoBlocks; }
                it.detail = plural (r.vstSoundFiles, "library", "libraries") + " (" + formatBytes (r.vstSoundBytes) + "). Inside: "
                            + plural (names, "preset name", "preset names") + ", " + plural (embedded, "preset", "presets") + ", "
                            + plural (tagged + meta, "set of tags", "sets of tags");
                it.status = (tagged + meta) > 0 ? 1 : -1;
            }
            out.push_back (it);
        }

        out.push_back (databaseItem (r, "Arturia (Analog Lab)", [] (const juce::File& f) { return pathHas (f, "Arturia"); },
                                     "No Arturia preset database found"));
        out.push_back (databaseItem (r, "Steinberg MediaBay database",
                                     [] (const juce::File& f) { return pathHas (f, "Steinberg") || pathHas (f, "Cubase") || pathHas (f, "MediaBay"); },
                                     "Not found"));

        for (auto* maker : { "Cymatics", "Native Instruments" })
        {
            int folders = 0; juce::int64 files = 0;
            if (firstVendor (r, maker, folders, files) != nullptr)
                out.push_back ({ maker, plural (folders, "folder", "folders") + ", " + plural (files, "file", "files") + ". Mostly: " + topExtensions (r, maker), 1 });
            else if (juce::String (maker) == "Cymatics")
                out.push_back ({ maker, "No Cymatics folders found", 0 });
        }

        {
            const auto& s = r.siblings;
            out.push_back ({ "Key Compass", ! s.keyCompassFolder ? juce::String ("Not found")
                                            : s.songs.empty() ? juce::String ("Found, no songs yet")
                                            : plural ((int) s.songs.size(), "song", "songs") + " (newest: " + s.songs.front().name + ")",
                             s.keyCompassFolder ? 1 : 0 });
            out.push_back ({ "Groove Grabber", ! s.grooveGrabberFolder ? juce::String ("No saved kits folder yet")
                                               : plural (s.grooveGrabberKits, "saved kit", "saved kits"),
                             s.grooveGrabberFolder ? 1 : 0 });
            out.push_back ({ "Sample Snagger", s.sampleSnaggerFolder ? plural (s.sampleSnaggerFiles, "file", "files") + " in its library"
                                                                     : juce::String ("Library folder not found"),
                             s.sampleSnaggerFolder ? 1 : 0 });
        }
        return out;
    }

    juce::String buildReport (const ScanResult& r, const SystemLocations& loc, const DragTests& drag)
    {
        juce::String o;
        o << "SONG SAVANT - SETUP REPORT\n"
          << "Song Savant " << SS_VERSION << " | " << juce::SystemStats::getOperatingSystemName() << " | "
          << r.started.formatted ("%d %b %Y %H:%M") << " | scan took " << juce::String (r.seconds, 0) << " seconds"
          << (r.cancelled ? " (STOPPED EARLY)" : "") << "\n"
          << "Nothing on this computer was changed. Paths in your user folder start with ~ (your user name is left out).\n";

        heading (o, "Summary");
        for (auto& it : summarise (r)) o << status (it.status) << it.title << ": " << it.detail << "\n";

        heading (o, "Drag tests (Cubase)");
        o << "Preset file -> Cubase:           " << dragResult (drag.userPreset)
          << (r.userPreset != juce::File() ? "   (" + r.userPresetLabel + ")" : juce::String ("   (no preset file was found to test)")) << "\n";
        o << "Factory preset (copied out) -> Cubase: " << dragResult (drag.factoryPreset)
          << (r.factoryPreset != juce::File() ? "   (" + r.factoryPresetLabel + ")" : juce::String ("   (no preset could be copied out of a library)")) << "\n";
        o << "MIDI clip -> Cubase:             " << dragResult (drag.midiClip) << "\n";
        if (drag.notes.isNotEmpty()) o << "Notes: " << drag.notes << "\n";

        // ---- plug-ins
        heading (o, "1. Plug-ins");
        auto listPlugins = [&] (const juce::String& title, auto pred)
        {
            juce::StringArray lines;
            for (auto& p : r.plugins)
                if (pred (p))
                {
                    juce::String line = "  " + p.name;
                    if (p.vendor.isNotEmpty()) line << "  |  " << p.vendor;
                    if (p.version.isNotEmpty()) line << "  |  " << p.version;
                    if (! p.categories.isEmpty()) line << "  |  " << p.categories.joinIntoString ("/");
                    if (p.format == "VST3" && ! p.bundle) line << "  |  single file";
                    lines.add (line);
                }
            o << title << " (" << lines.size() << "):\n" << (lines.isEmpty() ? juce::String ("  none\n") : lines.joinIntoString ("\n") + "\n");
        };
        listPlugins ("VST3 instruments", [] (const PluginInfo& p) { return p.format == "VST3" && p.isInstrument; });
        listPlugins ("VST3 that didn't say what they are", [] (const PluginInfo& p) { return p.format == "VST3" && ! p.categoriesKnown; });
        listPlugins ("VST3 effects and others", [] (const PluginInfo& p) { return p.format == "VST3" && p.categoriesKnown && ! p.isInstrument; });
        listPlugins ("VST2 (names only)", [] (const PluginInfo& p) { return p.format == "VST2"; });
        listPlugins ("Audio Units (names only)", [] (const PluginInfo& p) { return p.format == "AU"; });
        o << "Plug-in folders: ";
        juce::StringArray dirs;
        for (auto& d : loc.vst3Folders) dirs.add (loc.tidy (d));
        for (auto& d : loc.vst2Folders) dirs.add (loc.tidy (d));
        o << (dirs.isEmpty() ? juce::String ("none") : dirs.joinIntoString ("; ")) << "\n";

        // ---- presets
        heading (o, "2. Preset files (.vstpreset)");
        o << "Files: " << withCommas (r.presets.files) << ", readable: " << withCommas (r.presets.readable)
          << ", with sound tags: " << withCommas (r.presets.withTags) << (r.presets.truncated ? " (stopped at 60,000)" : "") << "\n";
        if (! r.presets.byPlugin.empty())
        {
            o << "By plug-in:\n";
            for (auto& [name, n] : r.presets.byPlugin) o << "  " << name << ": " << withCommas (n) << "\n";
        }
        if (! r.presets.attributeKeys.empty())
        {
            o << "Tags seen (how many presets have each):\n";
            for (auto& [k, n] : r.presets.attributeKeys) o << "  " << k << ": " << withCommas (n) << "\n";
        }
        if (! r.presets.categories.empty())
        {
            o << "Categories:\n  ";
            juce::StringArray c;
            for (auto& [k, n] : r.presets.categories) c.add (k + " (" + withCommas (n) + ")");
            o << c.joinIntoString (", ") << "\n";
        }
        if (! r.presets.samples.isEmpty()) o << "Examples:\n  " << r.presets.samples.joinIntoString ("\n  ") << "\n";
        o << "Preset folders: ";
        juce::StringArray proots;
        for (auto& d : loc.presetRoots) proots.add (loc.tidy (d));
        o << (proots.isEmpty() ? juce::String ("none") : proots.joinIntoString ("; ")) << "\n";

        // ---- vstsound
        heading (o, "3. Steinberg sound libraries (.vstsound)");
        o << "Found " << withCommas (r.vstSoundFiles) << " (" << formatBytes (r.vstSoundBytes) << "), looked inside " << withCommas ((int) r.vstSounds.size()) << ".\n";
        {
            std::map<juce::String, int> plugins, cats;
            for (auto& p : r.vstSounds)
            {
                for (auto& [k, n] : p.plugins) plugins[k] += n;
                for (auto& [k, n] : p.categories) cats[k] += n;
            }
            if (! plugins.empty())
            {
                o << "Plug-ins named inside: ";
                juce::StringArray s;
                for (auto& [k, n] : plugins) s.add (k + " (" + withCommas (n) + ")");
                o << s.joinIntoString (", ") << "\n";
            }
            if (! cats.empty())
            {
                o << "Categories inside: ";
                juce::StringArray s;
                for (auto& [k, n] : cats) s.add (k + " (" + withCommas (n) + ")");
                o << s.joinIntoString (", ") << "\n";
            }
        }
        int shown = 0;
        for (auto& p : r.vstSounds)
        {
            ++shown;
            o << "\n" << loc.tidy (p.file) << "  [" << formatBytes (p.size) << (p.scannedWhole ? ", read whole" : ", read start and end") << "]\n"
              << "  first bytes: " << p.headerHex << "  |" << p.headerText << "|\n"
              << "  preset names " << p.presetNames << ", presets " << p.embeddedPresets << " (" << p.embeddedWithTags << " with tags), tag blocks " << p.metaInfoBlocks << "\n";
            if (! p.sampleNames.isEmpty()) o << "  names: " << p.sampleNames.joinIntoString (" ; ") << "\n";
            if (! p.sampleTags.isEmpty())  o << "  tags:  " << p.sampleTags.joinIntoString (" ; ") << "\n";
            if (shown <= 6 && ! p.textAtStart.isEmpty()) o << "  text near the start: " << p.textAtStart.joinIntoString (" ; ") << "\n";
        }

        // ---- databases
        heading (o, "4. Preset databases");
        if (r.databases.empty()) o << "None found.\n";
        for (auto& db : r.databases)
        {
            o << "\n" << loc.tidy (db.file) << "  [" << formatBytes (db.size) << "]  "
              << (db.opened ? juce::String ("opened") : "couldn't open: " + db.error) << "\n";
            for (auto& t : db.tables)
            {
                o << "  table " << t.name << " (" << withCommas (t.rows) << " rows): " << t.columns.joinIntoString (", ") << "\n";
                for (auto& row : t.sampleRows) o << "      " << row << "\n";
            }
        }

        // ---- vendor folders
        heading (o, "5. Instrument makers' folders");
        if (r.vendorFolders.empty()) o << "None found.\n";
        for (auto& v : r.vendorFolders)
        {
            o << "\n" << v.vendor << ": " << loc.tidy (v.folder) << "  [" << withCommas (v.stats.files) << " files, "
              << formatBytes (v.stats.bytes) << (v.stats.truncated ? ", stopped early" : "") << "]\n";
            juce::StringArray subs;
            for (size_t i = 0; i < v.subfolders.size() && i < 12; ++i) subs.add (v.subfolders[i].first + " (" + withCommas (v.subfolders[i].second) + ")");
            if (! subs.isEmpty()) o << "  folders: " << subs.joinIntoString (", ") << "\n";
            for (size_t i = 0; i < v.extensions.size() && i < 10; ++i)
            {
                auto& [ext, n] = v.extensions[i];
                o << "  " << ext << " x" << withCommas (n);
                if (auto it = v.samples.find (ext); it != v.samples.end() && ! it->second.isEmpty())
                    o << "   e.g. " << it->second.joinIntoString (" ; ");
                o << "\n";
            }
        }

        // ---- Cubase
        heading (o, "6. Cubase");
        if (r.cubase.empty()) o << "No Cubase folders found.\n";
        for (auto& c : r.cubase)
            o << "\n" << loc.tidy (c.folder) << "\n  " << c.entries.joinIntoString ("\n  ") << "\n";

        // ---- other apps
        heading (o, "7. Your other apps");
        const auto& s = r.siblings;
        o << "Key Compass songs (" << loc.tidy (loc.keyCompassSongs) << "): " << (s.keyCompassFolder ? juce::String ((int) s.songs.size()) : juce::String ("folder not found")) << "\n";
        for (auto& song : s.songs)
            o << "  " << song.name << "  |  " << keyName (song.key) << "  |  " << (song.bpm > 0 ? juce::String (song.bpm, 0) + " BPM" : juce::String ("no BPM"))
              << "  |  sections: " << song.sections.joinIntoString (", ") << "  |  " << plural (song.chords.size(), "chord", "chords") << "\n";
        o << "Groove Grabber kits (" << loc.tidy (loc.grooveGrabberKits) << "): " << (s.grooveGrabberFolder ? juce::String (s.grooveGrabberKits) : juce::String ("folder not found")) << "\n";
        for (auto& [g, n] : s.kitsByGenre) o << "  " << g << ": " << n << "\n";
        for (auto& k : s.kitSamples) o << "  e.g. " << k << "\n";
        o << "Sample Snagger library (" << loc.tidy (loc.sampleSnaggerLibrary) << "): "
          << (s.sampleSnaggerFolder ? plural (s.sampleSnaggerFiles, "file", "files") : juce::String ("not found")) << "\n";

        if (! r.notes.isEmpty())
        {
            heading (o, "Notes");
            o << r.notes.joinIntoString ("\n") << "\n";
        }
        if (! loc.extraFolders.isEmpty())
        {
            o << "\nFolders you added: ";
            juce::StringArray x;
            for (auto& f : loc.extraFolders) x.add (loc.tidy (f));
            o << x.joinIntoString ("; ") << "\n";
        }
        const auto home = loc.home.getFullPathName();
        return home.length() > 3 ? o.replace (home, "~", true) : o;
    }
}
