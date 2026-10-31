#include "Catalog.h"
#include "Database.h"
#include "FileWalk.h"
#include "VstPreset.h"
#include <sqlite3.h>
#include <algorithm>
#include <map>

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

        void prepare (Sound& s)
        {
            s.searchText = (s.name + " " + s.instrument + " " + s.library + " " + s.origin + " " + kindName (s.kind) + " " + s.detail + " "
                            + s.styles.joinIntoString (" ") + " " + s.character.joinIntoString (" ") + " " + s.description).toLowerCase();
        }

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
    std::vector<Sound> readArturia (const juce::File& file, juce::String& problem)
    {
        std::vector<Sound> out;
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

        const bool types = db.hasTable ("Types_V2") && cols.contains ("type_v2", true);
        const bool subtypes = db.hasTable ("Subtypes_V2") && cols.contains ("subtype_v2", true);
        const bool packs = db.hasTable ("Packs") && cols.contains ("pack", true);
        juce::String sql = "SELECT p.key_id, p.name, i.name, "
                           + juce::String (types ? "t.name" : "NULL") + ", " + (subtypes ? "s.name" : "NULL") + ", "
                           + colOrNull (cols, "comment", "p.") + ", " + colOrNull (cols, "file_path", "p.") + ", " + colOrNull (cols, "rating", "p.") + ", "
                           + colOrNull (cols, "analoglab_factory", "p.") + ", " + (packs ? "k.name, k.factory" : "NULL, NULL") + ", "
                           + colOrNull (cols, "hide_in_browser", "p.")
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

            Sound s;
            s.name = t (1);
            if (s.name.isEmpty()) return true;
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

        auto add = [&] (const juce::String& name, const juce::File& file, std::vector<Sound> sounds, const juce::String& problem)
        {
            CatalogSource src { name, file, (int) sounds.size(), 0, problem };
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
            auto s = readArturia (loc.arturiaDb, problem);
            add ("Arturia (Analog Lab)", loc.arturiaDb, std::move (s), problem);
        }
        if (cancel.load()) return c;

        if (! loc.cymaticsRoots.isEmpty())
            add ("Cymatics", loc.cymaticsRoots.getFirst(), readCymatics (loc.cymaticsRoots), {});

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
                 + (s.origin.isNotEmpty() ? " (from " + s.origin + ")." : juce::String ("."));
        else if (s.source == "Cymatics")
            text = "MediaBay doesn't list Cymatics sounds. Load " + s.instrument + " on an instrument track and choose " + q + " from its preset list"
                 + (s.library.isNotEmpty() ? " (" + s.library + ")." : juce::String ("."));
        else
            text = "Load " + s.instrument + " and open its preset " + q + " (in your VST3 Presets folder).";
        if (! s.owned) text = "Not owned: Analog Lab shows a buy button for this one. " + text;
        return text;
    }

    juce::String whereToPaste (const Sound& s)
    {
        if (s.source == "MediaBay") return "Paste it into MediaBay's search (with All Media selected).";
        if (s.source == "Arturia")  return "Paste it into Analog Lab's search box. (MediaBay doesn't list Analog Lab sounds.)";
        if (s.source == "Cymatics") return "Find it in " + s.instrument + "'s preset list. (MediaBay doesn't list Cymatics sounds.)";
        return "Find it in " + s.instrument + "'s preset browser.";
    }
}
