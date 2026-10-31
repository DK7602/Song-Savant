#include "SetupScan.h"
#include "Paths.h"
#include <algorithm>

namespace ss
{
    namespace
    {
        // the plug-in a .vstpreset belongs to: its Info chunk, else the folder (<root>/<maker>/<plug-in>/...)
        juce::String pluginForPreset (const juce::File& f, const VstPresetInfo& info, const SystemLocations& loc)
        {
            if (auto n = info.attributes["PlugInName"]; n.isNotEmpty()) return n;
            for (auto& root : loc.presetRoots)
                if (f.isAChildOf (root))
                {
                    auto rel = f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
                    auto parts = juce::StringArray::fromTokens (rel, "/", "");
                    if (parts.size() >= 3) return parts[1];
                }
            return f.getParentDirectory().getFileName();
        }

        int dragPreference (const juce::String& plugin, const VstPresetInfo& info)
        {
            static const std::pair<const char*, int> prefs[] = { { "HALion Sonic", 6 }, { "HALion", 5 }, { "Retrologue", 4 },
                                                                   { "Padshop", 4 }, { "Groove Agent", 2 } };
            for (auto& [name, score] : prefs)
                if (plugin.containsIgnoreCase (name)) return score;
            return info.attributes["PlugInCategory"].containsIgnoreCase ("Instrument") ? 3 : 1;
        }

        FolderListing listFolder (const juce::File& dir, const SystemLocations& loc)
        {
            FolderListing l;
            l.folder = dir;
            for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories))
            {
                if (l.entries.size() >= 120) { l.entries.add ("..."); break; }
                const auto f = e.getFile();
                if (looksPrivate (f.getFileName())) continue;
                if (e.isDirectory())
                {
                    int n = 0;
                    for (const auto& c : juce::RangedDirectoryIterator (f, false, "*", juce::File::findFilesAndDirectories))
                    {
                        juce::ignoreUnused (c);
                        if (++n > 999) break;
                    }
                    l.entries.add (f.getFileName() + "/  (" + (n > 999 ? juce::String ("1000+") : juce::String (n)) + " items)");
                }
                else
                    l.entries.add (f.getFileName() + "  (" + formatBytes (e.getFileSize()) + ")");
            }
            juce::ignoreUnused (loc);
            l.entries.sortNatural();
            return l;
        }
    }

    ScanResult runSetupScan (const SystemLocations& loc, const std::atomic<bool>& cancel, const ScanProgress& progress)
    {
        ScanResult r;
        r.started = juce::Time::getCurrentTime();
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto report = [&] (float f, const juce::String& stage) { if (progress) progress (f, stage); };
        auto stopped = [&] { if (cancel.load()) { r.cancelled = true; return true; } return false; };
        FoundFiles found;

        // 1. plug-ins
        report (0.02f, "Listing your plug-ins...");
        r.plugins = scanInstalledPlugins (loc, &cancel);
        if (stopped()) return r;

        // 2. VST3 preset folders
        report (0.08f, "Looking for preset files...");
        for (auto& root : loc.presetRoots)
        {
            auto st = walkFiles (root, { 8, 200000, 120.0 }, [&] (const juce::File& f, int)
            {
                if (f.hasFileExtension (".vstpreset")) found.addUnique (found.vstPresets, f);
                return true;
            }, &cancel);
            if (st.truncated && ! cancel.load()) r.notes.add ("Stopped early in " + loc.tidy (root) + " (very large folder).");
        }
        if (stopped()) return r;

        // 3. instrument makers' folders (also collects .vstsound, databases and stray .vstpreset files)
        const auto vendors = vendorsToLookFor (r.plugins);
        juce::Array<juce::File> folders;
        juce::StringArray folderVendors;
        for (auto& v : vendors)
            for (auto& f : findVendorFolders (loc, v))
                if (! folders.contains (f)) { folders.add (f); folderVendors.add (v); }
        for (auto& f : loc.extraFolders)
            if (! folders.contains (f)) { folders.add (f); folderVendors.add ("(a folder you added)"); }

        for (int i = 0; i < folders.size(); ++i)
        {
            report (0.12f + 0.38f * (float) i / (float) juce::jmax (1, folders.size()), "Looking in " + folders[i].getFileName() + "...");
            auto vf = summariseFolder (folderVendors[i], folders[i], found, &cancel, { 9, 80000, 90.0 });
            if (vf.stats.truncated && ! cancel.load())
                r.notes.add ("Stopped early in " + loc.tidy (folders[i]) + " (a limit was reached).");
            if (vf.stats.files > 0) r.vendorFolders.push_back (std::move (vf));
            if (stopped()) return r;
        }

        // 4. read every .vstpreset's header and tags
        report (0.52f, "Reading preset tags...");
        std::pair<int, juce::File> bestDrag { 0, {} };
        juce::String bestDragLabel;
        for (int i = 0; i < found.vstPresets.size(); ++i)
        {
            if (i >= 60000) { r.presets.truncated = true; break; }
            if (i % 200 == 0)
            {
                report (0.52f + 0.13f * (float) i / (float) found.vstPresets.size(), "Reading preset tags (" + juce::String (i) + ")...");
                if (stopped()) return r;
            }
            const auto& f = found.vstPresets.getReference (i);
            ++r.presets.files;
            const auto info = readVstPreset (f);
            if (! info.valid) continue;
            ++r.presets.readable;
            const auto plugin = pluginForPreset (f, info, loc);
            r.presets.byPlugin[plugin]++;
            for (auto& k : info.attributes.getAllKeys()) r.presets.attributeKeys[k]++;

            juce::StringArray tags;
            for (auto& k : soundTagKeys())
                if (info.attributes[k].isNotEmpty()) tags.add (info.attributes[k]);
            if (! tags.isEmpty())
            {
                ++r.presets.withTags;
                if (auto c = info.attributes["MusicalCategory"]; c.isNotEmpty()) r.presets.categories[c]++;
            }
            if (r.presets.samples.size() < 12 && (! tags.isEmpty() || r.presets.samples.size() < 4))
                r.presets.samples.add (plugin + ": " + f.getFileNameWithoutExtension()
                                       + (tags.isEmpty() ? juce::String (" (no tags)") : " (" + tags.joinIntoString (" / ") + ")"));

            if (const int pref = dragPreference (plugin, info); pref > bestDrag.first)
            {
                bestDrag = { pref, f };
                bestDragLabel = plugin + ": " + f.getFileNameWithoutExtension();
            }
        }
        r.userPreset = bestDrag.second;
        r.userPresetLabel = bestDragLabel;

        // 5. look inside Steinberg .vstsound libraries (preset libraries first: they're small)
        r.vstSoundFiles = found.vstSounds.size();
        for (auto& f : found.vstSounds) r.vstSoundBytes += f.getSize();
        auto sounds = found.vstSounds;
        std::sort (sounds.begin(), sounds.end(), [] (const juce::File& a, const juce::File& b) { return a.getSize() < b.getSize(); });
        juce::int64 budget = 3ll * 1024 * 1024 * 1024;
        const int maxProbed = 250;
        for (int i = 0; i < sounds.size() && i < maxProbed; ++i)
        {
            if (i % 5 == 0)
            {
                report (0.66f + 0.22f * (float) i / (float) juce::jmin (maxProbed, sounds.size()), "Looking inside sound libraries (" + juce::String (i + 1) + " of " + juce::String (sounds.size()) + ")...");
                if (stopped()) return r;
            }
            r.vstSounds.push_back (probeVstSound (sounds[i], budget));
        }
        if (sounds.size() > maxProbed) r.notes.add ("Looked inside the " + juce::String (maxProbed) + " smallest of " + juce::String (sounds.size()) + " sound libraries.");
        if (budget <= 0) r.notes.add ("Stopped reading sound libraries after 3 GB.");

        for (auto& p : r.vstSounds)
            if (p.example.offset >= 0)
            {
                auto name = p.example.plugin.isNotEmpty() ? p.example.plugin : juce::String ("Factory");
                if (p.example.label.isNotEmpty()) name << " - " << p.example.label;
                auto dest = paths::tempDir().getChildFile ("Drag Test").getChildFile (juce::File::createLegalFileName (name) + ".vstpreset");
                if (extractEmbeddedPreset (p.file, p.example.offset, p.example.size, dest))
                {
                    r.factoryPreset = dest;
                    r.factoryPresetLabel = name + "  (from " + p.file.getFileName() + ")";
                }
                break;
            }

        // 6. databases (Arturia, Steinberg MediaBay, ...): also look for MediaBay in Cubase's settings
        report (0.9f, "Checking preset databases...");
        for (auto& dir : loc.cubaseSettings)
            walkFiles (dir, { 3, 5000, 20.0 }, [&] (const juce::File& f, int)
            {
                if (looksLikeSqlite (f)) found.addUnique (found.databases, f);
                return true;
            }, &cancel);
        int probed = 0;
        for (auto& f : found.databases)
        {
            if (! looksLikeSqlite (f)) continue;
            if (++probed > 40) { r.notes.add ("Only the first 40 databases were opened."); break; }
            r.databases.push_back (probeSqlite (f));
            if (stopped()) return r;
        }

        // 7. Cubase, and the owner's other apps
        report (0.96f, "Checking Cubase and your other apps...");
        for (auto& d : loc.cubaseSettings)
        {
            r.cubase.push_back (listFolder (d, loc));
            if (r.cubasePluginList != juce::File()) continue;
            for (const auto& e : juce::RangedDirectoryIterator (d, false, "VstPlugInfo*.xml", juce::File::findFiles))
            {
                r.cubasePluginList = e.getFile();
                r.pluginTypesFromCubase = applyCubasePluginList (r.plugins, readCubasePluginList (e.getFile()));

                juce::StringArray lines;
                lines.addLines (e.getFile().loadFileAsString().substring (0, 400000));
                for (int i = 0; i < lines.size() && r.cubasePluginListSample.size() < 45; ++i)
                    if (lines[i].trim().isNotEmpty()) r.cubasePluginListSample.add (lines[i].substring (0, 200));
                std::map<juce::String, int> keys;
                for (auto& l : lines)
                    for (int pos = l.indexOf ("name=\""); pos >= 0; pos = l.indexOf (pos + 6, "name=\""))
                        keys[l.substring (pos + 6).upToFirstOccurrenceOf ("\"", false, false)]++;
                r.cubasePluginListKeys.assign (keys.begin(), keys.end());
                std::sort (r.cubasePluginListKeys.begin(), r.cubasePluginListKeys.end(), [] (auto& a, auto& b) { return a.second > b.second; });
                if (r.cubasePluginListKeys.size() > 40) r.cubasePluginListKeys.resize (40);
                break;
            }
        }
        for (auto& d : loc.cubaseInstalls) r.cubase.push_back (listFolder (d, loc));
        r.siblings = checkSiblingApps (loc);

        r.seconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        report (1.0f, "Done");
        return r;
    }
}
