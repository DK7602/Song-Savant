#include "FakeComputer.h"
#include "core/VstPreset.h"
#include <sqlite3.h>

namespace sstest
{
    namespace
    {
        void write (const juce::File& f, const juce::String& text)
        {
            f.getParentDirectory().createDirectory();
            f.replaceWithText (text);
        }

        void writeData (const juce::File& f, const juce::MemoryBlock& mb)
        {
            f.getParentDirectory().createDirectory();
            f.replaceWithData (mb.getData(), mb.getSize());
        }

        juce::String moduleInfo (const juce::String& name, const juce::String& vendor, const juce::String& subs, bool json5 = false)
        {
            juce::StringArray cats;
            for (auto& s : juce::StringArray::fromTokens (subs, "|", "")) cats.add ("\"" + s + "\"");
            juce::String j;
            j << "{\n";
            if (json5) j << "  // written by the VST3 SDK's moduleinfotool\n";
            j << "  \"Name\": \"" << name << "\",\n  \"Version\": \"1.2.3\",\n"
              << "  \"Factory Info\": { \"Vendor\": \"" << vendor << "\", \"URL\": \"\", \"E-Mail\": \"\" },\n"
              << "  \"Classes\": [\n"
              << "    { \"CID\": \"0123456789ABCDEF0123456789ABCDEF\", \"Category\": \"Audio Module Class\", \"Name\": \"" << name << "\",\n"
              << "      \"Vendor\": \"" << vendor << "\", \"Sub Categories\": [" << cats.joinIntoString (", ") << "]" << (json5 ? "," : "") << " },\n"
              << "    { \"CID\": \"FEDCBA9876543210FEDCBA9876543210\", \"Category\": \"Component Controller Class\", \"Name\": \"" << name << "Controller\" }"
              << (json5 ? ",\n" : "\n")
              << "  ]\n}\n";
            return j;
        }

        // json5 plug-ins also use the older place for moduleinfo.json (Contents/ rather than Contents/Resources/)
        void plugin (const juce::File& dir, const juce::String& file, const juce::String& name, const juce::String& vendor, const juce::String& subs, bool json5 = false)
        {
            write (dir.getChildFile (file + (json5 ? ".vst3/Contents/moduleinfo.json" : ".vst3/Contents/Resources/moduleinfo.json")),
                   moduleInfo (name, vendor, subs, json5));
            write (dir.getChildFile (file + ".vst3/Contents/x86_64-win/" + file + ".vst3"), "binary");
        }

        juce::StringPairArray tags (const juce::String& plugin, const juce::String& cat, const juce::String& inst,
                                    const juce::String& style, const juce::String& character)
        {
            juce::StringPairArray a;
            a.set ("MediaType", "VstPreset");
            a.set ("PlugInName", plugin);
            a.set ("PlugInCategory", "Instrument|Synth");
            a.set ("MusicalCategory", cat);
            a.set ("MusicalInstrument", inst);
            a.set ("MusicalStyle", style);
            a.set ("MusicalCharacter", character);
            return a;
        }

        juce::MemoryBlock comp (int bytes)
        {
            juce::MemoryBlock mb ((size_t) bytes);
            for (int i = 0; i < bytes; ++i) static_cast<char*> (mb.getData())[i] = (char) (i * 7 + 3);
            return mb;
        }

        void makeDb (const juce::File& f, const juce::StringArray& statements)
        {
            f.getParentDirectory().createDirectory();
            f.deleteFile();
            sqlite3* db = nullptr;
            sqlite3_open (f.getFullPathName().toRawUTF8(), &db);
            for (auto& s : statements) sqlite3_exec (db, s.toRawUTF8(), nullptr, nullptr, nullptr);
            sqlite3_close (db);
        }
    }

    FakeComputer makeFakeComputer (const juce::File& root)
    {
        FakeComputer fc;
        fc.root = root;
        root.deleteRecursively();

        const auto pf = root.getChildFile ("Program Files");
        const auto vst3 = pf.getChildFile ("Common Files/VST3");
        const auto home = root.getChildFile ("Users/me");
        const auto docs = home.getChildFile ("Documents");
        const auto roaming = home.getChildFile ("AppData/Roaming");
        const auto programData = root.getChildFile ("ProgramData");

        // plug-ins
        plugin (vst3.getChildFile ("Steinberg"), "HALion Sonic", "HALion Sonic", "Steinberg Media Technologies", "Instrument|Synth", true);
        write (vst3.getChildFile ("Analog Lab V.vst3"), "single-file plug-in: its type comes from Cubase's list");
        plugin (vst3, "Groove Grabber", "Groove Grabber", "Grabber Audio", "Instrument|Drum|Sampler");
        plugin (vst3, "Pro-Q 3", "Pro-Q 3", "FabFilter", "Fx|EQ");
        write (vst3.getChildFile ("Cymatics/Plinko.vst3"), "single-file plug-in");
        // instruments whose presets Song Savant can't read: suggested as plug-ins
        write (vst3.getChildFile ("SSDSampler5.vst3"), "single-file plug-in, known by name");
        plugin (vst3, "FM8", "FM8", "Native Instruments", "Instrument|Synth");
        plugin (vst3, "Mystery Keys", "Mystery Keys", "Small Maker", "Instrument|Piano");
        plugin (vst3, "SPL Vitalizer", "SPL Vitalizer MK2-T", "Plugin Alliance", "Fx|Mastering");   // an effect, not "Vital"
        write (vst3.getChildFile ("Odd Synth.vst3"), "single-file plug-in nothing knows about");
        plugin (pf.getChildFile ("Steinberg/Cubase 14/VST3"), "Retrologue", "Retrologue", "Steinberg Media Technologies", "Instrument|Synth");
        write (pf.getChildFile ("Steinberg/Cubase 14/Cubase14.exe"), "exe");

        // VST3 preset files
        fc.halionPreset = docs.getChildFile ("VST3 Presets/Steinberg Media Technologies/HALion Sonic/Keys/Warm Grand.vstpreset");
        writeData (fc.halionPreset, ss::makeVstPreset ("56535448534E636D6F6E6963000000000", comp (300),
                                                       tags ("HALion Sonic", "Piano", "Piano|A. Piano", "Pop/Rock|Pop", "Acoustic|Warm")));
        fc.untaggedPreset = roaming.getChildFile ("VST3 Presets/Steinberg Media Technologies/Retrologue/My Bass.vstpreset");
        writeData (fc.untaggedPreset, ss::makeVstPreset ("5653545265746F6C6F6775650000000000", comp (120), {}));
        write (docs.getChildFile ("VST3 Presets/Steinberg Media Technologies/HALion Sonic/broken.vstpreset"), "not a preset");

        // a VST Sound container: a header, some padding, preset names (ASCII and UTF-16) and an embedded preset
        {
            juce::MemoryOutputStream c;
            c.write ("VSTSOUND\x01\x00\x00\x00", 12);
            for (int i = 0; i < 2000; ++i) c.writeByte ((char) (i % 251));
            c.writeString ("/.AppData/Steinberg/HALion Sonic/Factory Presets/Pianos/Bright Grand.vstpreset");
            c.writeShort (0x0002);   // UTF-16 names are stored after a length or a separator, not right after text
            const juce::String u16name ("Airy Pad.vstpreset");
            for (auto ch : u16name) { c.writeByte ((char) ch); c.writeByte (0); }
            for (int i = 0; i < 500; ++i) c.writeByte ((char) (i % 97));
            auto embedded = ss::makeVstPreset ("56535448534E636D6F6E6963000000000", comp (800),
                                               tags ("HALion Sonic", "Synth Pad", "Synth Pad|Analog", "Electronica|Ambient", "Synthetic|Wide|Soft"));
            c.write (embedded.getData(), embedded.getSize());
            for (int i = 0; i < 300; ++i) c.writeByte (0);
            fc.vstSound = programData.getChildFile ("Steinberg/Content/HALion Sonic/HALion Sonic Factory.vstsound");
            writeData (fc.vstSound, c.getMemoryBlock());
        }

        // Arturia-style preset database. Preset files are on disk, except one (it can't be loaded, so it's left out)
        fc.arturiaDb = programData.getChildFile ("Arturia/Presets/db.db3");
        auto arturiaFile = [&] (const juce::String& rel, bool exists)
        {
            const auto f = programData.getChildFile ("Arturia/Presets/" + rel);
            if (exists) write (f, "Arturia preset");
            return "'" + f.getFullPathName().replaceCharacter ('\\', '/') + "'";
        };
        const auto userPresetPath = arturiaFile ("Analog Lab/User/my sound.labx", true);
        makeDb (fc.arturiaDb, {
            "CREATE TABLE Preset_Id (key_id INTEGER PRIMARY KEY, name TEXT, type INTEGER, type_v2 INTEGER, subtype_v2 INTEGER, instrument_key INTEGER, "
                "sound_designer INTEGER, rating REAL, pack INTEGER, comment TEXT, file_path TEXT, favorite INTEGER, hide_in_browser INTEGER, "
                "analoglab_factory INTEGER, data BLOB)",
            "INSERT INTO Preset_Id VALUES (1, 'Classic Jun Keys', 1, 3, 7, 60, 1, 0, 1, 'Warm 80s keys', " + arturiaFile ("Analog Lab V/Factory/Classic Jun Keys", true) + ", 0, 0, 1, x'00112233')",
            "INSERT INTO Preset_Id VALUES (2, 'Warm Analog Bass', 2, 1, 2, 60, 2, 0, 1, 'Round low end', " + arturiaFile ("Analog Lab V/Factory/Warm Analog Bass", true) + ", 1, 0, 1, NULL)",
            "INSERT INTO Preset_Id VALUES (3, 'Heaven Pad', 3, 4, 9, 60, 1, 0, 1, 'Wide worship pad', " + arturiaFile ("Analog Lab V/Factory/Heaven Pad", true) + ", 0, 0, 1, NULL)",
            "INSERT INTO Preset_Id VALUES (4, 'Jupiter Brass', 3, 1, 2, 61, 1, 0, 1, 'For sale', 'C:/x', 0, 0, 0, NULL)",
            "INSERT INTO Preset_Id VALUES (5, 'My Analog Lab Bass', 2, 1, 2, 60, 1, 0, 2, '', " + userPresetPath + ", 0, 0, 0, NULL)",
            "INSERT INTO Preset_Id VALUES (6, 'Hidden One', 2, 1, 2, 60, 1, 0, 1, '', 'C:/y', 0, 1, 1, NULL)",
            // not playable in today's Analog Lab: the old Prophet V3, a layered preset using it, and a preset whose file is gone
            "INSERT INTO Preset_Id VALUES (7, '75 Lead Soft', 2, 2, 2, 6, 1, 0, 1, '', " + arturiaFile ("Prophet V3/Factory/75 Lead Soft", true) + ", 0, 0, 1, NULL)",
            "INSERT INTO Preset_Id VALUES (8, 'Lost File Pad', 3, 4, 9, 60, 1, 0, 1, '', " + arturiaFile ("Analog Lab V/Factory/Lost File Pad", false) + ", 0, 0, 1, NULL)",
            "INSERT INTO Preset_Id VALUES (9, 'Prophet Stack', 3, 4, 9, 60, 1, 0, 1, '', " + arturiaFile ("Analog Lab V/Factory/Prophet Stack", true) + ", 0, 0, 1, NULL)",
            "CREATE TABLE Instruments (key_id INTEGER, name TEXT, display_name TEXT, version INTEGER, category_id INTEGER)",
            "INSERT INTO Instruments VALUES (60, 'Analog Lab V', 'Analog Lab', 5, 8), (61, 'Jup-8 V4', 'Jup-8', 4, 1), (6, 'Prophet V3', 'Prophet', 3, 1)",
            "CREATE TABLE Types_V2 (key_id INTEGER, name TEXT)",
            "INSERT INTO Types_V2 VALUES (1, 'Bass'), (2, 'Lead'), (3, 'Keys'), (4, 'Pad')",
            "CREATE TABLE Subtypes_V2 (key_id INTEGER, name TEXT, type_key INTEGER)",
            "INSERT INTO Subtypes_V2 VALUES (2, 'Analog Bass', 1), (7, 'Electric Piano', 3), (9, 'Warm Pad', 4)",
            "CREATE TABLE Characteristics_Types (key_id INTEGER, name TEXT)",
            "INSERT INTO Characteristics_Types VALUES (1, 'Characteristics'), (2, 'Genres'), (3, 'Styles')",
            "CREATE TABLE Characteristics (key_id INTEGER, name TEXT, type_key INTEGER)",
            "INSERT INTO Characteristics VALUES (1, 'Warm', 1), (2, 'Bright', 1), (3, 'Pop', 2), (4, 'Worship', 2)",
            "CREATE TABLE Preset_Characteristics (key_id INTEGER, preset_key INTEGER, characteristic_key INTEGER)",
            "INSERT INTO Preset_Characteristics VALUES (1, 1, 1), (2, 1, 3), (3, 2, 1), (4, 3, 1), (5, 3, 4)",
            "CREATE TABLE Genres (key_id INTEGER, name TEXT)",
            "INSERT INTO Genres VALUES (1, 'Pop'), (2, 'Worship')",
            "CREATE TABLE Packs (key_id INTEGER, name TEXT, factory INTEGER)",
            "INSERT INTO Packs VALUES (1, 'Analog Lab Factory', 1), (2, 'User', 0)",
            "CREATE TABLE Purchasable_Packs (product_id INTEGER, instrument_id INTEGER, name TEXT, owned INTEGER)",
            "INSERT INTO Purchasable_Packs VALUES (10, 60, 'Worship Keys', 1), (11, 60, 'Trap Leads', 0)",
            "CREATE TABLE Preset_Metadata (key_id INTEGER, preset_key INTEGER, name TEXT, value TEXT)",
            "INSERT INTO Preset_Metadata VALUES (1, 1, 'Characteristics', 'Characteristics,Warm;Genres,Pop'), "
                "(2, 9, 'InstrumentPart1', 'Prophet V3'), (3, 9, 'InstrumentPart2', 'Analog Lab V'), (4, 1, 'ALVersionFirst', '5.0.0')" });

        // MediaBay's database (the real place), a decoy database in the same maker's folder, and Cubase's plug-in list
        const auto localAppData = home.getChildFile ("AppData/Local");
        fc.mediaBayDb = localAppData.getChildFile ("Steinberg/Steinberg MediaBay Server/mediabay3.db");
        {
            juce::StringArray sql { "CREATE TABLE media (id INTEGER PRIMARY KEY, depth INTEGER, url TEXT, volume_id INTEGER, MediaType TEXT, FileName TEXT, "
                                    "PlugInName TEXT, MusicalCategory TEXT, MusicalInstrument TEXT, MusicalStyle TEXT, MusicalSubStyle TEXT, "
                                    "MusicalCharacter TEXT, MusicalMoods TEXT, MusicalProperties TEXT, MediaLibraryName TEXT, MediaRating INTEGER, MediaComment TEXT)",
                                    "CREATE TABLE volumes (id INTEGER, serial INTEGER, root TEXT, name TEXT, flags INTEGER)",
                                    "INSERT INTO volumes VALUES (1, 0, 'vstsound://F6ADB7B8D7C04F50A3968D24D463240F', 'HALion Sonic Library', 0)",
                                    "INSERT INTO media (url, MediaType, FileName) VALUES ('vstsound://F6AD/Audio Files/01 bass.wav', 'Audio', '01 bass.wav')" };
            for (int i = 0; i < 50; ++i)
                sql.add ("INSERT INTO media (url, MediaType, FileName, PlugInName, MusicalCategory, MusicalInstrument, MusicalStyle, MusicalSubStyle, MusicalCharacter, MediaLibraryName) "
                         "VALUES ('vstsound://F6AD/Presets/Keys " + juce::String (i) + ".vstpreset', 'VstPreset', 'Keys " + juce::String (i)
                         + ".vstpreset', 'HALion Sonic SE', 'Keyboard', 'Keyboard|E. Piano', 'Pop', 'Pop|Pop/Rock', 'Warm|Clean', 'HALion Sonic Library')");
            sql.add ("INSERT INTO media (url, MediaType, FileName, PlugInName, MusicalCategory) VALUES ('vstsound://F6AD/FX/Room.vstpreset', 'VstPreset', 'Big Room.vstpreset', 'REVelation', 'Musical FX')");
            sql.add ("INSERT INTO media (url, MediaType, FileName, PlugInName) VALUES ('file:///C:/Users/me/Documents/VST3 Presets/x.vstpreset', 'VstPreset', 'My Keys', 'HALion Sonic')");
            makeDb (fc.mediaBayDb, sql);
        }
        makeDb (roaming.getChildFile ("Steinberg/EBWebView/Default/declarative_performance_observer.db"),
                { "CREATE TABLE meta (key TEXT, value TEXT)", "INSERT INTO meta VALUES ('version', '2')" });
        write (roaming.getChildFile ("Steinberg/Cubase 14_64/VstPlugInfoV4.xml"),
               "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<VstPlugInfoV4>\n <list name=\"Plugins\" type=\"obj\">\n"
               "  <obj class=\"PlugInfo\">\n   <string name=\"Name\" value=\"Analog Lab V\"/>\n   <string name=\"Vendor\" value=\"Arturia\"/>\n"
               "   <string name=\"SubCategories\" value=\"Instrument|Synth\"/>\n  </obj>\n </list>\n</VstPlugInfoV4>\n");
        write (roaming.getChildFile ("Steinberg/Cubase 14_64/Defaults.xml"), "<Defaults/>");

        // Cymatics
        write (docs.getChildFile ("Cymatics/Plinko/Presets/Bell Pluck.cyp"), "preset");
        write (docs.getChildFile ("Cymatics/Plinko/Presets/Glass Keys.cyp"), "preset");
        write (docs.getChildFile ("Cymatics/Plinko/Samples/bell.wav"), "RIFF");
        write (docs.getChildFile ("Cymatics/Hub/license.dat"), "never listed");
        write (programData.getChildFile ("Cymatics/Cymatics Plinko/Presets/Factory/Glass Bell.cymatics"), "{}");
        write (programData.getChildFile ("Cymatics/Cymatics Plinko/Presets/Init.cymatics"), "{}");
        write (programData.getChildFile ("Cymatics/Daydream/Resources/Presets/01 Pack/Warm Tape Keys.json"), "{}");
        write (programData.getChildFile ("Cymatics/Space/Presets/Factory/Hall.cym"), "an effect, not an instrument");
        write (roaming.getChildFile ("Cymatics/Cymatics Pandora/Expansions/01 Essentials/UserPresets/BASS Apocalypse.preset"), "x");

        // the top of another drive: an exact maker folder, a near miss, and a private folder that must never be touched
        fc.driveD = root.getChildFile ("Drive D");
        write (fc.driveD.getChildFile ("Steinberg/Content/Extra.vstsound"), "Steinberg Resource File\r\n");
        write (fc.driveD.getChildFile ("Steinberg Backup/old.txt"), "x");
        write (fc.driveD.getChildFile ("Key Compass App Secret Key/signing key.txt"), "never read");

        // Key Compass, Groove Grabber, Sample Snagger
        write (roaming.getChildFile ("KeyCompass/Songs/Back Roads.kcsong"),
               "<KeyCompassSong name=\"Back Roads\" modified=\"1730000000000\"><SongMap bpm=\"92\" keyOverride=\"21\">"
               "<Section name=\"Verse\" chords=\"Am F C G\"/><Section name=\"Chorus\" chords=\"F G Am\"/></SongMap></KeyCompassSong>");
        write (docs.getChildFile ("Groove Grabber/Kits/Country/Nashville Knock.ggkit"),
               "{\"format\":\"groove-grabber-kit\",\"version\":1,\"name\":\"Nashville Knock\",\"genre\":\"country\",\"key\":9,\"bpm\":92}");
        write (docs.getChildFile ("Sample Snagger/Samples/vocal chop.wav"), "RIFF");

        root.getChildFile ("Users/Public/Documents").createDirectory();
        pf.getChildFile ("Common Files").createDirectory();

        fc.loc = ss::SystemLocations::windowsLayoutUnder (root);
        fc.loc.driveRoots.add (fc.driveD);
        return fc;
    }
}
