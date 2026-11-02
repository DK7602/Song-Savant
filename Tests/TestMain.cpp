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
#include "core/DropInfo.h"
#include "core/Catalog.h"
#include "core/Music.h"
#include "core/Parts.h"
#include "core/Palette.h"
#include "core/PluginProfiles.h"
#include "core/PluginTypes.h"
#include "core/KitMap.h"
#include "core/KitMapJob.h"
#include "core/StyleGuide.h"
#include "core/StarterParts.h"
#include "core/Arrangement.h"
#include "core/GrooveGrabberLink.h"
#include "core/SongSession.h"
#include <map>
#include <algorithm>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/DrumMapsPage.h"
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

        const auto midiDrop = ss::describeDroppedFile (midi);
        check (midiDrop.startsWith ("chords.mid: MIDI file, 1 track with notes, 16 notes, about 4.0 bars at 120 BPM"), "dropped MIDI described: " + midiDrop);
        check (ss::describeDroppedFile (midi.getParentDirectory()).endsWith (": a folder"), "dropped folder");
        auto other = midi.getSiblingFile ("thing.xyz");
        other.replaceWithText ("hello");
        check (ss::describeDroppedFile (other) == "thing.xyz: .xyz file (5 B)", "dropped unknown file: " + ss::describeDroppedFile (other));
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
        check (vst3 == 11, "11 VST3 plug-ins, got " + juce::String (vst3));
        auto* halion = findPlugin (r, "HALion Sonic");
        check (halion != nullptr && halion->isInstrument && halion->vendor == "Steinberg Media Technologies", "HALion Sonic: instrument by Steinberg (JSON5 moduleinfo)");
        auto* plinko = findPlugin (r, "Plinko");
        check (plinko != nullptr && ! plinko->categoriesKnown && plinko->vendor == "Cymatics" && ! plinko->bundle, "Plinko: single file, maker from its folder");
        check (findPlugin (r, "Retrologue") != nullptr && findPlugin (r, "Retrologue")->categorySource == "moduleinfo",
               "Retrologue found inside the Cubase folder (moduleinfo in Contents/Resources)");
        auto* lab = findPlugin (r, "Analog Lab V");
        check (lab != nullptr && ! lab->bundle && lab->isInstrument && lab->categorySource == "Cubase", "Analog Lab V: type from Cubase's plug-in list");
        check (r.pluginTypesFromCubase == 1 && r.cubasePluginList.existsAsFile(), "Cubase's plug-in list read");
        auto* eq = findPlugin (r, "Pro-Q 3");
        check (eq != nullptr && ! eq->isInstrument && eq->categoriesKnown, "an effect isn't an instrument");

        // preset files
        check (r.presets.files == 3, "3 preset files, got " + juce::String (r.presets.files));
        check (r.presets.readable == 2 && r.presets.withTags == 1, "2 readable, 1 with tags");
        check (r.presets.byPlugin["HALion Sonic"] == 1 && r.presets.byPlugin["Retrologue"] == 1, "presets by plug-in (tags, then folder)");
        check (r.presets.categories["Piano"] == 1, "category from tags");
        check (r.userPreset == fc.halionPreset, "drag test prefers the HALion Sonic preset");
        check (ss::describeDroppedFile (fc.halionPreset).contains ("VST3 preset for HALion Sonic, tags: Piano / Piano|A. Piano"), "dropped preset described");

        // VST Sound
        check (r.vstSoundFiles == 2 && r.vstSounds.size() == 2, "two sound libraries found (one on another drive)");
        const ss::VstSoundProbeResult* lib = nullptr;
        for (auto& p : r.vstSounds) if (p.file == fc.vstSound) lib = &p;
        check (lib != nullptr, "the HALion library was looked into");
        if (lib != nullptr)
        {
            auto& p = *lib;
            check (p.presetNames == 2, "two preset names (ASCII + UTF-16), got " + juce::String (p.presetNames));
            check (p.sampleNames.contains ("Bright Grand") && p.sampleNames.contains ("Airy Pad"), "preset names read");
            check (p.embeddedPresets == 1 && p.embeddedWithTags == 1, "embedded preset with tags");
            check (p.plugins.count ("HALion Sonic") == 1 && p.categories.count ("Synth Pad") == 1, "embedded tags read");
            check (p.headerText.startsWith ("VSTSOUND"), "header shown");
            check (p.textAtStart.contains ("/.AppData/Steinberg/HALion Sonic/Factory Presets/Pianos/Bright Grand.vstpreset"), "readable text near the start");
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
        check (arturia != nullptr && arturia->opened && arturia->kind == "Arturia presets", "Arturia database opened and recognised");
        auto insight = [] (const ss::SqliteProbeResult* db, const juce::String& title) -> juce::StringArray
        {
            if (db != nullptr)
                for (auto& [t, lines] : db->insights) if (t == title) return lines;
            return {};
        };
        if (arturia != nullptr)
        {
            const ss::SqliteTable* presetTable = nullptr;
            for (auto& tb : arturia->tables) if (tb.name == "Preset_Id") presetTable = &tb;
            check (presetTable != nullptr && presetTable->rows == 9 && presetTable->sampleRows.size() == 3, "Preset_Id: row count and sample rows");
            if (presetTable != nullptr && ! presetTable->sampleRows.isEmpty())
                check (presetTable->sampleRows[0].contains ("name=Classic Jun Keys") && presetTable->sampleRows[0].contains ("..."), "sample row text (cut at 14 columns)");
            check (insight (arturia, "Presets by instrument").contains ("Analog Lab | 7") && insight (arturia, "Presets by instrument").contains ("Prophet | 1"), "Arturia: presets by instrument");
            check (insight (arturia, "Presets by type").contains ("Keys | 1"), "Arturia: presets by type");
            check (insight (arturia, "Characteristics, genres and styles").contains ("Characteristics | Warm | 3"), "Arturia: characteristics");
            check (insight (arturia, "Genres").contains ("2 | Worship"), "Arturia: genres");
            check (insight (arturia, "Analog Lab factory presets by instrument").contains ("Analog Lab | Analog Lab V | 6"), "Arturia: factory presets by instrument");
            check (insight (arturia, "Analog Lab preset files by instrument (instrument | on disk | missing)").contains ("Analog Lab V | 4 | 2")
                   && insight (arturia, "Analog Lab preset files by instrument (instrument | on disk | missing)").contains ("Prophet V3 | 1 | 0"),
                   "Arturia: preset files on disk, per instrument");
            check (insight (arturia, "Instruments inside layered presets").contains ("Prophet V3 | 1")
                   && insight (arturia, "Analog Lab version tags (tag | value | presets)").contains ("ALVersionFirst | 5.0.0 | 1"), "Arturia: layers and version tags");
            check (insight (arturia, "Packs (name | factory | presets)").contains ("Analog Lab Factory | 1 | 8"), "Arturia: packs");
            check (insight (arturia, "Packs for sale marked as owned").contains ("Worship Keys | 60"), "Arturia: owned packs");
        }
        check (mediabay != nullptr && mediabay->opened && mediabay->kind == "Steinberg MediaBay", "MediaBay database found and recognised");
        check (insight (mediabay, "Presets by plug-in").contains ("HALion Sonic SE | 50"), "MediaBay: presets by plug-in");
        check (insight (mediabay, "Presets with tags").joinIntoString ("").startsWith ("52 | 51 | 50 | 50"), "MediaBay: how many presets have tags");
        check (insight (mediabay, "Most common MusicalCategory").contains ("Keyboard | 50"), "MediaBay: categories");
        check (insight (mediabay, "Examples: HALion Sonic SE").size() == 4, "MediaBay: examples");
        check (insight (mediabay, "Examples: your own presets").joinIntoString ("").contains ("My Keys"), "MediaBay: your own presets");
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

        // privacy: private-looking files and folders are never listed or matched
        auto keyFolders = ss::findVendorFolders (fc.loc, "Key Compass");
        bool touchedSecret = false;
        for (auto& kf : keyFolders) if (kf.getFileName().containsIgnoreCase ("Secret")) touchedSecret = true;
        check (! touchedSecret, "a folder named like a secret key is never matched");
        auto steinbergFolders = ss::findVendorFolders (fc.loc, "Steinberg");
        check (steinbergFolders.contains (fc.driveD.getChildFile ("Steinberg")) && ! steinbergFolders.contains (fc.driveD.getChildFile ("Steinberg Backup")),
               "on the top of a drive, maker folders must match exactly");
        bool privateCounted = false, privateNamed = false;
        for (auto& v : r.vendorFolders)
            if (v.vendor == "Cymatics")
            {
                for (auto& [ext, n] : v.extensions) if (ext == "(private, not listed)") privateCounted = true;
                for (auto& [ext, names] : v.samples) for (auto& nm : names) if (nm.containsIgnoreCase ("license")) privateNamed = true;
            }
        check (privateCounted && ! privateNamed, "private-looking files are counted but never named");
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
        check (text.contains ("[OK]  Plug-ins: 11 VST3 plug-ins, 6 of them instruments"), "plug-in summary line");
        check (text.contains ("Preset file -> Cubase:           WORKED"), "drag test answers");
        check (text.contains ("MIDI clip -> Cubase:             DIDN'T WORK"), "drag test answers (2)");
        check (text.contains ("table Preset_Id (9 rows)"), "database tables listed");
        check (text.contains ("~"), "paths shortened with ~");
        check (! text.contains (fc.loc.home.getFullPathName()), "the user's home folder is never written out");
        check (! text.containsIgnoreCase ("Secret"), "nothing private-looking in the report");
        const auto h = fc.loc.home.getFullPathName();
        check (fc.loc.tidy ("url=file://46" + h.replaceCharacter ('\\', '/') + "/Documents/x") == "url=file://46~/Documents/x", "home folder hidden in file:// links");
        check (fc.loc.tidy ("p=" + h.replaceCharacter ('/', '\\') + "\\Documents") == "p=~\\Documents", "home folder hidden with either slash");
        check (text.contains ("Back Roads  |  A Minor  |  92 BPM"), "Key Compass song listed");
        check (text.contains ("Cubase -> Song Savant:           nothing dropped"), "nothing dragged from Cubase yet");
        check (text.contains ("type from Cubase's list"), "where a plug-in's type came from");
        check (text.contains ("recognised as: Steinberg MediaBay") && text.contains ("recognised as: Arturia presets"), "recognised databases");
        bool mediaBayRow = false, arturiaRow = false;
        for (auto& it : items)
        {
            if (it.title == "Steinberg MediaBay") mediaBayRow = it.status == 1 && it.detail.contains ("52 presets (51 with a category)");
            if (it.title == "Arturia (Analog Lab)") arturiaRow = it.status == 1 && it.detail.contains ("9 presets, 3 instruments");
        }
        check (mediaBayRow, "summary: MediaBay (not the decoy database)");
        check (arturiaRow, "summary: Arturia");

        drag.fromDaw.add ("Piano.mid: MIDI file");
        check (ss::buildReport (r, fc.loc, drag).contains ("Cubase -> Song Savant:           1 dropped\n  Piano.mid: MIDI file"), "things dragged from Cubase are listed");
    }

    void testCatalog (const sstest::FakeComputer& fc)
    {
        section ("Catalog");
        juce::String problem;
        auto mb = ss::readMediaBay (fc.mediaBayDb, problem);
        check (problem.isEmpty() && mb.size() == 51, "MediaBay: 51 instrument presets (effects left out), got " + juce::String ((int) mb.size()) + " " + problem);
        const ss::Sound* keys0 = nullptr;
        const ss::Sound* mine = nullptr;
        for (auto& snd : mb)
        {
            if (snd.name == "Keys 0") keys0 = &snd;
            if (snd.name == "My Keys") mine = &snd;
        }
        check (keys0 != nullptr && keys0->instrument == "HALion Sonic" && keys0->kind == ss::Kind::keys && keys0->detail == "E. Piano"
               && keys0->library == "HALion Sonic Library" && ! keys0->guessed, "MediaBay preset: name, instrument (SE -> HALion Sonic), kind, library");
        if (keys0 != nullptr)
            check (keys0->styles.contains ("Pop") && keys0->styles.contains ("Pop/Rock") && keys0->character.contains ("Warm"), "MediaBay preset: styles and character");
        check (mine != nullptr && mine->yours && mine->library == "Your presets" && mine->kind == ss::Kind::keys && mine->guessed, "your own preset, kind guessed");

        int arLeftOut = -1;
        auto ar = ss::readArturia (fc.arturiaDb, problem, &arLeftOut);
        check (ar.size() == 5, "Arturia: 5 presets (hidden one left out), got " + juce::String ((int) ar.size()));
        check (arLeftOut == 3, "Arturia: 3 left out (old Prophet V3, a layered preset using it, a missing file), got " + juce::String (arLeftOut));
        bool oldOnes = false;
        for (auto& snd : ar) oldOnes = oldOnes || snd.name == "75 Lead Soft" || snd.name == "Prophet Stack" || snd.name == "Lost File Pad";
        check (! oldOnes, "presets today's Analog Lab can't play are never suggested");
        std::map<juce::String, const ss::Sound*> byName;
        for (auto& snd : ar) byName[snd.name] = &snd;
        auto* pad = byName["Heaven Pad"];
        check (pad != nullptr && pad->kind == ss::Kind::pad && pad->owned && pad->styles.contains ("Worship") && pad->character.contains ("Warm")
               && pad->instrument == "Analog Lab V" && pad->origin == "Analog Lab (layered)" && pad->detail == "Warm Pad", "Arturia preset: kind, tags, instrument");
        check (byName["Jupiter Brass"] != nullptr && ! byName["Jupiter Brass"]->owned && byName["Jupiter Brass"]->origin == "Jup-8 V4", "a preset you'd have to buy");
        check (byName["My Analog Lab Bass"] != nullptr && byName["My Analog Lab Bass"]->owned && byName["My Analog Lab Bass"]->yours, "your own Analog Lab preset");

        auto cy = ss::readCymatics (fc.loc.cymaticsRoots);
        std::map<juce::String, const ss::Sound*> cyByName;
        for (auto& snd : cy) cyByName[snd.name] = &snd;
        check (cy.size() == 3, "Cymatics: 3 presets (Init and effects left out), got " + juce::String ((int) cy.size()));
        check (cyByName["Glass Bell"] != nullptr && cyByName["Glass Bell"]->instrument == "Cymatics Plinko" && cyByName["Glass Bell"]->kind == ss::Kind::pluck
               && cyByName["Glass Bell"]->library == "Factory", "Cymatics Plinko preset");
        check (cyByName["Warm Tape Keys"] != nullptr && cyByName["Warm Tape Keys"]->instrument == "Cymatics Daydream" && cyByName["Warm Tape Keys"]->library == "01 Pack"
               && cyByName["Warm Tape Keys"]->kind == ss::Kind::keys, "Cymatics Daydream preset");
        check (cyByName["BASS Apocalypse"] != nullptr && cyByName["BASS Apocalypse"]->kind == ss::Kind::bass && cyByName["BASS Apocalypse"]->yours, "Cymatics Pandora preset");

        auto files = ss::readPresetFiles (fc.loc);
        check (files.size() == 2, "preset files: 2 readable, got " + juce::String ((int) files.size()));
        for (auto& snd : files)
            if (snd.name == "Warm Grand") check (snd.kind == ss::Kind::piano && snd.instrument == "HALion Sonic" && ! snd.guessed, "preset file with its own tags");

        std::atomic<bool> cancel { false };
        auto cat = ss::buildCatalog (fc.loc, cancel);
        // + plug-ins: Groove Grabber and SSD Sampler (drums), FM8 (6 kinds), Mystery Keys (piano, from its own categories),
        //   Retrologue (synth: none of its presets are in this library)
        check (cat.sources.size() == 5 && cat.sounds.size() == 51 + 5 + 3 + 10 + 1, "catalog: 5 sources, 70 sounds (one a Groove Grabber kit), got "
               + juce::String ((int) cat.sounds.size()));
        check (cat.sounds.front().kind == ss::Kind::keys || cat.sounds.front().kind == ss::Kind::piano, "sorted by kind");
        check (cat.instruments().contains ("Analog Lab V") && cat.instruments().contains ("HALion Sonic") && cat.instruments().contains ("Cymatics Plinko"), "instruments");

        ss::SoundFilter f;
        f.text = "warm pad";
        auto hits = ss::findSounds (cat, f);
        check (hits.size() == 1 && cat.sounds[(size_t) hits[0]].name == "Heaven Pad", "search: every word must match");
        f.text = "";
        f.kinds = { ss::Kind::bass };
        hits = ss::findSounds (cat, f);
        check (hits.size() == 4, "filter by kind (owned only): 3 bass presets + FM8, got " + juce::String ((int) hits.size()));
        f.kinds.clear();
        f.text = "jupiter";
        check (ss::findSounds (cat, f).empty(), "presets you'd have to buy are hidden");
        f.ownedOnly = false;
        check (ss::findSounds (cat, f).size() == 1, "...unless you ask for them");
        f = {};
        f.instrument = "Cymatics Plinko";
        check (ss::findSounds (cat, f).size() == 1, "filter by instrument");
        f = {};
        f.yoursOnly = true;
        check (ss::findSounds (cat, f).size() == 4, "your own presets and kits");

        check (ss::howToLoad (*keys0).contains ("All Media") && ss::howToLoad (*pad).contains ("Analog Lab V")
               && ss::howToLoad (*byName["Jupiter Brass"]).startsWith ("Not owned"), "how to load");
        check (ss::whereToPaste (*keys0).contains ("MediaBay") && ss::whereToPaste (*pad).contains ("Analog Lab's search")
               && ss::whereToPaste (*cyByName["Glass Bell"]).contains ("Cymatics Plinko's preset list"), "where to paste depends on the library");
        check (ss::guessKind ("Bassoon Solo") == ss::Kind::woodwinds && ss::guessKind ("Deep Sub 808 Bass") == ss::Kind::bass
               && ss::guessKind ("Kick 808") == ss::Kind::drums && ss::guessKind ("Warm Rhodes") == ss::Kind::keys, "guessing a kind from words");
    }

    void testPluginSuggestions (const sstest::FakeComputer& fc)
    {
        section ("Plug-ins suggested as instruments");
        check (ss::profileFor ("SSDSampler5") != nullptr && juce::String (ss::profileFor ("SSDSampler5")->name) == "SSD Sampler 5", "known by name");
        check (ss::profileFor ("Vital") != nullptr && ss::profileFor ("SPL Vitalizer") == nullptr && ss::profileFor ("AudioLabs Gate") == nullptr
               && ss::profileFor ("Serum2") != nullptr, "short names must match the whole name");
        ss::PluginInfo plinko;
        plinko.name = "Plinko";
        plinko.file = fc.loc.vst3Folders.getFirst().getChildFile ("Cymatics/Plinko.vst3");
        check (ss::profileFor (plinko) != nullptr && juce::String (ss::profileFor (plinko)->name) == "Cymatics Plinko", "maker found from the folder");
        ss::PluginInfo fx;
        fx.name = "Serum FX";
        fx.categoriesKnown = true;
        check (ss::profileFor (fx) == nullptr, "an effect is never suggested as an instrument");

        std::atomic<bool> cancel { false };
        auto cat = ss::buildCatalog (fc.loc, cancel);
        std::map<juce::String, std::vector<const ss::Sound*>> plugins;
        for (auto& snd : cat.sounds)
            if (snd.source == "Plug-in") plugins[snd.name].push_back (&snd);
        juce::StringArray names;
        for (auto& [n, v] : plugins) names.add (n);
        check (plugins.size() == 5, "5 plug-ins suggested, got " + names.joinIntoString (", "));
        check (plugins["SSD Sampler 5"].size() == 1 && plugins["SSD Sampler 5"][0]->kind == ss::Kind::drums
               && plugins["SSD Sampler 5"][0]->detail == "Pick a kit in the plug-in", "SSD Sampler 5 for drums");
        check (plugins["Groove Grabber"].size() == 1 && plugins["Groove Grabber"][0]->kind == ss::Kind::drums, "Groove Grabber for drums");
        check (plugins["FM8"].size() == 6, "FM8 for several kinds");
        check (plugins["Mystery Keys"].size() == 1 && plugins["Mystery Keys"][0]->kind == ss::Kind::piano, "an unknown instrument goes by its own categories");
        check (plugins.count ("HALion Sonic") == 0 && plugins.count ("Analog Lab V") == 0 && plugins.count ("Cymatics Plinko") == 0,
               "instruments whose presets are listed aren't suggested twice");
        check (plugins["Retrologue"].size() == 1 && plugins["Retrologue"][0]->kind == ss::Kind::synth, "an instrument with no listed presets is suggested");
        check (plugins.count ("Odd Synth") == 0 && plugins.count ("SPL Vitalizer MK2-T") == 0 && plugins.count ("Pro-Q 3") == 0, "effects and unknowns left out");
        if (! plugins["SSD Sampler 5"].empty())
        {
            const auto& ssd = *plugins["SSD Sampler 5"][0];
            check (ss::howToLoad (ssd).contains ("SSD Sampler 5 (SSDSampler5 in your plug-in list)") && ss::whereToPaste (ssd).contains ("instrument list")
                   && ss::nameToCopy (ssd) == "SSDSampler5", "how to load a plug-in, and the name your DAW knows it by");
            ss::PartSpec drums;
            drums.role = "Drums";
            drums.kinds = { ss::Kind::drums };
            ss::Sound preset = ssd;
            preset.source = "MediaBay";
            const auto brief = ss::readBrief ("rock", {}, {});
            check (ss::scoreSound (ssd, drums, brief) < ss::scoreSound (preset, drums, brief), "a real preset beats a plug-in suggestion");
        }
    }

    void testPluginCheck (const sstest::FakeComputer& fc)
    {
        section ("Finding out what a plug-in is (Song Savant Scanner)");
        const juce::File scanner (SS_TEST_SCANNER);
        const juce::File realPlugin (SS_TEST_VST3);
        check (scanner.existsAsFile() && realPlugin.exists(), "the scanner and the Song Savant plug-in were built");

        // a copy of the real plug-in without its moduleinfo.json, so nothing on disk says what it is
        const auto copyDir = fc.loc.home.getChildFile ("Plug-ins to check");
        const auto copy = copyDir.getChildFile (realPlugin.getFileName());   // a bundle's name must match the file inside
        copyDir.createDirectory();
        check (realPlugin.isDirectory() ? realPlugin.copyDirectoryTo (copy) : realPlugin.copyFileTo (copy), "copied the plug-in");
        copy.getChildFile ("Contents/Resources/moduleinfo.json").deleteFile();
        copy.getChildFile ("Contents/moduleinfo.json").deleteFile();

        std::vector<ss::PluginInfo> list (2);
        list[0].name = "Song Savant (copy)";
        list[0].format = "VST3";
        list[0].file = copy;
        list[1].name = "Odd Synth";
        list[1].format = "VST3";
        list[1].file = fc.loc.vst3Folders.getFirst().getChildFile ("Odd Synth.vst3");

        std::atomic<bool> cancel { false };
        const auto one = ss::checkPlugin (copy, scanner, cancel);
        check (one.ok && one.instrument && one.category.contains ("Instrument") && one.vendor == "Grabber Audio",
               "the scanner says what the plug-in is: " + one.category + " by " + one.vendor);
        check (! ss::checkPlugin (list[1].file, scanner, cancel).ok, "a file that isn't really a plug-in fails safely");

       #if JUCE_WINDOWS
        _putenv_s ("SS_SCANNER", scanner.getFullPathName().toRawUTF8());
       #else
        setenv ("SS_SCANNER", scanner.getFullPathName().toRawUTF8(), 1);
       #endif
        check (ss::pluginsToCheck (list).size() == 2, "both need checking");
        int calls = 0;
        check (ss::checkPlugins (ss::pluginsToCheck (list), cancel, 120, [&calls] (int, int) { ++calls; }) == 2 && calls >= 2, "checked both, with progress");
        check (ss::pluginsToCheck (list).empty(), "each plug-in is checked only once (even the one that failed)");
        ss::applyKnownTypes (list);
        check (list[0].categoriesKnown && list[0].isInstrument && list[0].categorySource == "checked" && list[0].categories.contains ("Instrument"),
               "the answer is remembered");
        check (! list[1].categoriesKnown, "a failed check doesn't make anything up");
        check (ss::paths::appDataDir().getChildFile ("plugin-types.json").existsAsFile(), "remembered in Song Savant's own folder");

        std::atomic<bool> stop { true };
        check (! ss::checkPlugin (copy, scanner, stop).ok, "a check can be stopped");
       #if JUCE_WINDOWS
        _putenv_s ("SS_SCANNER", "");
       #else
        unsetenv ("SS_SCANNER");
       #endif
        copyDir.deleteRecursively();
    }

    // made-up drum sounds, 1.2 seconds at 44.1 kHz
    std::vector<float> synthHit (const juce::String& what)
    {
        const double sr = 44100.0;
        std::vector<float> x ((size_t) (sr * 1.2), 0.0f);
        juce::Random rng (42);
        auto noise = [&rng] { return rng.nextFloat() * 2.0f - 1.0f; };
        auto filtered = [&] (std::vector<float> v, juce::IIRCoefficients c, int passes)
        {
            for (int p = 0; p < passes; ++p) { juce::IIRFilter f; f.setCoefficients (c); f.processSamples (v.data(), (int) v.size()); }
            return v;
        };
        std::vector<float> white (x.size());
        for (auto& v : white) v = noise();
        double phase = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const double t = (double) i / sr;
            if (what == "kick")
            {
                phase += juce::MathConstants<double>::twoPi * (50.0 + 100.0 * std::exp (-t / 0.03)) / sr;
                x[i] = (float) (std::sin (phase) * std::exp (-t / 0.06));
            }
            else if (what == "808")  x[i] = (float) (std::sin (juce::MathConstants<double>::twoPi * 45.0 * t) * std::exp (-t / 0.35));
            else if (what == "tom")  x[i] = (float) (std::sin (juce::MathConstants<double>::twoPi * 120.0 * t) * std::exp (-t / 0.1));
            else if (what == "high tom") x[i] = (float) (std::sin (juce::MathConstants<double>::twoPi * 600.0 * t) * std::exp (-t / 0.1));
            else if (what == "kick up")
            {
                phase += juce::MathConstants<double>::twoPi * (200.0 + 400.0 * std::exp (-t / 0.015)) / sr;
                x[i] = (float) (std::sin (phase) * std::exp (-t / 0.03));
            }
            else if (what == "cowbell")
                x[i] = (float) ((0.6 * std::sin (juce::MathConstants<double>::twoPi * 800.0 * t) + 0.4 * std::sin (juce::MathConstants<double>::twoPi * 540.0 * t))
                                * std::exp (-t / 0.06));
        }
        if (what == "snare" || what == "snare up")
        {
            const bool up = what == "snare up";      // the same snare played an octave higher
            const auto n = filtered (white, juce::IIRCoefficients::makeLowPass (sr, up ? 10000.0 : 5000.0), 2);
            for (size_t i = 0; i < x.size(); ++i)
            {
                const double t = (double) i / sr;
                x[i] = (float) (n[i] * std::exp (-t / (up ? 0.025 : 0.05))
                                + 0.6 * std::sin (juce::MathConstants<double>::twoPi * (up ? 380.0 : 190.0) * t) * std::exp (-t / (up ? 0.02 : 0.04)));
            }
        }
        else if (what == "clap")
        {
            auto n = filtered (white, juce::IIRCoefficients::makeBandPass (sr, 1800.0, 1.0), 1);
            for (size_t i = 0; i < x.size(); ++i)
            {
                const double t = (double) i / sr;
                double a = 0;
                for (double b : { 0.0, 0.011, 0.022, 0.033 }) if (t >= b) a = std::max (a, std::exp (-(t - b) / 0.002));
                if (t >= 0.033) a = std::max (a, 0.5 * std::exp (-(t - 0.033) / 0.05));
                x[i] = (float) (n[i] * 3.0 * a);
            }
        }
        else if (what == "closed hat" || what == "open hat" || what == "cymbal")
        {
            const auto n = filtered (white, juce::IIRCoefficients::makeHighPass (sr, 7000.0), 2);
            const double tau = what == "closed hat" ? 0.015 : what == "open hat" ? 0.12 : 0.5;
            for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (n[i] * std::exp (-((double) i / sr) / tau));
        }
        return x;
    }

    void testKitMap()
    {
        section ("Mapping a drum kit");
        const std::pair<const char*, const char*> sounds[] = { { "kick", "Kick" }, { "808", "808" }, { "snare", "Snare" }, { "clap", "Clap" },
                                                               { "closed hat", "Closed Hat" }, { "open hat", "Open Hat" }, { "cymbal", "Cymbal" },
                                                               { "tom", "Tom" } };
        for (auto& [what, expected] : sounds)
        {
            const auto x = synthHit (what);
            const auto f = ss::analyseHit (x.data(), (int) x.size(), 44100.0);
            const auto got = ss::classifyHit (f);
            check (got == expected, juce::String (what) + " heard as " + got + "  (low " + juce::String (f.lowRatio, 2) + ", high " + juce::String (f.highRatio, 2)
                                    + ", zcr " + juce::String ((int) f.zcr) + ", decay " + juce::String (f.decay, 2) + " s, pitch " + juce::String ((int) f.pitchHz)
                                    + " Hz, bursts " + juce::String (f.bursts) + ")");
        }
        // played higher up the keyboard: pitched sounds mustn't pass for snares (the owner's Beat Box, C3 to C6)
        const std::pair<const char*, juce::StringArray> pitched[] = { { "high tom", { "Tom", "Perc" } }, { "kick up", { "Kick", "Tom", "Perc" } },
                                                                      { "cowbell", { "Perc" } }, { "snare up", { "Snare" } } };
        for (auto& [what, allowed] : pitched)
        {
            const auto x = synthHit (what);
            const auto f = ss::analyseHit (x.data(), (int) x.size(), 44100.0);
            const auto got = ss::classifyHit (f);
            check (allowed.contains (got), juce::String (what) + " heard as " + got + "  (tonal " + juce::String (f.tonal, 2) + " at " + juce::String ((int) f.toneHz)
                                           + " Hz, zcr " + juce::String ((int) f.zcr) + ", decay " + juce::String (f.decay, 2) + " s)");
        }
        // the kinds of sound in the owner's Beat Box (Analog Lab), as Song Savant measures them (round numbers)
        {
            auto hit = [] (float tonal, float tone, float lowHz, float zcr, float decay, float low, float body, float high, int bursts)
            {
                ss::HitFeatures f;
                f.peak = 0.3f; f.tonal = tonal; f.toneHz = tone; f.pitchHz = lowHz; f.zcr = zcr; f.decay = decay;
                f.lowRatio = low; f.bodyRatio = body; f.highRatio = high; f.bursts = bursts;
                return ss::classifyHit (f);
            };
            const auto kick = hit (0.9f, 800, 25, 350, 0.13f, 0.95f, 0.97f, 0.0f, 3);
            const auto snare = hit (0.8f, 120, 130, 1900, 0.15f, 0.45f, 0.9f, 0.0f, 2);     // pitched, but noisier than the pitch explains
            const auto tom = hit (0.8f, 100, 95, 360, 0.17f, 0.65f, 0.94f, 0.0f, 2);
            const auto breathyTom = hit (0.3f, 150, 130, 1550, 0.3f, 0.15f, 0.48f, 0.0f, 1); // noisy, but low, dark and ringing
            const auto perc = hit (0.94f, 720, 500, 1800, 0.09f, 0.0f, 0.01f, 0.0f, 1);
            check (kick == "Kick" && snare == "Snare" && tom == "Tom" && breathyTom == "Tom" && perc == "Perc",
                   "Beat Box's sounds: " + kick + ", " + snare + ", " + tom + ", " + breathyTom + ", " + perc);
        }
        std::vector<float> quiet (44100, 0.0f);
        check (ss::classifyHit (ss::analyseHit (quiet.data(), (int) quiet.size(), 44100.0)).isEmpty(), "a silent key is left out");

        ss::KitMap k;
        k.name = "Beat Box";
        k.plugin = "Analog Lab V";
        for (auto [note, kind] : std::initializer_list<std::pair<int, const char*>> { { 36, "Kick" }, { 38, "Snare" }, { 42, "Closed Hat" }, { 45, "Tom" },
                                                                                       { 47, "Tom" }, { 48, "Tom" }, { 35, "Kick" }, { 46, "Open Hat" } })
        {
            ss::KitPad p;
            p.note = note;
            p.kind = kind;
            k.pads.push_back (p);
        }
        ss::nameKitPads (k);
        juce::StringArray names;
        for (auto& p : k.pads) names.add (ss::noteName (p.note) + " " + p.name);
        check (names.joinIntoString (", ") == "B0 Kick, C1 Kick 2, D1 Snare, F#1 Closed Hat, A1 Low Tom, A#1 Open Hat, B1 Mid Tom, C2 High Tom",
               "pads named and in order: " + names.joinIntoString (", "));
        check (! k.oneSound && k.ok(), "a real kit");
        check (ss::noteName (60) == "C3" && ss::noteName (36) == "C1", "Cubase's note names");

        const auto back = ss::kitMapFromJson (juce::JSON::parse (juce::JSON::toString (ss::kitMapToJson (k))));
        check (back.name == "Beat Box" && back.pads.size() == k.pads.size() && back.pads[3].name == "Closed Hat", "a kit map survives the trip from the helper");

        const auto xml = juce::parseXML (ss::makeCubaseDrumMap (k));
        bool ok = xml != nullptr && xml->hasTagName ("DrumMap");
        int items = 0;
        juce::String at36, firstInOrder;
        if (ok)
            for (auto* list : xml->getChildWithTagNameIterator ("list"))
            {
                if (list->getStringAttribute ("name") == "Map")
                    for (auto* item : list->getChildIterator())
                    {
                        ++items;
                        int in = -1;
                        juce::String nm;
                        for (auto* field : item->getChildIterator())
                        {
                            if (field->getStringAttribute ("name") == "INote") in = field->getIntAttribute ("value");
                            if (field->getStringAttribute ("name") == "Name") nm = field->getStringAttribute ("value");
                        }
                        if (in == 36) at36 = nm;
                    }
                if (list->getStringAttribute ("name") == "Order" && list->getFirstChildElement() != nullptr)
                    firstInOrder = list->getFirstChildElement()->getStringAttribute ("value");
            }
        check (ok && items == 128 && at36 == "Kick 2" && firstInOrder == "35", "a Cubase drum map: 128 keys, named, the kit first");
        const auto file = ss::saveCubaseDrumMap (k);
        check (file.getFileName() == "Beat Box.drm" && file.getParentDirectory() == ss::drumMapsFolder() && file.loadFileAsString().contains ("Closed Hat"),
               "saved in Documents/Song Savant/Drum Maps");

        // renaming by hand: picking another kind renumbers; a typed name is kept as it is
        {
            auto edited = k;
            for (auto& p : edited.pads)
            {
                if (p.note == 38) p.kind = "Rim";                       // was Snare
                if (p.note == 47) { p.name = "Big Floor Tom"; p.custom = true; }
            }
            ss::nameKitPads (edited);
            juce::StringArray after;
            for (auto& p : edited.pads) after.add (p.name);
            check (after.joinIntoString (", ") == "Kick, Kick 2, Rim, Closed Hat, Low Tom, Open Hat, Big Floor Tom, High Tom",
                   "renamed pads: " + after.joinIntoString (", "));
            const auto again = ss::kitMapFromJson (juce::JSON::parse (juce::JSON::toString (ss::kitMapToJson (edited))));
            check (again.pads[6].custom && again.pads[6].name == "Big Floor Tom" && ss::makeCubaseDrumMap (again).contains ("Big Floor Tom"),
                   "a typed name is kept, and goes in the drum map");
            check (ss::padKinds().contains ("Rim") && ss::padKinds().contains ("Cowbell"), "names to choose from");
        }

        // one recording stretched over several keys
        {
            const double sr = 44100.0;
            juce::Random rng (7);
            std::vector<float> rec ((size_t) sr);
            for (size_t i = 0; i < rec.size(); ++i)
            {
                const double t = (double) i / sr;
                rec[i] = (float) (std::sin (juce::MathConstants<double>::twoPi * 120.0 * t) * std::exp (-t / 0.12)
                                  + (rng.nextFloat() * 2.0f - 1.0f) * 0.5 * std::exp (-t / 0.006));
            }
            auto played = [&rec] (int semitones)       // the recording on a key `semitones` up
            {
                const double rate = std::pow (2.0, semitones / 12.0);
                std::vector<float> out;
                for (double pos = 0; pos + 1 < (double) rec.size(); pos += rate)
                {
                    const auto j = (size_t) pos;
                    out.push_back ((float) (rec[j] + (rec[j + 1] - rec[j]) * (pos - (double) j)));
                }
                return out;
            };
            const float same = ss::pitchedCopyMatch (played (11), played (12), sr);
            const float other = ss::pitchedCopyMatch (synthHit ("tom"), synthHit ("snare"), sr);
            check (same > 0.85f && other < 0.5f, "the same recording a semitone higher is spotted (" + juce::String (same, 2) + "), a different sound isn't ("
                                                 + juce::String (other, 2) + ")");

            ss::KitMap zoned;
            const char* kinds[] = { "Tom", "Tom", "Tom", "Perc", "Perc", "Snare", "Snare", "Snare", "Perc" };
            const float matches[] = { 0.0f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.2f };
            for (int i = 0; i < 9; ++i)
            {
                ss::KitPad pad;
                pad.note = 60 + i;
                pad.kind = kinds[i];
                pad.matchBelow = matches[i];
                zoned.pads.push_back (pad);
            }
            ss::groupPitchedZones (zoned);
            juce::StringArray after;
            for (auto& pad : zoned.pads) after.add (pad.kind);
            check (after.joinIntoString (",") == "Tom,Tom,Tom,Tom,Tom,Tom,Tom,Tom,Perc", "a zone is called what its lowest keys sound like: " + after.joinIntoString (","));
        }

        // picking pads to rename (the owner's video: closing the menu forgot the first pad, so shift-click did nothing)
        {
            ss::ui::RangePicker pick;
            pick.press (5, false);
            pick.menuClosed();                     // the menu for pad 5 was closed without choosing
            pick.press (12, true);                 // then shift-click on pad 12
            const bool shiftOk = pick.first == 5 && pick.last == 12;
            pick.press (20, false);                // a plain click starts again
            pick.drag (16);                        // dragging up to pad 16
            check (shiftOk && pick.first == 16 && pick.last == 20 && pick.picked (18) && ! pick.picked (21),
                   "a range: shift-click after closing the menu, or drag across pads");
        }

        ss::KitMap chromatic;                      // one 808 played up and down the keyboard
        for (int n = 24; n < 60; ++n) { ss::KitPad p; p.note = n; p.kind = "808"; chromatic.pads.push_back (p); }
        ss::nameKitPads (chromatic);
        check (chromatic.oneSound, "one sound across the keyboard is spotted (no drum map needed)");
    }

    void testKitMapping (const sstest::FakeComputer& fc)
    {
        section ("Mapping a real kit plug-in (Song Savant Scanner)");
        const juce::File kit (SS_TEST_KIT_VST3), scanner (SS_TEST_SCANNER);
        check (kit.isDirectory() && scanner.existsAsFile(), "the test kit and the scanner were built");

        // the kit's class ID, from its own info, so a preset can be made for it
        juce::String cid;
        const auto moduleInfo = ss::parseLenientJson (kit.getChildFile ("Contents/Resources/moduleinfo.json").loadFileAsString());
        if (auto* classes = moduleInfo["Classes"].getArray())
            for (auto& c : *classes)
                if (c["Category"].toString() == "Audio Module Class") cid = c["CID"].toString();
        check (cid.length() == 32, "the test kit's class ID: " + cid);

        const auto presets = fc.loc.home.getChildFile ("Documents/VST3 Presets/Grabber Audio/SS Test Kit");
        auto writePreset = [&] (const juce::String& name, const juce::String& classId, const juce::String& state)
        {
            juce::StringPairArray attrs;
            attrs.set ("PlugInName", "SS Test Kit");
            juce::MemoryBlock comp (state.toRawUTF8(), state.getNumBytesAsUTF8());
            const auto f = presets.getChildFile (name + ".vstpreset");
            presets.createDirectory();
            const auto data = ss::makeVstPreset (classId, comp, attrs);
            f.replaceWithData (data.getData(), data.getSize());
            return f;
        };
        const auto good = writePreset ("Beat Box", cid, "SSTESTKIT on");
        const auto silent = writePreset ("Switched Off", cid, "SSTESTKIT off");
        const auto wrong = writePreset ("Someone Else's", "0123456789ABCDEF0123456789ABCDEF", "SSTESTKIT on");

        auto loc = fc.loc;
        loc.vst3Folders = { kit.getParentDirectory() };
        const auto installed = ss::scanInstalledPlugins (loc);
        const auto found = ss::pluginForPreset (good, installed);
        check (found.problem.isEmpty() && found.name == "SS Test Kit" && found.file == kit, "the preset's plug-in is found: " + found.name + found.problem);
        check (ss::pluginForPreset (fc.halionPreset, installed).problem.contains ("Couldn't find"), "a preset for a plug-in you don't have says so");

       #if JUCE_WINDOWS
        _putenv_s ("SS_SCANNER", scanner.getFullPathName().toRawUTF8());
       #else
        setenv ("SS_SCANNER", scanner.getFullPathName().toRawUTF8(), 1);
       #endif
        auto map = [&] (const juce::File& preset)
        {
            ss::KitMapJob job;
            job.timeoutSeconds = 150;
            const int g = job.generation();
            juce::StringArray statuses;
            job.start (preset, loc);
            for (int i = 0; i < 3200 && job.generation() == g; ++i)
            {
                juce::Thread::sleep (50);
                statuses.addIfNotAlreadyThere (job.status().upToFirstOccurrenceOf (" (", false, false));
            }
            auto r = job.result();
            return std::make_pair (r != nullptr ? *r : ss::KitMap(), statuses);
        };

        const auto [k, statuses] = map (good);
        juce::StringArray got, zone;
        int zoneToms = 0;
        for (auto& p : k.pads)
        {
            if (p.note < 60) got.add (ss::noteName (p.note) + " " + p.kind);
            else { zone.add (ss::noteName (p.note) + " " + p.kind + " " + juce::String (p.matchBelow, 2)); if (p.kind == "Tom") ++zoneToms; }
        }
        check (k.ok() && got.joinIntoString (", ") == "B0 808, C1 Kick, D1 Snare, D#1 Clap, F#1 Closed Hat, A1 Tom, A#1 Open Hat, C2 Tom, C#2 Cymbal",
               "every pad found and heard right: " + got.joinIntoString (", ") + k.error);
        check (zone.size() == 24 && zoneToms == 24, "one tom stretched over C3 to B4 is toms all the way up: " + zone.joinIntoString (", "));
        check (k.name == "Beat Box" && k.plugin == "SS Test Kit" && ! k.oneSound, "the map is named after the preset, and it's a kit (not one sound)");
        check (statuses.joinIntoString ("|").contains ("Playing key"), "progress is shown while it plays: " + statuses.joinIntoString (" | "));
        const auto drm = ss::saveCubaseDrumMap (k);
        check (drm.existsAsFile() && drm.loadFileAsString().contains ("Tom 24"), "saved as a Cubase drum map");

        const auto off = map (silent).first;
        check (! off.ok() && off.error.contains ("No key made a sound"), "the preset is really loaded (switched off, it stays silent): " + off.error);
        const auto other = map (wrong).first;
        check (! other.ok() && other.error.contains ("isn't for"), "a preset made for another plug-in is refused: " + other.error);

       #if JUCE_WINDOWS
        _putenv_s ("SS_SCANNER", "");
       #else
        unsetenv ("SS_SCANNER");
       #endif
        presets.getParentDirectory().deleteRecursively();   // leave the fake computer as it was
    }

    void testStyleGuide()
    {
        section ("Style knowledge");
        int complete = 0, total = 0;
        juce::StringArray thin;
        for (int g = 0; g < ss::numGenres; ++g)
            for (auto* st : ss::stylesFor ((ss::Genre) g))
            {
                ++total;
                const auto guide = ss::styleGuide ((ss::Genre) g, st->id);
                const bool ok = guide.summary.isNotEmpty() && guide.tempo.isNotEmpty() && guide.sounds.size() >= 3 && guide.drums.size() >= 2
                                && guide.harmony.size() >= 2 && guide.arrangement.size() >= 1 && guide.mix.size() >= 1 && guide.tips.size() >= 1
                                && ! guide.progressions.empty() && guide.groove.lanes.count ("kick") + guide.groove.lanes.count ("tomlow") > 0;
                if (ok) ++complete; else thin.add (st->id);
            }
        check (complete == total && total == 50, "every genre and style has its knowledge: " + juce::String (complete) + " of " + juce::String (total)
                                                  + (thin.isEmpty() ? juce::String() : " (missing: " + thin.joinIntoString (", ") + ")"));
        const auto drill = ss::styleGuide (ss::Genre::trap, "trap.ukdrill");
        const auto trap = ss::styleGuide (ss::Genre::trap);
        check (drill.name == "UK Drill" && drill.bassStyle == "808glide" && drill.summary.contains ("drill") && drill.mix == trap.mix
               && drill.groove.steps == 32 && drill.groove.lanes.at ("hat").length() == 32,
               "a style adds to its genre (UK drill: its own drums and summary, trap's mix tips)");
        check (ss::styleGuide (ss::Genre::country, "country.rap").bass.joinIntoString (" ").contains ("808 goes with the drums"),
               "country rap knows the 808 goes with the drums");
    }

    void testStarterParts()
    {
        section ("Starter MIDI parts");
        auto name = [] (const juce::String& numeral, int tonic, bool minor) { auto c = ss::chordFromRoman (numeral, tonic, minor, false); return c ? c->name : juce::String ("?"); };
        check (name ("i", 9, true) == "Am" && name ("VI", 9, true) == "F" && name ("III", 9, true) == "C" && name ("VII", 9, true) == "G",
               "Roman numerals in A minor: i VI III VII = Am F C G");
        auto bVII = ss::chordFromRoman ("bVII", 0, false, false);
        check (bVII && bVII->name == "Bb" && name ("ii7", 0, false) == "Dm7" && name ("IVmaj7", 0, false) == "Fmaj7" && name ("i9", 9, true) == "Am9",
               "flats, 7ths and 9ths: bVII, ii7, IVmaj7, i9");
        auto fs = ss::chordFromName ("F#m7");
        auto gb = ss::chordFromName ("G/B");
        auto sus = ss::chordFromName ("Csus2");
        check (fs && fs->root == 6 && fs->intervals == std::vector<int> ({ 0, 3, 7, 10 }) && gb && gb->root == 7 && gb->name == "G"
               && sus && sus->pitchClasses() == std::vector<int> ({ 0, 2, 7 }) && ! ss::chordFromName ("hello"), "chord names: F#m7, G/B, Csus2");

        const auto trapBrief = ss::readBrief ("trap 142 BPM in A minor");
        const auto plan = ss::planParts (trapBrief);
        check (plan.key == 21 && std::abs (plan.bpm - 142.0) < 0.01 && plan.chords.size() == 8 && plan.chordLine() == "Am - F - C - G",
               "a trap song in A minor: " + plan.chordLine() + " at " + juce::String (plan.bpm) + " BPM");
        const auto relative = ss::planParts (ss::readBrief ("trap in C major"));
        check (relative.chordLine() == "Am - F - C - G", "a minor progression in a major key comes from the relative minor: " + relative.chordLine());
        const auto kc = ss::planParts (ss::readBrief ("country in G"), juce::StringArray ({ "G", "Em", "C", "D" }));
        check (kc.chordLine() == "G - Em - C - D" && kc.chordSource.contains ("Key Compass"), "a Key Compass song's chords are used");
        const auto next = ss::planParts (trapBrief, {}, 2);
        check (next.chordLine() != plan.chordLine(), "a new idea takes another progression: " + next.chordLine());

        // which part plays what
        juce::StringArray types;
        for (auto& part : ss::partsFor (trapBrief)) types.add (part.role + "=" + ss::partTypeName (ss::partTypeFor (part)));
        check (types.joinIntoString (", ").contains ("Drums=drums") && types.joinIntoString (", ").contains ("Bass=bass line")
               && types.joinIntoString (", ").contains ("Melody=hook idea") && types.joinIntoString (", ").contains ("Pad=held chords"),
               "each trap card gets a part: " + types.joinIntoString (", "));

        auto notesOf = [] (const juce::MidiMessageSequence& seq)
        {
            std::vector<int> v;
            for (auto* e : seq) if (e->message.isNoteOn()) v.push_back (e->message.getNoteNumber());
            return v;
        };
        const auto gm = ss::generalMidiDrums();
        const auto drums = notesOf (ss::makePart (plan, ss::PartType::drums, gm));
        const auto bass = ss::makePart (plan, ss::PartType::bass);
        const auto bassNotes = notesOf (bass);
        const auto pad = ss::makePart (plan, ss::PartType::pad);
        const auto hook = notesOf (ss::makePart (plan, ss::PartType::hook));
        check (drums.size() > 60 && std::count (drums.begin(), drums.end(), 36) >= 8 && std::count (drums.begin(), drums.end(), 39) >= 8,
               "drums: kicks and claps on the General MIDI keys (" + juce::String ((int) drums.size()) + " hits)");
        bool bassLow = ! bassNotes.empty();
        for (auto n : bassNotes) bassLow = bassLow && n >= 24 && n <= 50;
        check (bassLow && bassNotes.front() == 33, "the 808 is low and starts on the root, A (key 33)");

        // every held chord is the bar's chord
        bool padRight = true;
        int padNotes = 0;
        for (auto* e : pad)
            if (e->message.isNoteOn())
            {
                ++padNotes;
                const int bar = (int) (e->message.getTimeStamp() / (ss::ticksPerQuarter * 4));
                const auto pcs = plan.chords[(size_t) bar].pitchClasses();
                padRight = padRight && std::find (pcs.begin(), pcs.end(), e->message.getNoteNumber() % 12) != pcs.end();
            }
        check (padRight && padNotes >= 24, "the pad plays each bar's chord");
        bool inScale = ! hook.empty();
        for (auto n : hook) { const int pc = n % 12; inScale = inScale && (pc == 9 || pc == 11 || pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7); }
        check (inScale, "the trap hook stays in A minor (" + juce::String ((int) hook.size()) + " notes)");

        // every style makes every kind of part
        int styled = 0, total = 0;
        for (int g = 0; g < ss::numGenres; ++g)
            for (auto* st : ss::stylesFor ((ss::Genre) g))
            {
                ++total;
                const auto p = ss::planParts (ss::readBrief ("", (ss::Genre) g, st->id));
                bool all = true;
                for (auto t : { ss::PartType::drums, ss::PartType::bass, ss::PartType::chords, ss::PartType::pad, ss::PartType::hook })
                    all = all && ss::makePart (p, t).getNumEvents() > 0 && ss::describePart (p, t).isNotEmpty();
                if (all) ++styled;
            }
        check (styled == total, "every style makes drums, bass, chords, pads and a hook: " + juce::String (styled) + " of " + juce::String (total));
        const auto country = ss::planParts (ss::readBrief ("modern country"));
        check (ss::chordStyleFor (country, ss::Kind::guitar) == "strum" && ss::chordStyleFor (country, ss::Kind::piano) == "ballad"
               && ss::chordStyleFor (ss::planParts (ss::readBrief ("modern worship")), ss::Kind::guitar) == "pluck",
               "a guitar strums (or picks in worship), a piano doesn't strum");

        // drums from a Song Savant drum map
        ss::KitMap kit;
        kit.name = "Beat Box";
        for (auto [note, kind] : std::initializer_list<std::pair<int, const char*>> { { 24, "Kick" }, { 25, "Kick" }, { 43, "Snare" }, { 54, "Tom" },
                                                                                       { 60, "Tom" }, { 78, "Perc" } })
        {
            ss::KitPad pad1;
            pad1.note = note;
            pad1.kind = kind;
            kit.pads.push_back (pad1);
        }
        ss::nameKitPads (kit);
        const auto drm = ss::saveCubaseDrumMap (kit);
        const auto mapped = ss::drumNotesFromMap (drm);
        check (mapped && mapped->note ("kick") == 24 && mapped->note ("snare") == 43 && mapped->note ("tomlow") == 54 && mapped->note ("tomhigh") == 60
               && mapped->source.contains ("Beat Box"), "a drum map says which key is the kick, the snare and the toms");

        // the MIDI file: a tempo track and one track per part
        const auto file = ss::paths::tempDir().getChildFile ("Starter parts test.mid");
        std::vector<ss::NamedPart> parts { { "Drums", ss::makePart (plan, ss::PartType::drums) }, { "Bass", bass } };
        check (ss::writeMidiFile (plan, parts, file), "writes a MIDI file");
        juce::MidiFile back;
        juce::FileInputStream in (file);
        double bpmBack = 0;
        if (in.openedOk() && back.readFrom (in))
        {
            juce::MidiMessageSequence tempos;
            back.findAllTempoEvents (tempos);
            if (tempos.getNumEvents() > 0) bpmBack = 60.0 / tempos.getEventPointer (0)->message.getTempoSecondsPerQuarterNote();
        }
        check (back.getNumTracks() == 3 && std::abs (bpmBack - 142.0) < 0.1, "the file has a tempo track and a track per part, at 142 BPM");

        // SS_DUMP_PARTS=<folder>: write every style's parts, to look at and listen to
        if (const auto dump = juce::SystemStats::getEnvironmentVariable ("SS_DUMP_PARTS", {}); dump.isNotEmpty())
            for (int g = 0; g < ss::numGenres; ++g)
                for (auto* st : ss::stylesFor ((ss::Genre) g))
                {
                    const auto p = ss::planParts (ss::readBrief ("", (ss::Genre) g, st->id));
                    std::vector<ss::NamedPart> all;
                    for (auto t : { ss::PartType::drums, ss::PartType::bass, ss::PartType::chords, ss::PartType::pad, ss::PartType::hook })
                        all.push_back ({ ss::partTypeName (t), ss::makePart (p, t) });
                    ss::writeMidiFile (p, all, juce::File (dump).getChildFile (st->id + ".mid"));
                }
    }

    void testArrangement (const sstest::FakeComputer& fc)
    {
        section ("Arrangement map");
        std::atomic<bool> cancel { false };
        const auto cat = ss::buildCatalog (fc.loc, cancel);
        const auto brief = ss::readBrief ("trap 142 BPM in A minor");
        const auto pals = ss::makePalettes (cat, brief);
        const auto parts = ss::arrangementParts (pals[0], &cat);
        const auto plan = ss::planParts (brief);
        const ss::KeyCompassShape none;

        auto full = ss::makeArrangement (plan, parts, none, ss::SongShape::full);
        juce::StringArray names;
        for (auto& s : full.sections) names.add (s.name + " " + juce::String (s.bars));
        check (names.joinIntoString (", ") == "Intro 8, Hook 8, Verse 16, Hook 8, Verse 16, Hook 8, Outro 8" && full.totalBars() == 72,
               "a trap song's shape: " + names.joinIntoString (", "));
        juce::String drumsRole, bassRole;
        for (auto& p : parts) { if (p.type == ss::PartType::drums) drumsRole = p.role; if (p.type == ss::PartType::bass) bassRole = p.role; }
        check (full.sections[0].modeFor (drumsRole).isEmpty() && full.sections[0].modeFor (bassRole).isEmpty() && full.sections[1].modeFor (drumsRole) == "full"
               && full.sections[2].modeFor (drumsRole) == "groove", "the intro has no drums or 808; the hook has everything");

        const auto shortSong = ss::makeArrangement (plan, parts, none, ss::SongShape::shortSong);
        check (shortSong.totalBars() < full.totalBars() && shortSong.sections.size() == full.sections.size() - 2, "a short song drops the second verse and hook");

        // the whole song as MIDI: the drums only start with the hook (bar 9)
        const auto song = ss::arrangeParts (plan, full, parts, ss::generalMidiDrums());
        double firstDrum = -1;
        for (size_t i = 0; i < parts.size(); ++i)
            if (parts[i].type == ss::PartType::drums)
                for (auto* e : song[i].notes)
                    if (e->message.isNoteOn()) { firstDrum = e->message.getTimeStamp(); break; }
        check (std::abs (firstDrum - 8.0 * ss::ticksPerQuarter * 4) < 1.0, "the drums come in at the hook (bar 9)");
        const auto file = ss::paths::tempDir().getChildFile ("Whole song test.mid");
        check (ss::writeArrangement (plan, full, song, file), "writes the whole song");
        juce::MidiFile mf;
        juce::FileInputStream in (file);
        int markers = 0;
        if (in.openedOk() && mf.readFrom (in))
            for (auto* e : *mf.getTrack (0)) if (e->message.isTextMetaEvent() && e->message.getMetaEventType() == 6) ++markers;
        check (mf.getNumTracks() == (int) parts.size() + 1 && markers == 7, "a track per part and a marker per section (" + juce::String (markers) + ")");

        // clicking boxes, editing sections
        auto edited = full;
        for (auto& p : parts)
            if (p.type == ss::PartType::drums)
            {
                ss::toggleArrangementCell (edited, 0, p);
                const auto first = edited.sections[0].modeFor (p.role);
                ss::toggleArrangementCell (edited, 0, p);
                check (first == "light" && edited.sections[0].modeFor (p.role) == "groove", "the drums box steps through light, groove...");
            }
        ss::setSectionBars (edited, 0, 4);
        ss::copySection (edited, 1);
        ss::moveSection (edited, 0, 1);
        ss::removeSection (edited, 3);
        check (edited.sections[0].name == "Hook" && edited.sections[1].name == "Intro" && edited.sections[1].bars == 4 && edited.sections.size() == 7,
               "sections can be shortened, copied, moved and removed");
        const auto back = ss::Arrangement::fromXml (*edited.toXml());
        check (back.sections.size() == edited.sections.size() && back.sections[1].bars == 4 && back.sections[0].modeFor (drumsRole) == "full",
               "the map is saved with the song");

        // a Key Compass song's own sections and chords
        ss::KeyCompassShape kc { "Back Roads", { "Verse", "Chorus" }, { { "Am", "F", "C", "G" }, { "F", "G", "Am" } } };
        const auto kcArr = ss::makeArrangement (plan, parts, kc, ss::SongShape::keyCompass);
        check (kcArr.sections.size() == 2 && kcArr.sections[1].name == "Chorus" && kcArr.sections[1].modeFor (drumsRole) == "full"
               && ss::sectionChords (plan, kcArr.sections[1])[2].name == "Am" && ss::songShapeNames (kc).contains ("Key Compass: Back Roads"),
               "a Key Compass song's sections and chords");

        // the song session keeps it, and makes it again when the style changes
        ss::SongSession session;
        session.form.description = "trap 142 BPM in A minor";
        session.updateArrangement (parts);
        ss::toggleArrangementCell (session.arrangement, 0, parts[0]);
        const auto mine = session.arrangement.sections[0].play;
        session.updateArrangement (parts);
        check (session.arrangement.sections[0].play == mine, "your changes stay while the song stays the same");
        session.form.description = "modern country";
        session.updateArrangement (parts);
        check (session.arrangement.sections.size() == 8, "a new style gets its own shape");
        ss::SongSession restored;
        restored.fromXml (*session.toXml());
        check (restored.arrangement.sections.size() == 8 && restored.arrangement.signature == session.arrangement.signature, "saved and restored with the song");
    }

    void testGrooveGrabberLink (const sstest::FakeComputer& fc)
    {
        section ("Groove Grabber hand-off");
        std::atomic<bool> cancel { false };
        const auto cat = ss::buildCatalog (fc.loc, cancel);

        // the kits you saved in Groove Grabber are drum kits here
        const ss::Sound* kit = nullptr;
        for (auto& snd : cat.sounds) if (ss::isGrooveGrabberKit (snd)) kit = &snd;
        check (kit != nullptr && kit->name == "Nashville Knock" && kit->kind == ss::Kind::drums && kit->instrument == "Groove Grabber"
               && kit->library == "Country kits" && kit->yours, "a saved Groove Grabber kit is in the library");
        if (kit != nullptr)
        {
            check (kit->styles.contains ("Country") && kit->styles.contains ("Modern Nashville") && kit->styles.contains ("Today"), "its genre, style and era");
            check (kit->character.contains ("Punchy") && kit->character.contains ("Warm") && kit->character.contains ("Dry")
                   && kit->character.contains ("Live") && kit->character.contains ("808"), "its character, from Groove Grabber's sliders: "
                   + kit->character.joinIntoString (", "));
            check (ss::drumForm (*kit) == ss::DrumForm::kit && kit->description.contains ("A Major"), "a kit, tuned to its key");
            check (ss::howToLoad (*kit).contains ("MY KITS") && ss::whereToPaste (*kit).contains ("MY KITS"), "how to load it: Groove Grabber's MY KITS");

            // made for this very style, your own kit is the drums pick
            const auto brief = ss::readBrief ("modern country, warm", ss::Genre::country, "country.modern");
            const auto pals = ss::makePalettes (cat, brief);
            juce::String drumsPick;
            for (auto& slot : pals[0].slots)
                if (ss::partTypeFor (slot.part) == ss::PartType::drums && slot.current().isValid())
                    drumsPick = cat.sounds[(size_t) slot.current().sound].name;
            check (drumsPick == "Nashville Knock", "your Groove Grabber kit for the style is the drums pick, got " + drumsPick);
            check (ss::drumNotesFor (pals[0], &cat).source == "Groove Grabber's pads", "the starter drums play Groove Grabber's pads");
            ss::PartSpec drumPart;
            drumPart.role = "Drums";
            drumPart.kinds = { ss::Kind::drums };
            check (ss::scoreSound (*kit, drumPart, ss::readBrief ("pop")) + 6.0f < ss::scoreSound (*kit, drumPart, brief),
                   "a kit made for another genre counts for much less");
            for (auto& slot : pals[0].slots)
                if (ss::partTypeFor (slot.part) == ss::PartType::drums)
                    check (ss::whyItWorksPoints (cat, slot).joinIntoString ("|").contains ("Your own kit, built in Groove Grabber from your samples (made for Country)"),
                           "why it works: your own kit from Groove Grabber");
        }
        {
            const auto dir = ss::paths::tempDir().getChildFile ("gg kits");
            dir.getChildFile ("Rock/Empty.ggkit").create();
            dir.getChildFile ("Rock/Empty.ggkit").replaceWithText (R"({"name": "Empty", "genre": "rock", "slots": {}})");
            dir.getChildFile ("Rock/Broken.ggkit").replaceWithText ("{ not json");
            check (ss::readGrooveGrabberKits (dir).empty(), "an empty or damaged kit file is left out");
            dir.deleteRecursively();
        }

        // Groove Grabber's pads: the 808 plays with the kick in a style built on the 808
        {
            const auto gg = ss::grooveGrabberDrums();
            check (gg.note ("kick") == 36 && gg.note ("snare") == 38 && gg.note ("hat") == 42 && gg.note ("808") == 52 && gg.note ("tomlow") == 43,
                   "Groove Grabber's pad layout");
            const auto trap = ss::planParts (ss::readBrief ("trap 140 BPM in A minor"));
            int kicks = 0, eights = 0;
            for (auto* e : ss::makePart (trap, ss::PartType::drums, gg))
                if (e->message.isNoteOn()) { kicks += e->message.getNoteNumber() == 36; eights += e->message.getNoteNumber() == 52; }
            check (eights > 0 && eights <= kicks, "trap: the kit's 808 plays with the kick (" + juce::String (eights) + " of " + juce::String (kicks) + ")");
            check (ss::describePart (trap, ss::PartType::drums, gg).contains ("808 (E2)"), "and the part says so: " + ss::describePart (trap, ss::PartType::drums, gg));
            const auto rock = ss::planParts (ss::readBrief ("classic rock"));
            int rock808 = 0;
            for (auto* e : ss::makePart (rock, ss::PartType::drums, gg)) rock808 += e->message.isNoteOn() && e->message.getNoteNumber() == 52;
            check (rock808 == 0, "rock: no 808");
        }

        // SEND TO GROOVE GRABBER: the song as a file Groove Grabber reads
        {
            ss::SongSession song;
            song.form.description = "modern country like Morgan Wallen, warm";
            song.form.genre = (int) ss::Genre::country;
            song.form.styleId = "country.modern";
            song.form.key = 21;
            song.form.bpm = 92;
            check (ss::songNameFor (song) == "Modern Country Like Morgan Wallen", "a song name from the description: " + ss::songNameFor (song));
            song.form.fromSong = "Back Roads";
            check (ss::songNameFor (song) == "Back Roads", "or the Key Compass song's name");
            std::vector<ss::ArrangementPart> parts { { "Drums", "Drums - Nashville Knock", ss::PartType::drums, ss::Kind::drums, false } };
            song.updateArrangement (parts);

            const auto f = ss::sendToGrooveGrabber (song, "Nashville Knock");
            check (f.existsAsFile() && f.getParentDirectory() == ss::grooveGrabberSongsFolder() && f.getFileName() == "Back Roads.sssong"
                   && ss::grooveGrabberSongsFolder().getParentDirectory() == ss::paths::documentsDir(), "written to Documents/Song Savant/Songs");
            const auto v = juce::JSON::parse (f);
            check (v["app"] == "Song Savant" && v["name"] == "Back Roads" && v["genre"] == "country" && v["style"] == "country.modern"
                   && (int) v["key"] == 21 && (int) v["bpm"] == 92 && v["drums"] == "Nashville Knock", "the song: genre, style, key, tempo, drums");
            check (v["description"].toString().contains ("Morgan Wallen") && juce::Time::fromISO8601 (v["sent"].toString()).toMilliseconds() > 0,
                   "its description and when it was sent");
            auto* secs = v["sections"].getArray();
            check (secs != nullptr && secs->size() == (int) song.arrangement.sections.size() && secs->size() > 2
                   && (*secs)[0]["name"] == song.arrangement.sections[0].name && (int) (*secs)[0]["bars"] > 0, "the arrangement's sections");

            // no key given: the key the starter parts were made in
            song.form.key = -1;
            song.form.description = "worship";
            song.form.genre = -1;
            song.form.styleId = {};
            song.form.fromSong = {};
            const auto v2 = juce::JSON::parse (ss::sendToGrooveGrabber (song, {}));
            check ((int) v2["key"] == song.partsPlan().key && v2["genre"] == "worship" && v2["style"] == "" && v2["name"] == "Worship",
                   "without a key: the starter parts' key (" + v2["keyName"].toString() + ")");
        }

        // a kit you save in Groove Grabber comes back by itself, on the drum card when it was built for this song
        {
            const auto dir = ss::paths::tempDir().getChildFile ("gg kits back");
            dir.deleteRecursively();
            auto writeKit = [&dir] (const juce::String& name, const juce::String& description)
            {
                auto f = dir.getChildFile ("Trap/" + name + ".ggkit");
                f.getParentDirectory().createDirectory();
                f.replaceWithText ("{\"name\": \"" + name + "\", \"genre\": \"trap\", \"style\": \"trap.nydrill\", \"description\": \"" + description
                                   + "\", \"slots\": {\"kick\": {\"path\": \"k.wav\"}, \"808\": {\"path\": \"808.wav\"}}}");
                return f;
            };
            writeKit ("Old Faithful", "something else");
            auto base = cat;
            base.sounds.erase (std::remove_if (base.sounds.begin(), base.sounds.end(), [] (const ss::Sound& x) { return ss::isGrooveGrabberKit (x); }),
                               base.sounds.end());
            const auto since = ss::grooveGrabberKitFiles (dir);
            const auto sentAt = juce::Time::getCurrentTime();
            writeKit ("After Dark Vault", "Modern Trap");
            check (ss::grooveGrabberKitFiles (dir) != since && ss::grooveGrabberKitFiles (dir).size() == 2, "saving a kit changes what Song Savant sees in the folder");

            const auto merged = ss::withGrooveGrabberKits (base, dir);
            int kits = 0;
            for (auto& x : merged.sounds) kits += ss::isGrooveGrabberKit (x) ? 1 : 0;
            check (kits == 2 && merged.sounds.size() == base.sounds.size() + 2, "the library gets the kits without being read again");
            bool sorted = true;
            for (size_t i = 1; i < merged.sounds.size(); ++i)
                if (merged.sounds[i - 1].kind == merged.sounds[i].kind && merged.sounds[i - 1].name.compareNatural (merged.sounds[i].name) > 0) sorted = false;
            check (sorted, "and stays in order");

            ss::SongSession song;
            song.form.description = "modern trap";
            check (ss::newKitForSong (dir, since, song) == "after dark vault|groove grabber|trap / drill kits",
                   "the kit built for this song (same description) is found: " + ss::newKitForSong (dir, since, song));
            song.form.description = "dark trap";
            check (ss::newKitForSong (dir, since, song).isEmpty(), "a kit built for another song isn't");
            check (ss::newKitForSong (dir, since, song, sentAt).isNotEmpty(), "...unless it was saved after this song was sent to Groove Grabber");
            check (ss::newKitForSong (dir, ss::grooveGrabberKitFiles (dir), song, sentAt).isEmpty(), "only kits saved since the last look");
            check (ss::newKitForSong (dir, since, song, sentAt + juce::RelativeTime::seconds (60)).isEmpty(), "and not ones saved before it was sent");

            song.form.description = "modern trap";
            song.find (merged);
            const auto key = ss::newKitForSong (dir, since, song);
            check (ss::putOnDrumCard (song, merged, key) == (int) song.palettes.size(), "it goes on every palette's drum card");
            bool onCard = true;
            for (auto& pal : song.palettes)
                for (auto& slot : pal.slots)
                    if (ss::partTypeFor (slot.part) == ss::PartType::drums)
                        onCard = onCard && slot.locked && slot.current().isValid() && merged.sounds[(size_t) slot.current().sound].name == "After Dark Vault";
            check (onCard, "locked, as the card's sound");
            song.newPalettes (merged);
            juce::String kept;
            for (auto& slot : song.palettes[0].slots)
                if (ss::partTypeFor (slot.part) == ss::PartType::drums && slot.current().isValid()) kept = merged.sounds[(size_t) slot.current().sound].name;
            check (kept == "After Dark Vault", "and it stays through NEW PALETTES (got " + kept + ")");
            check (ss::drumNotesFor (song.palettes[0], &merged).source == "Groove Grabber's pads", "so the drum parts and ARRANGE use its pads");
            dir.deleteRecursively();
        }
    }

    void testMusic()
    {
        section ("Music knowledge");
        check (ss::allStyles().size() == 50, "50 styles from Groove Grabber, got " + juce::String ((int) ss::allStyles().size()));
        check (ss::stylesFor (ss::Genre::country).front()->isGeneral() && ss::stylesFor (ss::Genre::country).size() == 5, "country: General + 4 styles");
        check (ss::styleById ("country.modern") != nullptr && ss::styleById ("country.modern")->name == "Modern Nashville", "style by id");
        check (ss::genreId (ss::Genre::hiphop) == "boombap" && ss::genreFromId ("worship") == ss::Genre::worship, "genre ids match Groove Grabber");
        check (ss::allReferences().size() > 350, "artists and songs, got " + juce::String ((int) ss::allReferences().size()));

        check (ss::parseKey ("Am") == 21 && ss::parseKey ("F# major") == 6 && ss::parseKey ("in Bb") == 10 && ss::parseKey ("key of A") == 9
               && ss::parseKey ("C minor") == 12 && ss::parseKey ("in a hurry") == -1 && ss::parseKey ("Db major") == 1, "reading keys");
        check (ss::keyName (21) == "A Minor" && ss::keyName (-1) == "no key", "key names");

        auto b = ss::readBrief ("Modern country like Morgan Wallen, warm, 120 BPM in A minor");
        bool wallen = false;
        for (int r : b.references) if (ss::allReferences()[(size_t) r].name == "Morgan Wallen") wallen = true;
        check (b.genre == ss::Genre::country && b.styleId == "country.modern" && std::abs (b.bpm - 120.0) < 0.01 && b.key == 21 && wallen && b.sound.contains ("warm"),
               "a full description: genre, style, artist, BPM, key, sound (" + b.understood.joinIntoString (", ") + ")");

        b = ss::readBrief ("Elevation Worship");
        check (b.genre == ss::Genre::worship && b.styleId.startsWith ("worship") && b.bpm > 0, "an artist alone sets the genre and style: " + b.styleId);
        b = ss::readBrief ("dark trap 140bpm");
        check (b.genre == ss::Genre::trap && std::abs (b.bpm - 140.0) < 0.01 && b.sound.contains ("dark"), "genre word, BPM written together, sound word");
        b = ss::readBrief ("80s pop");
        check (b.genre == ss::Genre::pop && b.era == 2, "decade");
        b = ss::readBrief ("future bass drop");
        bool future = false;
        for (int r : b.references) if (ss::allReferences()[(size_t) r].name == "Future") future = true;
        check (! future, "an artist named like an ordinary word needs a cue");
        b = ss::readBrief ("like Morgan Wallen", ss::Genre::rock);
        check (b.genre == ss::Genre::rock, "the genre you pick wins over the artist's");
        b = ss::readBrief ("", ss::Genre::worship, "worship.gospel");
        check (b.styleId == "worship.gospel" && b.bpm > 0, "a picked style, with its tempo");

        auto parts = ss::partsFor (ss::readBrief ("warm modern country"));
        juce::StringArray roles;
        for (auto& p : parts) roles.add (p.role);
        check (roles.contains ("Drums") && roles.contains ("Bass") && roles.contains ("Rhythm 1") && roles.contains ("Lead"), "country band: " + roles.joinIntoString (", "));
        bool warmFirst = true, coldAvoided = true, fiddleOptional = false;
        for (auto& p : parts)
        {
            if (p.want[0] != "warm") warmFirst = false;
            if (! p.avoid.contains ("cold")) coldAvoided = false;
            if (p.role == "Fiddle") fiddleOptional = p.optional;
        }
        check (warmFirst && coldAvoided && fiddleOptional, "your words shape every part; extras are optional");
        auto gospel = ss::partsFor (ss::readBrief ("", ss::Genre::worship, "worship.gospel"));
        for (auto& p : gospel) if (p.role == "Keys") check (p.kinds.front() == ss::Kind::organ, "gospel keys lean on organ");
        auto countryRap = ss::partsFor (ss::readBrief ("country rap"));
        for (auto& p : countryRap)
        {
            if (p.role == "Drums") check (p.want.contains ("808") && ! p.avoid.contains ("808"), "country rap: the 808 is in the drums");
            if (p.role == "Bass") check (p.avoid.contains ("808") && p.alsoOffer.contains ("sub"), "country rap: the bass sits above the 808");
        }
        juce::StringArray trapRoles;
        for (auto& p : ss::partsFor (ss::readBrief ("modern trap"))) trapRoles.add (p.role);
        check (trapRoles.contains ("Bass") && ! trapRoles.contains ("808"), "trap has a Bass card (the 808 goes with the drums): " + trapRoles.joinIntoString (", "));
        for (int g = 0; g < ss::numGenres; ++g)
        {
            ss::SongBrief gb;
            gb.genre = (ss::Genre) g;
            auto ps = ss::partsFor (gb);
            int core = 0;
            for (auto& p : ps) if (! p.optional) ++core;
            check (core >= 5, ss::genreName ((ss::Genre) g) + " has a full band (" + juce::String (core) + " core parts)");
        }
    }

    void testBassAbove808 (const ss::Catalog& base)
    {
        // a trap bass sits above the kit's 808; deep subs are still offered
        auto cat = base;
        auto bass = [&cat] (const juce::String& name, const juce::String& detail, const juce::String& words)
        {
            ss::Sound s;
            s.name = name;
            s.instrument = "Analog Lab V";
            s.source = "Arturia";
            s.kind = ss::Kind::bass;
            s.detail = detail;
            s.searchText = " " + (name + " " + detail + " " + words).toLowerCase() + " ";
            cat.sounds.push_back (s);
        };
        bass ("Italian 808", "Sub Bass", "808 deep hard");
        bass ("Floor Sub", "Sub Bass", "deep dark");
        bass ("Night Pluck Bass", "Plucked Bass", "punchy short dark");
        bass ("Rubber Line", "Bass Line", "punchy");
        const auto brief = ss::readBrief ("modern trap");
        auto pals = ss::makePalettes (cat, brief);
        const ss::PaletteSlot* b = nullptr;
        for (auto& sl : pals[0].slots) if (sl.part.role == "Bass") b = &sl;
        check (b != nullptr && b->pick.isValid(), "trap palette has a bass");
        if (b == nullptr || ! b->pick.isValid()) return;
        auto isSub = [&cat] (const ss::Pick& x) { return cat.sounds[(size_t) x.sound].detail == "Sub Bass"; };
        int subs = 0;
        for (auto& a : b->alternatives) if (isSub (a)) ++subs;
        check (! isSub (b->pick), "the first choice sits above the 808: " + cat.sounds[(size_t) b->pick.sound].name);
        check (subs == 2, "both deep subs are still among the choices (" + juce::String (subs) + ")");
        check (ss::whyItWorksPoints (cat, *b).contains ("Sits above the 808 in your drums, leaving it the deep low end"), "why it works says why");
        for (auto& a : b->alternatives)
            if (isSub (a))
            {
                ss::PaletteSlot alt = *b;
                alt.pick = a;
                alt.alternatives.clear();
                check (ss::whyItWorksPoints (cat, alt).joinIntoString (" ").contains ("play the same notes as your 808"), "a deep sub says how to use it with the 808");
                break;
            }
    }

    void testDrumKits (const ss::Catalog& base)
    {
        // a drum part wants a whole kit you play yourself, not a loop or a single drum
        auto cat = base;
        auto drum = [&cat] (const juce::String& name, const juce::String& instrument, const juce::String& source, const juce::String& detail,
                            const juce::String& styles, const juce::String& character, bool yours, bool guessed)
        {
            ss::Sound s;
            s.name = name;
            s.instrument = instrument;
            s.source = source;
            s.kind = ss::Kind::drums;
            s.detail = detail;
            s.styles = juce::StringArray::fromTokens (styles, ",", "");
            s.character = juce::StringArray::fromTokens (character, ",", "");
            s.yours = yours;
            s.guessed = guessed;
            ss::prepareForSearch (s);
            cat.sounds.push_back (s);
            return (int) cat.sounds.size() - 1;
        };
        const int loop = drum ("Backstreet Girls", "HALion Sonic", "MediaBay", "Beats", "Urban (Hip-Hop / R&B)", "Percussive,Loop,Modern", false, false);
        const int kick = drum ("808 Kick", "Analog Lab V", "Arturia", "Kick", "Hip Hop/Trap", "Punchy", false, false);
        const int mine = drum ("DK Casette808", "Groove Agent SE", "MediaBay", "", "", "", true, true);
        const int kit = drum ("Trap Room Kit", "HALion Sonic", "MediaBay", "Drumset", "Urban (Hip-Hop / R&B)", "Modern,Processed", false, false);

        const int analogKit = drum ("Beat Box", "Analog Lab V", "Arturia", "", "Hip Hop/Trap", "Punchy", false, false);
        check (ss::drumForm (cat.sounds[(size_t) analogKit]) == ss::DrumForm::kit && ss::howToLoad (cat.sounds[(size_t) analogKit]).contains ("DRUM MAPS"),
               "an Analog Lab kit counts as a kit too (the Drum Maps page names its pads)");
        check (ss::drumForm (cat.sounds[(size_t) loop]) == ss::DrumForm::loop && ss::drumForm (cat.sounds[(size_t) kick]) == ss::DrumForm::single
               && ss::drumForm (cat.sounds[(size_t) mine]) == ss::DrumForm::kit && ss::drumForm (cat.sounds[(size_t) kit]) == ss::DrumForm::kit,
               "loops, single drums and kits are told apart");
        check (cat.sounds[(size_t) mine].searchText.contains (" 808 "), "\"Casette808\" also counts as \"808\"");

        const auto brief = ss::readBrief ("modern trap");
        auto pals = ss::makePalettes (cat, brief);
        const ss::PaletteSlot* d = nullptr;
        for (auto& sl : pals[0].slots) if (sl.part.role == "Drums") d = &sl;
        check (d != nullptr && d->pick.isValid(), "trap palette has drums");
        if (d == nullptr || ! d->pick.isValid()) return;
        auto rank = [d] (int sound)
        {
            if (d->pick.sound == sound) return 0;
            for (size_t i = 0; i < d->alternatives.size(); ++i) if (d->alternatives[i].sound == sound) return (int) i + 1;
            return 99;
        };
        check (ss::drumForm (cat.sounds[(size_t) d->pick.sound]) == ss::DrumForm::kit, "the first choice is a kit: " + cat.sounds[(size_t) d->pick.sound].name);
        juce::String scores;
        for (int i : { mine, kit, loop, kick })
            scores << cat.sounds[(size_t) i].name << " " << juce::String (ss::scoreSound (cat.sounds[(size_t) i], d->part, brief), 1) << " (#" << rank (i) << ")  ";
        check (rank (mine) < rank (loop) && rank (kit) < rank (loop) && rank (mine) < rank (kick), "your Groove Agent kit comes before loops and single drums: " + scores);
        check (ss::whyItWorksPoints (cat, *d).contains ("A full kit: you play your own beat on it"), "why it works says it's a kit");

        // sounds that fit equally well aren't picked in alphabetical order
        auto ties = base;
        for (auto* n : { "Aaa Pad", "Bbb Pad", "Ccc Pad", "Ddd Pad", "Eee Pad", "Fff Pad" })
        {
            ss::Sound s;
            s.name = n;
            s.instrument = "Padshop";
            s.source = "MediaBay";
            s.kind = ss::Kind::pad;
            s.detail = "Synth Pad";
            s.character = { "Warm", "Ambient", "Wide" };
            ss::prepareForSearch (s);
            ties.sounds.push_back (s);
        }
        juce::StringArray firstPicks;
        for (juce::uint32 seed = 1; seed <= 6; ++seed)
        {
            auto tp = ss::makePalettes (ties, ss::readBrief ("ambient worship"), { 1, 6, seed });
            for (auto& sl : tp[0].slots)
                if (sl.part.role == "Pad" && sl.pick.isValid()) firstPicks.addIfNotAlreadyThere (ties.sounds[(size_t) sl.pick.sound].name);
        }
        check (firstPicks.size() > 1, "equal sounds take turns: " + firstPicks.joinIntoString (", "));
    }

    void testPalettes (const sstest::FakeComputer& fc)
    {
        section ("Palettes");
        std::atomic<bool> cancel { false };
        auto cat = ss::buildCatalog (fc.loc, cancel);
        // (the Groove Grabber kit is tested on its own: here the library has no drum presets)
        cat.sounds.erase (std::remove_if (cat.sounds.begin(), cat.sounds.end(), [] (const ss::Sound& x) { return ss::isGrooveGrabberKit (x); }),
                          cat.sounds.end());
        testBassAbove808 (cat);
        testDrumKits (cat);
        const auto brief = ss::readBrief ("warm worship");
        const auto parts = ss::partsFor (brief);
        auto pals = ss::makePalettes (cat, brief);
        check (pals.size() == 3, "three palettes");
        for (auto& p : pals) check (p.slots.size() == parts.size(), p.name + " has a slot for every part");

        auto slotFor = [] (const ss::Palette& p, const juce::String& role) -> const ss::PaletteSlot*
        {
            for (auto& s : p.slots) if (s.part.role == role) return &s;
            return nullptr;
        };
        auto* pad = slotFor (pals[0], "Pad");
        check (pad != nullptr && pad->pick.isValid() && cat.sounds[(size_t) pad->pick.sound].name == "Heaven Pad", "the pad that's tagged Worship and warm wins");
        if (pad != nullptr)
        {
            check (pad->pick.reasons.contains ("warm") && pad->pick.reasons.contains ("tagged worship"), "reasons: " + pad->pick.reasons.joinIntoString (", "));
            check (ss::whyItWorks (cat, *pad).contains ("Tagged worship"), "why it works: " + ss::whyItWorks (cat, *pad));
        }
        auto* drums = slotFor (pals[0], "Drums");
        check (drums != nullptr && drums->pick.isValid() && cat.sounds[(size_t) drums->pick.sound].source == "Plug-in",
               "no drum presets: a drum plug-in is suggested instead");
        if (drums != nullptr && drums->pick.isValid())
        {
            const auto points = ss::whyItWorksPoints (cat, *drums);
            check (points[0] == "Drums for the Drums part" && points.contains ("Pick a kit in the plug-in"), "why it works, as a list: " + points.joinIntoString (" / "));
        }
        {
            auto many = cat;          // lots of tagged drum kits that fit even better: the plug-ins are still offered
            juce::String drumWords;
            for (auto& part : parts) if (part.role == "Drums") drumWords = part.want.joinIntoString (" ");
            for (int i = 0; i < 12; ++i)
            {
                ss::Sound kit;
                kit.name = "Worship Kit " + juce::String (i);
                kit.instrument = "Groove Agent SE";
                kit.source = "MediaBay";
                kit.kind = ss::Kind::drums;
                kit.styles.add ("Worship");
                kit.character.add ("Warm");
                kit.searchText = " worship kit groove agent se warm " + drumWords.toLowerCase() + " ";
                kit.rating = 5;
                kit.stylesText = " worship ";
                many.sounds.push_back (kit);
            }
            auto manyPals = ss::makePalettes (many, brief);
            auto* d = slotFor (manyPals[0], "Drums");
            int plugins = 0;
            if (d != nullptr)
                for (auto& a : d->alternatives) if (many.sounds[(size_t) a.sound].source == "Plug-in") ++plugins;
            juce::StringArray seen;
            if (d != nullptr) for (auto& a : d->alternatives) seen.add (many.sounds[(size_t) a.sound].name + " " + juce::String (a.score, 1));
            int fitting = 0;          // drum plug-ins that suit this song at all
            if (d != nullptr)
                for (auto& x : many.sounds)
                    if (x.source == "Plug-in" && ss::scoreSound (x, d->part, brief) > 0.0f) ++fitting;
            check (d != nullptr && d->pick.isValid() && many.sounds[(size_t) d->pick.sound].source == "MediaBay" && fitting >= 1
                   && plugins == juce::jmin (2, fitting),
                   "a real kit comes first, and the drum plug-ins are still offered: " + seen.joinIntoString (", "));
        }
        {
            auto bare = cat;          // no drum plug-ins either
            bare.sounds.erase (std::remove_if (bare.sounds.begin(), bare.sounds.end(), [] (const ss::Sound& x) { return x.source == "Plug-in"; }),
                               bare.sounds.end());
            auto barePals = ss::makePalettes (bare, brief);
            auto* none = slotFor (barePals[0], "Drums");
            check (none != nullptr && ! none->pick.isValid() && ss::whyItWorks (bare, *none).startsWith ("Nothing in your library"), "no drums at all: said plainly");
        }
        auto* keys1 = slotFor (pals[0], "Keys");
        auto* keys2 = slotFor (pals[1], "Keys");
        check (keys1 != nullptr && keys2 != nullptr && keys1->pick.isValid() && keys2->pick.isValid() && keys1->pick.sound != keys2->pick.sound,
               "each palette is a different take");
        check (keys1 != nullptr && keys1->alternatives.size() >= 6 && keys1->alternatives.size() <= 8, "alternatives for each part (6, plus up to 2 plug-ins)");
        if (keys1 != nullptr)
            for (auto& a : keys1->alternatives) check (a.sound != keys1->pick.sound, "an alternative is never the pick itself");

        // lock the keys and ask for new palettes: the keys stay
        if (keys1 != nullptr)
        {
            auto locked = *keys1;
            locked.locked = true;
            ss::PaletteOptions o;
            o.seed = 42;
            auto again = ss::makePalettes (cat, brief, o, { locked });
            bool kept = true;
            for (auto& p : again)
                if (auto* k = slotFor (p, "Keys"); k == nullptr || k->pick.sound != keys1->pick.sound || ! k->locked) kept = false;
            check (kept, "a locked part keeps its sound in every new palette");
        }
        const ss::Sound* notOwned = nullptr;
        for (auto& snd : cat.sounds) if (! snd.owned) notOwned = &snd;
        check (notOwned != nullptr && ss::scoreSound (*notOwned, parts[0], brief) < 0, "sounds you'd have to buy are never suggested");
        check (ss::findSoundByKey (cat, ss::soundKey (cat.sounds[3])) == 3, "a sound's identity survives a rebuilt catalog");

        // the song is saved and comes back the same: palettes, a locked part, a switched part
        ss::SongSession song;
        song.form.description = "warm worship";
        song.form.bpm = 70;
        song.find (cat);
        song.current = 1;
        auto& keysSlot = song.palettes[1].slots[2];
        auto& bassSlot = song.palettes[1].slots[1];
        keysSlot.cycle (2);
        keysSlot.keepKey = ss::soundKey (cat.sounds[(size_t) keysSlot.current().sound]);
        keysSlot.locked = true;
        bassSlot.cycle (1);
        bassSlot.keepKey = ss::soundKey (cat.sounds[(size_t) bassSlot.current().sound]);
        const int keysSound = keysSlot.current().sound, bassSound = bassSlot.current().sound;
        auto xml = song.toXml();
        ss::SongSession again;
        again.fromXml (*xml);
        check (again.needsRestore() && again.form.description == "warm worship" && std::abs (again.form.bpm - 70.0) < 0.01, "the form comes back");
        again.restore (cat);
        check (again.palettes.size() == 3 && again.current == 1, "the palettes come back");
        if (again.palettes.size() == 3)
        {
            check (again.palettes[1].slots[2].locked && again.palettes[1].slots[2].current().sound == keysSound, "the locked part comes back");
            check (again.palettes[1].slots[1].current().sound == bassSound, "the part you switched comes back");
            check (again.brief.bpm > 69.0 && again.brief.bpm < 71.0, "the form's BPM wins");
        }
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
        ss::KitMap beatBox;          // what a mapped kit looks like
        beatBox.name = "Beat Box";
        beatBox.plugin = "Analog Lab V";
        for (auto [note, kind] : std::initializer_list<std::pair<int, const char*>> { { 35, "808" }, { 36, "Kick" }, { 37, "Perc" }, { 38, "Snare" },
                 { 39, "Clap" }, { 40, "Snare" }, { 41, "Tom" }, { 42, "Closed Hat" }, { 43, "Tom" }, { 44, "Closed Hat" }, { 45, "Tom" },
                 { 46, "Open Hat" }, { 49, "Cymbal" }, { 51, "Cymbal" }, { 56, "Perc" } })
        {
            ss::KitPad pad;
            pad.note = note;
            pad.kind = kind;
            beatBox.pads.push_back (pad);
        }
        ss::nameKitPads (beatBox);
        auto shoot = [&] (const juce::String& name, bool withResult, int w, int h, int page, const juce::String& search = {}, const juce::String& song = {})
        {
            SongSavantProcessor proc;
            proc.locationProvider = [loc = fc.loc] { return loc; };
            proc.page = page;
            if (name.startsWith ("drum-maps-kit")) proc.kitMap.setResult (fc.loc.home.getChildFile ("Beat Box.vstpreset"), beatBox);
            std::atomic<bool> cancel { false };
            proc.catalog.setCatalog (ss::buildCatalog (fc.loc, cancel));
            if (withResult)
            {
                proc.scan.setResult (r, fc.loc);
                proc.dragTests.userPreset = 1;
            }
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            ed->setSize (w, h);
            if (auto* e = dynamic_cast<SongSavantEditor*> (ed.get()))
            {
                e->getView().getSetupCheck().refreshFromResult();
                if (search.isNotEmpty()) e->getView().getLibrary().setSearchText (search);
                e->getView().getLibrary().selectRow (0);
                if (song.isNotEmpty())
                {
                    e->getView().getSong().setDescription (song);
                    e->getView().getSong().find();
                    if (name.startsWith ("arrange"))
                    {
                        auto& ar = e->getView().getArrange();
                        ar.refresh();
                        const auto whole = ar.writeWholeSong();
                        juce::MidiFile mf;
                        juce::FileInputStream in (whole);
                        check (in.openedOk() && mf.readFrom (in) && mf.getNumTracks() >= 5 && ar.getArrangement().sections.size() >= 5,
                               "the Arrange page maps the song and writes it whole: " + whole.getFileName());
                    }
                    if (name.startsWith ("masterclass"))
                    {
                        auto& mc = e->getView().getMasterclass();
                        mc.followSong();
                        juce::StringArray titles;
                        for (auto& c : mc.getLesson().getCards()) titles.add (c.title);
                        check (titles.contains ("Drums") && titles.contains ("In your library") && titles.contains ("Chords") && titles.contains ("Listen to"),
                               "the Masterclass shows its lessons: " + titles.joinIntoString (", "));
                        if (name == "masterclass")
                        {
                            bool inKey = false;
                            for (auto& c : mc.getLesson().getCards())
                                if (c.title == "Chords") inKey = c.points.joinIntoString ("|").contains ("D - A - Bm - G");
                            check (mc.isFollowingSong() && inKey, "it follows the song (worship in D major: I - V - vi - IV = D - A - Bm - G)");
                        }
                    }
                    if (name == "song")
                    {
                        const auto all = e->getView().getSong().writeStarterParts (-1);
                        juce::MidiFile mf;
                        juce::FileInputStream in (all);
                        const bool read = in.openedOk() && mf.readFrom (in);
                        check (read && mf.getNumTracks() >= 6 && all.getFileName().contains ("BPM"),
                               "the Song page writes all its starter parts: " + all.getFileName() + ", " + juce::String (mf.getNumTracks() - 1) + " parts");
                        check (e->getView().getSong().writeStarterParts (1).existsAsFile(), "and one card's part");

                        // a kit saved in Groove Grabber for this song lands on the drum card by itself
                        auto& sp = e->getView().getSong();
                        sp.checkGrooveGrabberKits();                    // the first look
                        auto kitFile = fc.loc.grooveGrabberKits.getChildFile ("Worship/Sunday Room.ggkit");
                        kitFile.getParentDirectory().createDirectory();
                        kitFile.replaceWithText ("{\"name\": \"Sunday Room\", \"genre\": \"worship\", \"description\": \"" + song
                                                 + "\", \"slots\": {\"kick\": {\"path\": \"k.wav\"}}}");
                        const bool placed = sp.checkGrooveGrabberKits();
                        juce::String onCard;
                        if (auto* pal = proc.song.currentPalette())
                            for (auto& slot : pal->slots)
                                if (ss::partTypeFor (slot.part) == ss::PartType::drums && slot.current().isValid() && slot.locked)
                                    onCard = proc.catalog.catalog()->sounds[(size_t) slot.current().sound].name;
                        check (placed && onCard == "Sunday Room", "the Song page puts a kit saved for this song on the drum card: " + onCard);
                        kitFile.getParentDirectory().deleteRecursively();
                    }
                }
                if (name.startsWith ("manual"))
                {
                    auto& v = e->getView();
                    v.showHelp();
                    auto& man = v.getHelp().getManual();
                    juce::StringArray titles;
                    for (auto& sec : man.getSections()) titles.add (sec.title);
                    check (v.getHelp().isVisible() && titles.size() >= 18 && man.numBlocks() > 120 && titles.contains ("Working with Groove Grabber")
                           && titles.contains ("DRUM MAPS: name a kit's pads") && titles.contains ("Quick reference card"),
                           "the manual opens with its contents: " + juce::String (titles.size()) + " chapters, " + juce::String (man.numBlocks()) + " blocks");
                    check (man.setSearch ("drum map") >= 5 && man.getMatchY (0) > 0, "searching the manual finds every mention of a word");
                    check (man.setSearch ("no such words here") == 0, "and nothing when nothing matches");
                    man.setSearch ({});
                    if (name == "manual-search")
                    {
                        v.getHelp().getSearch().setText ("808", true);
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);      // the search updates as you type
                        check (man.setSearch ("808") >= 5, "typing in the search box highlights the matches");
                        v.getHelp().getSearch().setText ("808", true);
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
                    }
                    if (name == "manual-drums") v.getHelp().scrollToSection (titles.indexOf ("Working with Groove Grabber"));
                }
            }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = dir.getChildFile (name + ".png");
            f.deleteFile();
            juce::FileOutputStream out (f);
            juce::PNGImageFormat png;
            check (out.openedOk() && png.writeImageToStream (img, out), "wrote " + f.getFileName());
            std::cout << "  " << f.getFullPathName() << std::endl;
            if (name == "song")
                if (auto* e = dynamic_cast<SongSavantEditor*> (ed.get()))
                {
                    // a click on the logo switches light / dark, like the sun / moon button
                    auto& v = e->getView();
                    const auto logo = v.getLogoArea();
                    const bool before = ss::ui::isLightTheme();
                    juce::ModifierKeys mods (juce::ModifierKeys::leftButtonModifier);
                    const auto at = logo.getCentre();
                    juce::MouseEvent click (juce::Desktop::getInstance().getMainMouseSource(), at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                            &v, &v, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false);
                    v.mouseUp (click);
                    const bool switched = ss::ui::isLightTheme() != before;
                    v.mouseUp (click);
                    check (! logo.isEmpty() && logo.getX() < 60.0f && switched && ss::ui::isLightTheme() == before,
                           "a click on the logo switches light / dark (and back)");
                }
        };
        shoot ("song-empty", false, 1240, 820, 0);
        shoot ("song", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("song-small", false, 1060, 780, 0, {}, "country");
        shoot ("library", false, 1240, 820, 1);
        shoot ("library-search", false, 1240, 820, 1, "warm pad");
        shoot ("library-small", false, 1060, 780, 1);
        shoot ("arrange", false, 1240, 820, 5, {}, "trap 142 BPM in A minor");
        shoot ("arrange-small", false, 1060, 780, 5, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("arrange-empty", false, 1240, 820, 5);
        shoot ("masterclass", false, 1240, 820, 4, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("masterclass-small", false, 1060, 780, 4, {}, "uk drill 142 bpm");
        shoot ("masterclass-tall", false, 1240, 1900, 4, {}, "uk drill 142 bpm in F minor");
        shoot ("drum-maps-empty", false, 1240, 820, 3);
        shoot ("drum-maps-kit", false, 1240, 820, 3);
        shoot ("drum-maps-kit-small", false, 1060, 780, 3);
        shoot ("setup-check-empty", false, 1140, 800, 2);
        shoot ("setup-check-results", true, 1140, 800, 2);
        shoot ("setup-check-small", true, 1060, 780, 2);
        shoot ("manual", false, 1240, 820, 0);
        shoot ("manual-drums", false, 1240, 820, 0);
        shoot ("manual-search", false, 1060, 780, 0);
        ss::ui::setLightTheme (true);
        shoot ("song-light", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("manual-light", false, 1240, 820, 0);
        shoot ("library-light", false, 1240, 820, 1, "warm pad");
        shoot ("setup-check-light", true, 1140, 800, 2);
        shoot ("drum-maps-kit-light", false, 1240, 820, 3);
        shoot ("masterclass-light", false, 1240, 820, 4, {}, "modern country like Morgan Wallen in G");
        ss::ui::setLightTheme (false);
        check (! ss::ui::isLightTheme(), "back to dark");
        juce::ignoreUnused (r);
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
    testCatalog (fc);
    testPluginSuggestions (fc);
    testPluginCheck (fc);
    testKitMap();
    testKitMapping (fc);
    testMusic();
    testStyleGuide();
    testStarterParts();
    testArrangement (fc);
    testGrooveGrabberLink (fc);
    testPalettes (fc);
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
