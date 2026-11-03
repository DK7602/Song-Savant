#include "Catalog.h"
#include "Database.h"
#include "FileWalk.h"
#include "VstPreset.h"
#include "PluginProfiles.h"
#include "PluginTypes.h"
#include <sqlite3.h>
#include <algorithm>
#include <map>
#include <set>

namespace ss
{
    namespace
    {
        void addParts (juce::StringArray& list, const juce::String& text, bool lastPartOnly = false)
        {
            auto parts = juce::StringArray::fromTokens (text, "|", "");
            parts.trim();
            parts.removeEmptyStrings();
            if (lastPartOnly && parts.size() > 1) parts = juce::StringArray (parts[parts.size() - 1]);
            for (auto& p : parts) list.addIfNotAlreadyThere (p, true);
        }

        void prepare (Sound& s)   { prepareForSearch (s); }
    }

    void prepareForSearch (Sound& s)
    {
        {
            auto t = " " + (s.name + " " + s.instrument + " " + s.library + " " + s.origin + " " + kindName (s.kind) + " " + s.detail + " "
                            + s.styles.joinIntoString (" ") + " " + s.character.joinIntoString (" ") + " " + s.description)
                              .toLowerCase().replaceCharacters ("_-/()[]|,:;\t\r\n", "              ") + " ";
            while (t.contains ("  ")) t = t.replace ("  ", " ");

            // "Casette808", "808s": also as separate words, so "808" finds them
            juce::String split;
            for (int i = 0; i < t.length(); ++i)
            {
                const auto ch = t[i];
                if (i > 0 && t[i - 1] != ' ' && ch != ' '
                    && juce::CharacterFunctions::isDigit (ch) != juce::CharacterFunctions::isDigit (t[i - 1])
                    && juce::CharacterFunctions::isLetterOrDigit (ch) && juce::CharacterFunctions::isLetterOrDigit (t[i - 1]))
                    split << ' ';
                split << ch;
            }
            if (split != t) t << split.substring (1);
            s.searchText = t;
            s.stylesText = s.styles.joinIntoString (" ").toLowerCase();
        }
    }

    namespace
    {
        juce::String colOrNull (const juce::StringArray& cols, const char* name, const juce::String& prefix = {})
        {
            return cols.contains (name, true) ? prefix + name : juce::String ("NULL");
        }

        // Steinberg instruments whose presets MediaBay lists (anything else is an effect, unless MediaBay says otherwise)
        bool isSteinbergInstrument (const juce::String& plugin)
        {
            static const juce::StringArray names { "HALion Sonic SE", "HALion Sonic", "HALion", "Retrologue", "Padshop", "Padshop Pro",
                                                   "Groove Agent SE", "Groove Agent", "Drum Machine", "Prologue", "Spector", "Mystic",
                                                   "Embracer", "Sampler Track" };
            if (names.contains (plugin, true)) return true;
            return plugin.startsWithIgnoreCase ("HALion") || plugin.startsWithIgnoreCase ("Groove Agent");
        }

        // MediaBay names some presets after older versions: load them in the instrument you have today
        juce::String steinbergPluginToLoad (const juce::String& plugin)
        {
            if (plugin.equalsIgnoreCase ("HALion Sonic SE")) return "HALion Sonic";
            if (plugin.startsWithIgnoreCase ("HALion ") && juce::CharacterFunctions::isDigit (plugin[7])) return "HALion";
            return plugin;
        }
    }

    //==============================================================================
    std::vector<Sound> readMediaBay (const juce::File& file, juce::String& problem)
    {
        std::vector<Sound> out;
        ReadOnlyDatabase db (file);
        if (! db.isOpen()) { problem = db.error(); return out; }
        const auto cols = db.columns ("media");
        if (! cols.contains ("MediaType", true) || ! cols.contains ("FileName", true)) { problem = "not a MediaBay database"; return out; }

        juce::StringArray select;
        for (auto* c : { "url", "FileName", "PlugInName", "PlugInCategory", "MusicalCategory", "MusicalInstrument", "MusicalStyle", "MusicalSubStyle",
                         "MusicalCharacter", "MusicalProperties", "MusicalMoods", "MediaLibraryName", "MediaRating", "MediaComment" })
            select.add (colOrNull (cols, c));

        std::set<juce::String> seen;
        const bool ok = db.query ("SELECT " + select.joinIntoString (", ") + " FROM media WHERE MediaType = 'VstPreset'", [&] (sqlite3_stmt* st)
        {
            auto t = [st] (int c) { return ReadOnlyDatabase::text (st, c).trim(); };
            const auto url = t (0), plugin = t (2), pluginCategory = t (3), category = t (4), instrumentTag = t (5);
            const bool instrument = pluginCategory.containsIgnoreCase ("Instrument") || isSteinbergInstrument (plugin)
                                    || (plugin.isEmpty() && category.isNotEmpty() && ! pluginCategory.containsIgnoreCase ("Fx"));
            if (! instrument) return true;

            Sound s;
            s.name = t (1).upToLastOccurrenceOf (".vstpreset", false, true);
            if (s.name.isEmpty()) s.name = t (1);
            s.instrument = steinbergPluginToLoad (plugin);
            s.source = "MediaBay";
            s.yours = ! url.startsWithIgnoreCase ("vstsound:");
            s.library = t (11).isNotEmpty() ? t (11) : (s.yours ? juce::String ("Your presets") : juce::String());
            s.location = url;
            s.description = t (13);
            s.rating = juce::jlimit (0, 5, (int) ReadOnlyDatabase::integer (st, 12));
            addParts (s.styles, t (6));
            addParts (s.styles, t (7), true);
            for (int c : { 8, 9, 10 }) addParts (s.character, t (c));
            if (instrumentTag.isNotEmpty()) s.detail = instrumentTag.fromLastOccurrenceOf ("|", false, false);

            if (category.isNotEmpty())
                s.kind = kindFromMediaBay (category, instrumentTag);
            else
            {
                s.kind = s.instrument.containsIgnoreCase ("Groove Agent") || s.instrument.containsIgnoreCase ("Drum Machine")
                           ? Kind::drums : guessKind (s.name + " " + s.library);
                s.guessed = true;
            }

            const auto key = (s.name + "|" + s.instrument + "|" + s.library).toLowerCase();
            if (seen.insert (key).second)
            {
                prepare (s);
                out.push_back (std::move (s));
            }
            return true;
        });
        if (! ok) problem = db.error();
        return out;
    }

    //==============================================================================
    const juce::StringArray& retiredArturiaInstruments()
    {
        // Prophet V3 became Prophet-5 V and Prophet-VS V; Analog Lab Pro no longer lists it (seen on the owner's PC)
        static const juce::StringArray list { "Prophet V3" };
        return list;
    }

    const juce::StringArray& retiredArturiaPresets()
    {
        // presets Arturia's database still lists that Analog Lab V no longer offers (the owner looked, November 2026)
        static const juce::StringArray list { "Smoker" };
        return list;
    }

    std::vector<Sound> readArturia (const juce::File& file, juce::String& problem, int* leftOut)
    {
        std::vector<Sound> out;
        int left = 0;
        if (leftOut != nullptr) *leftOut = 0;
        ReadOnlyDatabase db (file);
        if (! db.isOpen()) { problem = db.error(); return out; }
        const auto cols = db.columns ("Preset_Id");
        if (! cols.contains ("name", true) || ! cols.contains ("instrument_key", true)) { problem = "not an Arturia preset database"; return out; }

        // characteristics, genres and styles of every preset
        std::map<juce::int64, std::pair<juce::StringArray, juce::StringArray>> tags;   // preset -> (genres, sound words)
        if (db.hasTable ("Preset_Characteristics") && db.hasTable ("Characteristics"))
        {
            const bool typed = db.hasTable ("Characteristics_Types");
            db.query (juce::String ("SELECT pc.preset_key, c.name, ") + (typed ? "ct.name" : "NULL")
                      + " FROM Preset_Characteristics pc JOIN Characteristics c ON pc.characteristic_key = c.key_id"
                      + (typed ? " LEFT JOIN Characteristics_Types ct ON c.type_key = ct.key_id" : ""), [&] (sqlite3_stmt* st)
            {
                auto& [genres, words] = tags[ReadOnlyDatabase::integer (st, 0)];
                const auto name = ReadOnlyDatabase::text (st, 1).trim();
                (ReadOnlyDatabase::text (st, 2) == "Genres" ? genres : words).addIfNotAlreadyThere (name, true);
                return true;
            });
        }

        // instruments Analog Lab no longer has, and layered presets that use one of them
        std::set<juce::int64> retiredIds, retiredLayers;
        juce::StringArray retiredKeys;
        db.query ("SELECT key_id, name FROM Instruments", [&] (sqlite3_stmt* st)
        {
            if (retiredArturiaInstruments().contains (ReadOnlyDatabase::text (st, 1).trim(), true))
            {
                retiredIds.insert (ReadOnlyDatabase::integer (st, 0));
                retiredKeys.add (juce::String (ReadOnlyDatabase::integer (st, 0)));
            }
            return true;
        });
        if (! retiredIds.empty() && db.hasTable ("Preset_Metadata"))
            db.query ("SELECT preset_key, value FROM Preset_Metadata WHERE name IN ('InstrumentPart1', 'InstrumentPart2', 'InstrumentPart3')",
                      [&] (sqlite3_stmt* st)
            {
                const auto v = ReadOnlyDatabase::text (st, 1).trim();
                if (retiredArturiaInstruments().contains (v, true) || retiredKeys.contains (v)) retiredLayers.insert (ReadOnlyDatabase::integer (st, 0));
                return true;
            });

        const bool types = db.hasTable ("Types_V2") && cols.contains ("type_v2", true);
        const bool subtypes = db.hasTable ("Subtypes_V2") && cols.contains ("subtype_v2", true);
        const bool packs = db.hasTable ("Packs") && cols.contains ("pack", true);
        juce::String sql = "SELECT p.key_id, p.name, i.name, "
                           + juce::String (types ? "t.name" : "NULL") + ", " + (subtypes ? "s.name" : "NULL") + ", "
                           + colOrNull (cols, "comment", "p.") + ", " + colOrNull (cols, "file_path", "p.") + ", " + colOrNull (cols, "rating", "p.") + ", "
                           + colOrNull (cols, "analoglab_factory", "p.") + ", " + (packs ? "k.name, k.factory" : "NULL, NULL") + ", "
                           + colOrNull (cols, "hide_in_browser", "p.") + ", p.instrument_key"
                           + " FROM Preset_Id p LEFT JOIN Instruments i ON p.instrument_key = i.key_id"
                           + (types ? " LEFT JOIN Types_V2 t ON p.type_v2 = t.key_id" : "")
                           + (subtypes ? " LEFT JOIN Subtypes_V2 s ON p.subtype_v2 = s.key_id" : "")
                           + (packs ? " LEFT JOIN Packs k ON p.pack = k.key_id" : "");

        const bool ok = db.query (sql, [&] (sqlite3_stmt* st)
        {
            auto t = [st] (int c) { return ReadOnlyDatabase::text (st, c).trim(); };
            if (ReadOnlyDatabase::integer (st, 11) == 1) return true;          // hidden in Analog Lab's own browser
            const auto type = t (3);
            if (type == "Template") return true;
            if (retiredIds.count (ReadOnlyDatabase::integer (st, 12)) > 0 || retiredLayers.count (ReadOnlyDatabase::integer (st, 0)) > 0)
            {
                ++left;                                                         // e.g. "75 Lead Soft" from the old Prophet V3
                return true;
            }

            Sound s;
            s.name = t (1);
            if (s.name.isEmpty()) return true;
            if (retiredArturiaPresets().contains (s.name, true)) { ++left; return true; }
            s.instrument = "Analog Lab V";
            s.source = "Arturia";
            s.origin = t (2);
            if (s.origin.startsWithIgnoreCase ("Analog Lab")) s.origin = "Analog Lab (layered)";
            s.detail = t (4);
            s.kind = kindFromArturia (type, s.detail);
            s.description = t (5);
            s.location = t (6);
            s.rating = juce::jlimit (0, 5, juce::roundToInt (ReadOnlyDatabase::real (st, 7)));
            s.library = t (9);
            const bool factoryPack = sqlite3_column_type (st, 10) == SQLITE_NULL || ReadOnlyDatabase::integer (st, 10) != 0;
            s.yours = ! factoryPack;
            s.owned = ReadOnlyDatabase::integer (st, 8) == 1 || s.yours;      // the rest show "buy" in Analog Lab
            if (auto it = tags.find (ReadOnlyDatabase::integer (st, 0)); it != tags.end())
            {
                s.styles = it->second.first;
                s.character = it->second.second;
            }
            prepare (s);
            out.push_back (std::move (s));
            return true;
        });
        if (! ok) problem = db.error();

        // A preset whose file is gone can't be loaded. Only trusted when most of the files can be found (if the paths
        // mean nothing on this computer, nothing is left out).
        {
            std::vector<int> missing;
            int checked = 0;
            for (size_t i = 0; i < out.size(); ++i)
            {
                if (! out[i].owned) continue;
                const auto path = out[i].location.replaceCharacter ('/', juce::File::getSeparatorChar());
                if (! juce::File::isAbsolutePath (path)) continue;
                ++checked;
                if (! juce::File (path).existsAsFile()) missing.push_back ((int) i);
            }
            if (checked > 0 && (int) missing.size() * 2 < checked)
            {
                for (auto it = missing.rbegin(); it != missing.rend(); ++it) out.erase (out.begin() + *it);
                left += (int) missing.size();
            }
        }
        if (leftOut != nullptr) *leftOut = left;
        return out;
    }

    //==============================================================================
    namespace
    {
        struct CymaticsInstrument { const char* key; const char* plugin; Kind kind; };

        // Cymatics instruments, and what they're mostly for (a preset's own name wins when it says)
        const CymaticsInstrument* cymaticsInstrument (const juce::String& folderName)
        {
            static const CymaticsInstrument list[] = {
                { "daydream", "Cymatics Daydream", Kind::keys },       { "drumpod", "Cymatics DrumPod", Kind::drums },
                { "galaxy", "Cymatics Galaxy", Kind::pad },            { "gameover", "Cymatics GAMEOVER", Kind::lead },
                { "invaderlite", "Cymatics Invader Lite", Kind::bass }, { "monarch", "Cymatics Monarch", Kind::synth },
                { "nightdrive", "Cymatics Night Drive", Kind::synth }, { "oceanpluck", "Cymatics Ocean Pluck", Kind::pluck },
                { "paradigmlite", "Cymatics Paradigm Lite", Kind::synth }, { "plinko", "Cymatics Plinko", Kind::pluck },
                { "quake", "Cymatics Quake", Kind::bass },             { "ripple", "Cymatics Ripple", Kind::synth },
                { "rwnd", "Cymatics RWND", Kind::keys },               { "sketch", "Cymatics Sketch", Kind::synth },
                { "pandora", "Cymatics Pandora", Kind::synth },        { "origin", "Cymatics Origin", Kind::synth },
                { "pluto", "Cymatics Pluto", Kind::synth },            { "memory", "Cymatics Memory", Kind::keys },
                { "halo", "Cymatics Halo", Kind::pad } };
            auto key = folderName.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789");
            if (key.startsWith ("cymatics")) key = key.substring (8);
            for (auto& c : list)
                if (key == c.key) return &c;
            return nullptr;
        }
    }

    std::vector<Sound> readCymatics (const juce::Array<juce::File>& roots)
    {
        std::vector<Sound> out;
        std::set<juce::String> seen;
        for (auto& root : roots)
            for (const auto& e : juce::RangedDirectoryIterator (root, false, "*", juce::File::findDirectories))
            {
                const auto* inst = cymaticsInstrument (e.getFile().getFileName());
                if (inst == nullptr) continue;
                const auto folder = e.getFile();
                walkFiles (folder, { 7, 20000, 30.0 }, [&] (const juce::File& f, int)
                {
                    const auto ext = f.getFileExtension().toLowerCase();
                    const auto rel = f.getRelativePathFrom (folder).replaceCharacter ('\\', '/');
                    const bool inPresets = rel.containsIgnoreCase ("preset");
                    if (! (ext == ".cymatics" || ext == ".preset" || (ext == ".json" && inPresets))) return true;
                    const auto name = f.getFileNameWithoutExtension();
                    if (name.startsWithIgnoreCase ("init") || name.equalsIgnoreCase ("db") || name.equalsIgnoreCase ("manifest")
                        || name.equalsIgnoreCase ("favorites") || looksPrivate (name))
                        return true;

                    // the folders between "Presets" (or the plug-in's folder) and the file name the library
                    auto parts = juce::StringArray::fromTokens (rel, "/", "");
                    parts.remove (parts.size() - 1);
                    juce::StringArray lib;
                    for (auto& p : parts)
                        if (! p.equalsIgnoreCase ("Presets") && ! p.equalsIgnoreCase ("Resources") && ! p.equalsIgnoreCase ("Expansions")
                            && ! p.equalsIgnoreCase ("UserPresets"))
                            lib.add (p);

                    Sound s;
                    s.name = name;
                    s.instrument = inst->plugin;
                    s.source = "Cymatics";
                    s.library = lib.joinIntoString (" / ");
                    s.yours = rel.containsIgnoreCase ("user");
                    s.location = f.getFullPathName();
                    s.kind = guessKind (name + " " + lib.joinIntoString (" "));
                    if (s.kind == Kind::other) s.kind = inst->kind;
                    s.guessed = true;
                    if (seen.insert ((s.instrument + "|" + s.library + "|" + s.name).toLowerCase()).second)
                    {
                        prepare (s);
                        out.push_back (std::move (s));
                    }
                    return true;
                });
            }
        return out;
    }

    //==============================================================================
    std::vector<Sound> readPresetFiles (const SystemLocations& loc)
    {
        std::vector<Sound> out;
        for (auto& root : loc.presetRoots)
            walkFiles (root, { 8, 60000, 60.0 }, [&] (const juce::File& f, int)
            {
                if (! f.hasFileExtension (".vstpreset") || looksPrivate (f.getFileName())) return true;
                const auto info = readVstPreset (f);
                if (! info.valid) return true;
                auto parts = juce::StringArray::fromTokens (f.getRelativePathFrom (root).replaceCharacter ('\\', '/'), "/", "");
                Sound s;
                s.name = f.getFileNameWithoutExtension();
                s.instrument = info.attributes["PlugInName"].isNotEmpty() ? info.attributes["PlugInName"]
                                                                         : (parts.size() >= 3 ? parts[1] : f.getParentDirectory().getFileName());
                s.instrument = steinbergPluginToLoad (s.instrument);
                if (info.attributes["PlugInCategory"].containsIgnoreCase ("Fx")) return true;
                s.source = "Preset file";
                s.yours = true;
                s.library = "Your presets";
                s.location = f.getFullPathName();
                addParts (s.styles, info.attributes["MusicalStyle"]);
                addParts (s.character, info.attributes["MusicalCharacter"]);
                if (auto c = info.attributes["MusicalCategory"]; c.isNotEmpty())
                    s.kind = kindFromMediaBay (c, info.attributes["MusicalInstrument"]);
                else
                {
                    s.kind = s.instrument.containsIgnoreCase ("Groove Agent") ? Kind::drums : guessKind (s.name);
                    s.guessed = true;
                }
                prepare (s);
                out.push_back (std::move (s));
                return true;
            });
        return out;
    }

    //==============================================================================
    Catalog buildCatalog (const SystemLocations& loc, const std::atomic<bool>& cancel)
    {
        Catalog c;
        c.built = juce::Time::getCurrentTime();
        const auto t0 = juce::Time::getMillisecondCounterHiRes();

        auto add = [&] (const juce::String& name, const juce::File& file, std::vector<Sound> sounds, const juce::String& problem, int leftOut = 0)
        {
            CatalogSource src { name, file, (int) sounds.size(), 0, leftOut, problem };
            for (auto& s : sounds) if (! s.owned) ++src.notOwned;
            c.sources.push_back (src);
            for (auto& s : sounds) c.sounds.push_back (std::move (s));
        };

        if (loc.mediaBayDb.existsAsFile())
        {
            juce::String problem;
            auto s = readMediaBay (loc.mediaBayDb, problem);
            add ("Steinberg (MediaBay)", loc.mediaBayDb, std::move (s), problem);
        }
        else
            add ("Your preset files", {}, readPresetFiles (loc), {});    // without MediaBay, read the .vstpreset files themselves
        if (cancel.load()) return c;

        if (loc.arturiaDb.existsAsFile())
        {
            juce::String problem;
            int leftOut = 0;
            auto s = readArturia (loc.arturiaDb, problem, &leftOut);
            add ("Arturia (Analog Lab)", loc.arturiaDb, std::move (s), problem, leftOut);
        }
        if (cancel.load()) return c;

        if (! loc.cymaticsRoots.isEmpty())
            add ("Cymatics", loc.cymaticsRoots.getFirst(), readCymatics (loc.cymaticsRoots), {});
        if (cancel.load()) return c;

        // the kits you saved in Groove Grabber (built from your own samples)
        if (loc.grooveGrabberKits.isDirectory())
            add ("Groove Grabber kits", loc.grooveGrabberKits, readGrooveGrabberKits (loc.grooveGrabberKits), {});
        if (cancel.load()) return c;

        // instruments whose presets Song Savant can't read: suggest the plug-in itself
        {
            auto plugins = installedPlugins (loc, &cancel);
            auto level = pluginLevelSounds (plugins, c.sounds);
            for (auto& s : level) prepare (s);
            add ("Other instruments (pick a sound inside)", {}, std::move (level), {});
        }

        std::map<Kind, int> order;
        for (size_t i = 0; i < allKinds().size(); ++i) order[allKinds()[i]] = (int) i;
        std::stable_sort (c.sounds.begin(), c.sounds.end(), [&] (const Sound& a, const Sound& b)
        {
            if (a.kind != b.kind) return order[a.kind] < order[b.kind];
            return a.name.compareNatural (b.name) < 0;
        });
        c.seconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        return c;
    }

    juce::StringArray Catalog::instruments() const
    {
        juce::StringArray out;
        for (auto& s : sounds)
            if (s.instrument.isNotEmpty()) out.addIfNotAlreadyThere (s.instrument);
        out.sortNatural();
        return out;
    }

    int Catalog::count (Kind k, bool ownedOnly) const
    {
        int n = 0;
        for (auto& s : sounds)
            if (s.kind == k && (s.owned || ! ownedOnly)) ++n;
        return n;
    }

    std::vector<int> findSounds (const Catalog& c, const SoundFilter& f, int limit)
    {
        std::vector<int> out;
        auto words = juce::StringArray::fromTokens (f.text.toLowerCase(), " ", "\"");
        words.removeEmptyStrings();
        for (int i = 0; i < (int) c.sounds.size() && (int) out.size() < limit; ++i)
        {
            const auto& s = c.sounds[(size_t) i];
            if (f.ownedOnly && ! s.owned) continue;
            if (f.yoursOnly && ! s.yours) continue;
            if (! f.kinds.empty() && f.kinds.count (s.kind) == 0) continue;
            if (f.instrument.isNotEmpty() && ! s.instrument.equalsIgnoreCase (f.instrument)) continue;
            bool all = true;
            for (auto& w : words)
                if (! s.searchText.contains (w)) { all = false; break; }
            if (all) out.push_back (i);
        }
        return out;
    }

    namespace
    {
        // "SSD Sampler 5 (SSDSampler5 in your plug-in list)": the friendly name, and the one the DAW shows
        juce::String pluginLabel (const Sound& s)
        {
            if (s.instrument.isEmpty() || s.instrument == s.name) return s.name;
            return s.name + " (" + s.instrument + " in your plug-in list)";
        }
    }

    juce::String howToLoad (const Sound& s)
    {
        const auto q = "\"" + s.name + "\"";
        juce::String text;
        if (s.source == "MediaBay")
            text = "In Cubase: open MediaBay (F5), click All Media at the top of the File Browser on the left, search for " + q
                 + " and drag it into your project. Or load " + (s.instrument.isNotEmpty() ? s.instrument : juce::String ("the instrument"))
                 + " and type the name in its preset browser.";
        else if (s.source == "Arturia")
            text = "MediaBay doesn't list Analog Lab sounds. Load Analog Lab V on an instrument track and search for " + q + " in its own sound browser"
                 + (s.origin.isNotEmpty() && ! s.origin.startsWith ("Analog Lab") ? " (from " + s.origin + ")." : juce::String ("."));
        else if (s.source == "Plug-in")
            text = "Load " + pluginLabel (s) + " on an instrument track and " + s.detail.toLowerCase().replace (" in the plug-in", "")
                 + " that fits. (Song Savant can't see this plug-in's presets.)";
        else if (s.source == "Cymatics")
            text = "MediaBay doesn't list Cymatics sounds. Load " + s.instrument + " on an instrument track and choose " + q + " from its preset list"
                 + (s.library.isNotEmpty() ? " (" + s.library + ")." : juce::String ("."));
        else if (isGrooveGrabberKit (s))
            text = "Your kit from Groove Grabber. Load Groove Grabber on an instrument track, open MY KITS and click LOAD on " + q
                 + ": it plays the kit itself, and the starter drum parts already use its pads. Or EXPORT it there for Groove Agent.";
        else
            text = "Load " + s.instrument + " and open its preset " + q + " (in your VST3 Presets folder).";
        if (! s.owned) text = "Not owned: Analog Lab shows a buy button for this one. " + text;
        if (s.kind == Kind::drums && s.source != "Plug-in" && ! s.instrument.containsIgnoreCase ("Groove Agent") && ! isGrooveGrabberKit (s))
            text << " No pad names in Cubase's drum editor? Save the kit as a preset and map it on the DRUM MAPS page.";
        return text;
    }

    juce::String nameToCopy (const Sound& s)   { return s.source == "Plug-in" && s.instrument.isNotEmpty() ? s.instrument : s.name; }

    juce::String whereToPaste (const Sound& s)
    {
        if (s.source == "MediaBay") return "Paste it into MediaBay's search (with All Media selected).";
        if (s.source == "Arturia")  return "Paste it into Analog Lab's search box. (MediaBay doesn't list Analog Lab sounds.)";
        if (s.source == "Cymatics") return "Find it in " + s.instrument + "'s preset list. (MediaBay doesn't list Cymatics sounds.)";
        if (isGrooveGrabberKit (s)) return "Paste it into the search on Groove Grabber's MY KITS page.";
        if (s.source == "Plug-in")  return "Find it in your DAW's instrument list, load it on an instrument track and pick a sound in it.";
        return "Find it in " + s.instrument + "'s preset browser.";
    }
}
