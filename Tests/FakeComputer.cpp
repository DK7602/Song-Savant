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

        void plugin (const juce::File& dir, const juce::String& file, const juce::String& name, const juce::String& vendor, const juce::String& subs, bool json5 = false)
        {
            write (dir.getChildFile (file + ".vst3/Contents/moduleinfo.json"), moduleInfo (name, vendor, subs, json5));
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
        plugin (vst3, "Analog Lab V", "Analog Lab V", "Arturia", "Instrument|Synth");
        plugin (vst3, "Groove Grabber", "Groove Grabber", "Grabber Audio", "Instrument|Drum|Sampler");
        plugin (vst3, "Pro-Q 3", "Pro-Q 3", "FabFilter", "Fx|EQ");
        write (vst3.getChildFile ("Cymatics/Plinko.vst3"), "single-file plug-in");
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

        // Arturia-style preset database
        fc.arturiaDb = programData.getChildFile ("Arturia/Presets/db.db3");
        makeDb (fc.arturiaDb, {
            "CREATE TABLE Preset_Id (key_id INTEGER PRIMARY KEY, name TEXT, instrument_key INTEGER, type TEXT, sound_designer TEXT, data BLOB)",
            "INSERT INTO Preset_Id VALUES (1, 'Classic Jun Keys', 3, 'Keys', 'Arturia', x'00112233')",
            "INSERT INTO Preset_Id VALUES (2, 'Warm Analog Bass', 4, 'Bass', 'Someone', NULL)",
            "INSERT INTO Preset_Id VALUES (3, 'Heaven Pad', 5, 'Pad', 'Arturia', NULL)",
            "CREATE TABLE Characteristics (key_id INTEGER, name TEXT)",
            "INSERT INTO Characteristics VALUES (1, 'Warm'), (2, 'Bright')",
            "CREATE TABLE Version (v INTEGER)",
            "INSERT INTO Version VALUES (7)" });
        write (programData.getChildFile ("Arturia/Presets/Analog Lab/User/my sound.labx"), "user preset");

        // Cubase settings with a MediaBay-style database
        fc.mediaBayDb = roaming.getChildFile ("Steinberg/Cubase 14_64/MediaBay/mediabay3.db");
        makeDb (fc.mediaBayDb, { "CREATE TABLE media (id INTEGER, path TEXT, category TEXT)",
                                 "INSERT INTO media VALUES (1, 'x.vstpreset', 'Piano')" });
        write (roaming.getChildFile ("Steinberg/Cubase 14_64/Defaults.xml"), "<Defaults/>");

        // Cymatics
        write (docs.getChildFile ("Cymatics/Plinko/Presets/Bell Pluck.cyp"), "preset");
        write (docs.getChildFile ("Cymatics/Plinko/Presets/Glass Keys.cyp"), "preset");
        write (docs.getChildFile ("Cymatics/Plinko/Samples/bell.wav"), "RIFF");

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
        return fc;
    }
}
