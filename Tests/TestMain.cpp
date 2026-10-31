// Song Savant test runner: builds a fake computer, scans it, checks what was found and what the report says.
//   SSTests              engine tests
//   SSTests --shots DIR  also draw the screen to PNGs in DIR
//   SSTests --real       also scan this computer for real and print the summary (a smoke test; nothing is changed)
#include "FakeComputer.h"
#include "core/SetupScan.h"
#include "core/ScanJob.h"
#include "core/Report.h"
#include "core/MidiClip.h"
#include "core/VstPreset.h"
#include "core/PluginScanner.h"
#include "core/Paths.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <cstdlib>

namespace
{
    int failures = 0, checks = 0;

    void check (bool ok, const juce::String& what)
    {
        ++checks;
        if (! ok) { ++failures; std::cout << "  FAIL: " << what << std::endl; }
    }

    void section (const char* name)   { std::cout << "\n== " << name << std::endl; }

    const ss::PluginInfo* findPlugin (const ss::ScanResult& r, const juce::String& name)
    {
        for (auto& p : r.plugins)
            if (p.name == name) return &p;
        return nullptr;
    }

    void testPresetFormat()
    {
        section ("VST3 preset format");
        juce::StringPairArray a;
        a.set ("PlugInName", "HALion Sonic");
        a.set ("MusicalCategory", "Bass");
        a.set ("MusicalCharacter", "Warm & \"round\" <deep>");
        juce::MemoryBlock comp (64, true);
        auto mb = ss::makeVstPreset ("0123456789ABCDEF0123456789ABCDEF", comp, a);
        auto info = ss::parseVstPreset (static_cast<const juce::uint8*> (mb.getData()), mb.getSize());
        check (info.valid, "a preset we wrote reads back");
        check (info.hasInfo && info.attributes["MusicalCategory"] == "Bass", "its tags read back");
        check (info.attributes["MusicalCharacter"] == "Warm & \"round\" <deep>", "tags with XML characters survive");
        check (info.classId == "0123456789ABCDEF0123456789ABCDEF", "class ID");
        check (info.totalSize == (juce::int64) mb.getSize(), "total size is the whole preset");
        check (info.chunkIds.contains ("Comp") && info.chunkIds.contains ("Info"), "chunk list");

        auto cut = ss::parseVstPreset (static_cast<const juce::uint8*> (mb.getData()), mb.getSize() - 30);
        check (! cut.valid, "a cut-off preset is rejected");
        const char junk[] = "VST3 this is not really a preset at all, just text that starts the same way";
        check (! ss::parseVstPreset (reinterpret_cast<const juce::uint8*> (junk), sizeof (junk)).valid, "junk starting with VST3 is rejected");
        check (! ss::parseVstPreset (nullptr, 0).valid, "empty data is rejected");
    }

    void testHelpers()
    {
        section ("Helpers");
        check (ss::withCommas (0) == "0" && ss::withCommas (999) == "999" && ss::withCommas (1234567) == "1,234,567", "thousands separators");
        check (ss::keyName (9) == "A Major" && ss::keyName (21) == "A Minor" && ss::keyName (-1) == "no key", "key names");
        auto v = ss::parseLenientJson ("{ // comment\n \"a\": [1, 2, ], \"b\": \"x // not a comment\", /* block */ }");
        check (v.isObject() && v["a"].size() == 2 && v["b"].toString() == "x // not a comment", "lenient JSON (comments, trailing commas)");
        check (ss::normaliseName ("Native Instruments") == "nativeinstruments", "maker names");

        auto midi = ss::paths::tempDir().getChildFile ("tests/chords.mid");
        check (ss::writeTestChordClip (midi, 120.0), "test MIDI clip is written");
        juce::MidiFile mf;
        juce::FileInputStream in (midi);
        check (in.openedOk() && mf.readFrom (in), "test MIDI clip reads back");
        int noteOns = 0;
        double tempo = 0;
        if (mf.getNumTracks() > 0)
            for (auto* e : *mf.getTrack (0))
            {
                if (e->message.isNoteOn()) ++noteOns;
                if (e->message.isTempoMetaEvent()) tempo = 60.0 / e->message.getTempoSecondsPerQuarterNote();
            }
        check (noteOns == 16, "16 notes (4 chords of 4), got " + juce::String (noteOns));
        check (std::abs (tempo - 120.0) < 0.01, "tempo 120");
    }

    ss::ScanResult testScan (const sstest::FakeComputer& fc)
    {
        section ("Setup scan on a fake computer");
        std::atomic<bool> cancel { false };
        juce::StringArray stages;
        auto r = ss::runSetupScan (fc.loc, cancel, [&] (float, const juce::String& s) { stages.addIfNotAlreadyThere (s); });
        check (! r.cancelled, "scan finished");
        check (stages.size() > 4, "progress was reported");

        // plug-ins
        int vst3 = 0;
        for (auto& p : r.plugins) if (p.format == "VST3") ++vst3;
        check (vst3 == 6, "6 VST3 plug-ins, got " + juce::String (vst3));
        auto* halion = findPlugin (r, "HALion Sonic");
        check (halion != nullptr && halion->isInstrument && halion->vendor == "Steinberg Media Technologies", "HALion Sonic: instrument by Steinberg (JSON5 moduleinfo)");
        auto* plinko = findPlugin (r, "Plinko");
        check (plinko != nullptr && ! plinko->categoriesKnown && plinko->vendor == "Cymatics" && ! plinko->bundle, "Plinko: single file, maker from its folder");
        check (findPlugin (r, "Retrologue") != nullptr, "Retrologue found inside the Cubase folder");
        auto* eq = findPlugin (r, "Pro-Q 3");
        check (eq != nullptr && ! eq->isInstrument && eq->categoriesKnown, "an effect isn't an instrument");

        // preset files
        check (r.presets.files == 3, "3 preset files, got " + juce::String (r.presets.files));
        check (r.presets.readable == 2 && r.presets.withTags == 1, "2 readable, 1 with tags");
        check (r.presets.byPlugin["HALion Sonic"] == 1 && r.presets.byPlugin["Retrologue"] == 1, "presets by plug-in (tags, then folder)");
        check (r.presets.categories["Piano"] == 1, "category from tags");
        check (r.userPreset == fc.halionPreset, "drag test prefers the HALion Sonic preset");

        // VST Sound
        check (r.vstSoundFiles == 1 && r.vstSounds.size() == 1, "one sound library found and looked into");
        if (! r.vstSounds.empty())
        {
            auto& p = r.vstSounds.front();
            check (p.presetNames == 2, "two preset names (ASCII + UTF-16), got " + juce::String (p.presetNames));
            check (p.sampleNames.contains ("Bright Grand") && p.sampleNames.contains ("Airy Pad"), "preset names read");
            check (p.embeddedPresets == 1 && p.embeddedWithTags == 1, "embedded preset with tags");
            check (p.plugins.count ("HALion Sonic") == 1 && p.categories.count ("Synth Pad") == 1, "embedded tags read");
            check (p.headerText.startsWith ("VSTSOUND"), "header shown");
        }
        check (r.factoryPreset.existsAsFile(), "a factory preset was copied out for the drag test");
        auto fp = ss::readVstPreset (r.factoryPreset);
        check (fp.valid && fp.attributes["MusicalCategory"] == "Synth Pad", "the copied preset is a valid preset with its tags");

        // databases
        const ss::SqliteProbeResult* arturia = nullptr;
        const ss::SqliteProbeResult* mediabay = nullptr;
        for (auto& d : r.databases)
        {
            if (d.file == fc.arturiaDb) arturia = &d;
            if (d.file == fc.mediaBayDb) mediabay = &d;
        }
        check (arturia != nullptr && arturia->opened, "Arturia database opened");
        if (arturia != nullptr && ! arturia->tables.empty())
        {
            check (arturia->tables.front().name == "Preset_Id", "preset table ranked first, got " + arturia->tables.front().name);
            check (arturia->tables.front().rows == 3 && arturia->tables.front().sampleRows.size() == 3, "row count and sample rows");
            check (arturia->tables.front().sampleRows[0].contains ("name=Classic Jun Keys") && arturia->tables.front().sampleRows[0].contains ("<blob 4 bytes>"),
                   "sample row text and blob");
        }
        check (mediabay != nullptr && mediabay->opened, "MediaBay database in Cubase's settings found");
        check (fc.arturiaDb.existsAsFile() && ! fc.arturiaDb.getSiblingFile ("db.db3-wal").exists(), "original database untouched");

        // makers' folders
        bool cymatics = false, arturiaFolder = false;
        for (auto& v : r.vendorFolders)
        {
            if (v.vendor == "Cymatics")
                for (auto& [ext, n] : v.extensions) if (ext == ".cyp" && n == 2) cymatics = true;
            if (v.vendor == "Arturia") arturiaFolder = true;
        }
        check (cymatics, "Cymatics folder with its preset files");
        check (arturiaFolder, "Arturia folder");

        // Cubase + other apps
        check (r.cubase.size() == 2, "Cubase settings and install folders listed");
        check (r.siblings.songs.size() == 1 && r.siblings.songs[0].key == 21 && std::abs (r.siblings.songs[0].bpm - 92.0) < 0.01
               && r.siblings.songs[0].chords.size() == 7, "Key Compass song: A minor, 92 BPM, 7 chords");
        check (r.siblings.grooveGrabberKits == 1 && r.siblings.kitsByGenre["country"] == 1, "Groove Grabber kit");
        check (r.siblings.sampleSnaggerFiles == 1, "Sample Snagger library");
        return r;
    }

    void testReport (const ss::ScanResult& r, const sstest::FakeComputer& fc)
    {
        section ("Report");
        auto items = ss::summarise (r);
        check (items.size() >= 9, "summary rows");
        int ok = 0;
        for (auto& it : items) if (it.status > 0) ++ok;
        check (ok >= 8, "most rows found something, got " + juce::String (ok));

        ss::DragTests drag;
        drag.userPreset = 1;
        drag.midiClip = -1;
        const auto text = ss::buildReport (r, fc.loc, drag);
        check (text.startsWith ("SONG SAVANT - SETUP REPORT"), "title");
        check (text.contains ("[OK]  Plug-ins: 6 VST3 plug-ins, 4 of them instruments"), "plug-in summary line");
        check (text.contains ("Preset file -> Cubase:           WORKED"), "drag test answers");
        check (text.contains ("MIDI clip -> Cubase:             DIDN'T WORK"), "drag test answers (2)");
        check (text.contains ("table Preset_Id (3 rows)"), "database tables listed");
        check (text.contains ("~"), "paths shortened with ~");
        check (! text.contains (fc.loc.home.getFullPathName()), "the user's home folder is never written out");
        check (text.contains ("Back Roads  |  A Minor  |  92 BPM"), "Key Compass song listed");
    }

    void testCancelAndJob (const sstest::FakeComputer& fc)
    {
        section ("Cancelling and the background job");
        std::atomic<bool> cancel { true };
        auto r = ss::runSetupScan (fc.loc, cancel);
        check (r.cancelled, "a cancelled scan says so");

        ss::ScanJob job;
        const int g0 = job.generation();
        job.start (fc.loc);
        for (int i = 0; i < 600 && job.generation() == g0; ++i) juce::Thread::sleep (20);
        check (job.generation() == g0 + 1, "background scan finished");
        check (job.result() != nullptr && job.result()->presets.files == 3, "background scan result");
        check (! job.isScanning(), "thread stopped");
    }

    void writeShots (const juce::File& dir, const sstest::FakeComputer& fc, const ss::ScanResult& r)
    {
        section ("Screenshots");
        dir.createDirectory();
        auto shoot = [&] (const juce::String& name, bool withResult, int w, int h)
        {
            SongSavantProcessor proc;
            proc.locationProvider = [loc = fc.loc] { return loc; };
            if (withResult)
            {
                proc.scan.setResult (r, fc.loc);
                proc.dragTests.userPreset = 1;
            }
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            ed->setSize (w, h);
            if (auto* e = dynamic_cast<SongSavantEditor*> (ed.get())) e->getPage().refreshFromResult();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = dir.getChildFile (name + ".png");
            f.deleteFile();
            juce::FileOutputStream out (f);
            juce::PNGImageFormat png;
            check (out.openedOk() && png.writeImageToStream (img, out), "wrote " + f.getFileName());
            std::cout << "  " << f.getFullPathName() << std::endl;
        };
        shoot ("setup-check-empty", false, 1140, 800);
        shoot ("setup-check-results", true, 1140, 800);
        shoot ("setup-check-small", true, 1060, 760);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);

    auto home = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("SongSavantTests");
    home.deleteRecursively();
    const auto testHome = home.getChildFile ("ss-home").getFullPathName();
   #if JUCE_WINDOWS
    _putenv_s ("SS_TEST_HOME", testHome.toRawUTF8());
   #else
    setenv ("SS_TEST_HOME", testHome.toRawUTF8(), 1);
   #endif

    auto fc = sstest::makeFakeComputer (home.getChildFile ("computer"));

    testPresetFormat();
    testHelpers();
    auto r = testScan (fc);
    testReport (r, fc);
    testCancelAndJob (fc);

    if (const int i = args.indexOf ("--shots"); i >= 0)
        writeShots (juce::File::getCurrentWorkingDirectory().getChildFile (args[i + 1].isNotEmpty() ? args[i + 1] : "screenshots"), fc, r);

    if (args.contains ("--real"))
    {
        section ("This computer (smoke test)");
        std::atomic<bool> cancel { false };
        const auto loc = ss::SystemLocations::forThisComputer();
        auto real = ss::runSetupScan (loc, cancel);
        check (! real.cancelled, "the real scan finished");
        std::cout << "  took " << real.seconds << " s" << std::endl;
        for (auto& it : ss::summarise (real))
            std::cout << "  " << it.title << ": " << it.detail << std::endl;
        check (ss::buildReport (real, loc, {}).isNotEmpty(), "report for this computer");
    }

    std::cout << "\n" << checks << " checks, " << failures << " failed" << std::endl;
    if (failures == 0) home.deleteRecursively();
    return failures == 0 ? 0 : 1;
}
