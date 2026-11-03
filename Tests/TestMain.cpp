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
#include "core/SketchPlayer.h"
#include "core/SongSession.h"
#include "core/PatternEdit.h"
#include "core/DrumLanes.h"
#include "core/HiddenSounds.h"
#include <map>
#include <juce_audio_formats/juce_audio_formats.h>
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
        // a preset inside a library can be copied out as its own .vstpreset file (the scan itself no longer does: that
        // was for the drag test, gone in 1.0.2)
        bool copied = false;
        if (lib != nullptr && lib->example.offset >= 0)
        {
            const auto dest = ss::paths::tempDir().getChildFile ("tests/copied out.vstpreset");
            if (ss::extractEmbeddedPreset (lib->file, lib->example.offset, lib->example.size, dest))
            {
                const auto fp = ss::readVstPreset (dest);
                copied = fp.valid && fp.attributes["MusicalCategory"] == "Synth Pad";
            }
        }
        check (copied, "a preset inside a sound library can be copied out, with its tags");

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

        const auto text = ss::buildReport (r, fc.loc);
        check (text.startsWith ("SONG SAVANT - SETUP REPORT"), "title");
        check (text.contains ("[OK]  Plug-ins: 11 VST3 plug-ins, 6 of them instruments"), "plug-in summary line");
        check (! text.containsIgnoreCase ("drag test") && ! text.contains ("-> Cubase"), "no drag test in the report (gone in 1.0.2)");
        check (text.contains ("table Preset_Id (9 rows)"), "database tables listed");
        check (text.contains ("~"), "paths shortened with ~");
        check (! text.contains (fc.loc.home.getFullPathName()), "the user's home folder is never written out");
        check (! text.containsIgnoreCase ("Secret"), "nothing private-looking in the report");
        const auto h = fc.loc.home.getFullPathName();
        check (fc.loc.tidy ("url=file://46" + h.replaceCharacter ('\\', '/') + "/Documents/x") == "url=file://46~/Documents/x", "home folder hidden in file:// links");
        check (fc.loc.tidy ("p=" + h.replaceCharacter ('/', '\\') + "\\Documents") == "p=~\\Documents", "home folder hidden with either slash");
        check (text.contains ("Back Roads  |  A Minor  |  92 BPM"), "Key Compass song listed");
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
        std::vector<ss::NamedPart> parts { { "Drums", ss::makePart (plan, ss::PartType::drums), {} }, { "Bass", bass, {} } };
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
                        all.push_back ({ ss::partTypeName (t), ss::makePart (p, t), {} });
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

        // dragging a section to another place, adding sections
        {
            auto order = [] (const ss::Arrangement& a) { juce::StringArray n; for (auto& x : a.sections) n.add (x.name); return n.joinIntoString (","); };
            auto moved = full;
            ss::moveSectionTo (moved, 0, 7);                                  // the intro to the very end
            const auto toEnd = order (moved);
            ss::moveSectionTo (moved, 6, 0);                                  // and back to the start
            const auto back2 = order (moved);
            ss::moveSectionTo (moved, 2, 2);                                  // dropped where it was: nothing
            ss::moveSectionTo (moved, 2, 3);
            ss::moveSectionTo (moved, 1, 3);                                  // the first hook after the first verse
            check (toEnd == "Hook,Verse,Hook,Verse,Hook,Outro,Intro" && back2 == order (full) && order (moved) == "Intro,Verse,Hook,Hook,Verse,Hook,Outro",
                   "a section dragged to another place: " + order (moved));
            ss::addSection (moved, 1, "Verse", plan, parts);                  // one more verse: like the song's verses
            ss::addSection (moved, 0, "Bridge", plan, parts);                 // a new kind: the style's way
            ss::addSection (moved, 99, "Outro", plan, parts);                 // (past the end: at the end)
            const auto& verse = moved.sections[2];
            const auto& bridge = moved.sections[0];
            check (verse.name == "Verse" && verse.bars == full.sections[2].bars && verse.play == full.sections[2].play && verse.patternFor (bassRole) == "Verse"
                   && bridge.name == "Bridge" && bridge.bars >= 4 && bridge.progression == 2 && bridge.play.size() == parts.size()
                   && moved.sections.back().name == "Outro" && moved.sections.size() == full.sections.size() + 3,
                   "a section added: one more verse plays like the verses; a new bridge gets the style's length and parts");
            const auto inSong = ss::sectionNamesInSong (full), fresh = ss::newSectionNames (plan.guide, full);
            check (inSong.joinIntoString (",") == "Intro,Hook,Verse,Outro" && fresh.contains ("Bridge") && fresh.contains ("Pre-Chorus")
                   && ! fresh.contains ("Chorus") && ! fresh.contains ("Hook") && ! fresh.contains ("Verse"),
                   "what can be added: the song's own sections, and new kinds in the style's words (a trap song has a Hook, not a Chorus): "
                   + fresh.joinIntoString (", "));
        }

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

        const ss::ArrangementPart* bassPart = nullptr;
        for (auto& p : parts) if (p.type == ss::PartType::bass) bassPart = &p;
        const auto gm = ss::generalMidiDrums();

        // a blank song: one empty section to start from, nothing suggested, the sections you add empty too
        if (bassPart != nullptr)
        {
            auto blank = ss::makeArrangement (plan, parts, none, ss::SongShape::blank);
            bool allOff = true;
            for (auto& p : parts) allOff = allOff && blank.sections[0].modeFor (p.role).isEmpty();
            check (blank.sections.size() == 1 && blank.sections[0].bars == 8 && blank.shape == ss::SongShape::blank && allOff,
                   "a blank song: one empty 8-bar section, every part off");
            blank.sections[0].play[bassRole] = "on";
            check (ss::sectionNotes (plan, blank, 0, *bassPart, gm, nullptr).empty() && ! ss::suggestedNotes (plan, blank.sections[0], *bassPart, gm).empty(),
                   "a blank song: a part switched on plays nothing until you write it");
            ss::addSection (blank, 1, "Bridge", plan, parts);
            ss::addSection (blank, 0, "Verse", plan, parts);
            bool bridgeOff = true;
            for (auto& p : parts) bridgeOff = bridgeOff && blank.sections[2].modeFor (p.role).isEmpty();
            check (blank.sections.size() == 3 && blank.sections[2].name == "Bridge" && bridgeOff && blank.sections[0].modeFor (bassRole) == "on",
                   "a blank song: a new kind of section starts empty; one more verse plays like your verse");
            check (ss::arrangementSignature (ss::readBrief ("modern country"), {}, none, ss::SongShape::blank)
                       == ss::arrangementSignature (brief, parts, none, ss::SongShape::blank)
                   && ss::songShapeNames (none).contains ("A blank song") && ss::songShapes (none).back() == ss::SongShape::blank
                   && ss::Arrangement::fromXml (*blank.toXml()).shape == ss::SongShape::blank,
                   "a blank song stays yours when the style or the parts change; it's in the shape box; it's saved");
        }

        // your own chords: a bar of a section, in every section of that name; Song Savant's parts follow them
        if (bassPart != nullptr)
        {
            auto own = full;                                         // Intro, Hook, Verse, Hook, Verse, Hook, Outro
            const auto before = ss::sectionChords (plan, own.sections[1]);
            ss::setSectionChord (own, plan, 1, 2, "Dm7");
            const auto after = ss::sectionChords (plan, own.sections[1]);
            const auto nextHook = ss::sectionChords (plan, own.sections[3]);
            check (after.size() == before.size() && after[2].name == "Dm7" && after[0].name == before[0].name && nextHook[2].name == "Dm7"
                   && ss::hasOwnChords (own, 3) && ! ss::hasOwnChords (own, 2)
                   && ss::sectionChords (plan, own.sections[2])[2].name == ss::sectionChords (plan, full.sections[2])[2].name,
                   "your own chord for a bar: in every hook, nowhere else (" + before[2].name + " -> " + after[2].name + ")");
            bool followsD = false, beforeD = false;
            for (auto& n : ss::sectionNotes (plan, own, 1, *bassPart, gm, nullptr))
                if (n.start >= 8.0 && n.start < 12.0 && ((n.note % 12) + 12) % 12 == 2) followsD = true;
            for (auto& n : ss::sectionNotes (plan, full, 1, *bassPart, gm, nullptr))
                if (n.start >= 8.0 && n.start < 12.0 && ((n.note % 12) + 12) % 12 == 2) beforeD = true;
            check (followsD && ! beforeD, "Song Savant's bass follows your chord (a D under the Dm7)");
            const auto saved = ss::Arrangement::fromXml (*own.toXml());
            check (ss::hasOwnChords (saved, 1) && ss::sectionChords (plan, saved.sections[1])[2].name == "Dm7", "your chords are saved with the song");
            ss::setSectionChord (own, plan, 1, 2, {});
            check (ss::sectionChords (plan, own.sections[1])[2].name == before[2].name, "Song Savant's chord for that bar again");
            ss::setSectionChord (own, plan, 1, 0, "E");
            ss::clearSectionChords (own, 1);
            check (! ss::hasOwnChords (own, 1) && ! ss::hasOwnChords (own, 3) && ss::sectionChords (plan, own.sections[1])[0].name == before[0].name,
                   "all of Song Savant's chords again");
            ss::setSectionChord (own, plan, 1, 99, "E");             // (no such bar: nothing)
            check (! ss::hasOwnChords (own, 1), "a bar past the section's end changes nothing");
        }

        // a blank song keeps its own notes and map: the full song's go aside, and come back
        if (bassPart != nullptr)
        {
            ss::SongSession ses;
            ses.form.description = "trap 142 BPM in A minor";
            ses.updateArrangement (parts);
            ss::Pattern pat;
            pat.bars = 8;
            pat.notes = { { 0.0, 1.0, 45, 100 } };
            ses.storePattern (1, *bassPart, pat);
            ses.setSongShape (ss::SongShape::blank);
            ses.updateArrangement (parts);
            const bool blankNow = ses.patterns.empty() && ses.arrangement.sections.size() == 1 && ses.arrangement.shape == ss::SongShape::blank
                                  && ses.patternAt (0, *bassPart, gm).notes.empty();
            ses.storePattern (0, *bassPart, pat);
            const bool wrote = ses.isMine (0, bassRole) && ses.arrangement.sections[0].modeFor (bassRole) == "on";
            ss::SongSession reread;
            reread.fromXml (*ses.toXml());
            const bool savedBoth = reread.songShape == ss::SongShape::blank && ! reread.patterns.empty() && ! reread.patternsAside.empty()
                                   && reread.arrangementAside.sections.size() == 7 && reread.arrangement.sections.size() == 1;
            ses.setSongShape (ss::SongShape::full);
            ses.updateArrangement (parts);
            const bool fullAgain = ses.arrangement.sections.size() == 7 && ses.isMine (1, bassRole) && ! ses.patternsAside.empty();
            ses.setSongShape (ss::SongShape::blank);
            ses.updateArrangement (parts);
            check (blankNow && wrote && savedBoth && fullAgain && ses.isMine (0, bassRole) && ses.arrangement.sections.size() == 1,
                   "a blank song keeps its own map and notes; the full song's come back when you go back to it");
        }

        // a sound that isn't in your library: never suggested again (and back when you say so)
        {
            const auto firstKey = [&cat] (const ss::Palette& p) { return p.slots.size() < 2 || ! p.slots[1].current().isValid() ? juce::String()
                                                                          : ss::soundKey (cat.sounds[(size_t) p.slots[1].current().sound]); };
            auto everywhere = [&cat] (const std::vector<ss::Palette>& ps, const juce::String& key)
            {
                for (auto& p : ps)
                    for (auto& sl : p.slots)
                    {
                        if (sl.pick.isValid() && ss::soundKey (cat.sounds[(size_t) sl.pick.sound]) == key) return true;
                        for (auto& a : sl.alternatives) if (ss::soundKey (cat.sounds[(size_t) a.sound]) == key) return true;
                    }
                return false;
            };
            const auto key = firstKey (pals[0]);
            const int idx = ss::findSoundByKey (cat, key);
            if (key.isNotEmpty() && idx >= 0)
            {
                const auto& snd = cat.sounds[(size_t) idx];
                ss::SongSession ses;
                ses.palettes = pals;
                ses.forgetSound (cat, key);
                const bool gone = ! everywhere (ses.palettes, key) && ses.palettes[0].slots[1].current().isValid() == (pals[0].slots[1].numOptions() > 1);
                ss::hidden::hide (snd);
                const bool hid = ss::hidden::isHidden (snd) && ss::hidden::count() >= 1 && ss::hidden::file().existsAsFile();
                const bool notSuggested = ! everywhere (ss::makePalettes (cat, brief), key);
                ss::hidden::useFileForTests (ss::hidden::file());     // (read again from the file)
                const bool remembered = ss::hidden::isHidden (snd);
                ss::hidden::show (snd);
                check (gone && hid && notSuggested && remembered && ! ss::hidden::isHidden (snd) && everywhere (ss::makePalettes (cat, brief), key),
                       "NOT IN MY LIBRARY: \"" + snd.name + "\" leaves the palettes, isn't suggested again, is remembered, and SUGGEST IT AGAIN brings it back");
            }
            check (ss::retiredArturiaPresets().contains ("Smoker"), "Analog Lab's Smoker (gone from Analog Lab V) is left out");
        }
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

    void testSketch()
    {
        section ("Hearing the parts (PLAY: the sketch player)");
        const auto plan = ss::planParts (ss::readBrief ("trap 140 BPM in A minor"));
        std::vector<ss::ArrangementPart> parts { { "Drums", "Drums", ss::PartType::drums, ss::Kind::drums, false },
                                                 { "Bass", "Bass", ss::PartType::bass, ss::Kind::bass, false },
                                                 { "Keys", "Keys", ss::PartType::chords, ss::Kind::piano, false },
                                                 { "Pad", "Pad", ss::PartType::pad, ss::Kind::pad, false },
                                                 { "Lead", "Lead", ss::PartType::hook, ss::Kind::lead, false } };
        const auto loop = ss::sketchOfParts (plan, parts);
        bool sorted = true;
        for (size_t i = 1; i < loop.notes.size(); ++i) sorted = sorted && loop.notes[i - 1].start <= loop.notes[i].start;
        int partsWithNotes = 0;
        for (int p = 0; p < 5; ++p)
            for (auto& n : loop.notes) if (n.part == p) { ++partsWithNotes; break; }
        check (loop.loop && loop.parts.size() == 5 && partsWithNotes == 5 && sorted && std::abs (loop.lengthBeats - plan.bars * 4.0) < 1e-9
               && std::abs (loop.bpm - 140.0) < 0.01, "the starter parts as a loop: every part, in order, " + juce::String (plan.bars) + " bars at 140 BPM");

        const double sr = 48000.0, samplesPerBeat = sr * 60.0 / 140.0;
        ss::SketchPlayer p;
        p.prepare (sr);
        p.setSong (std::make_shared<const ss::SketchSong> (loop));
        juce::AudioBuffer<float> buf (2, 512);
        const ss::SketchPlayer::Host app;                     // the app: no DAW
        p.process (buf, app);
        check (buf.getMagnitude (0, 512) == 0.0f && p.getPosition() < 0.0 && ! p.isPlaying(), "silent until PLAY");
        p.play();
        float peak = 0.0f;
        const int blocks = (int) (samplesPerBeat * 8.0 / 512.0);           // two bars
        for (int i = 0; i < blocks; ++i) { p.process (buf, app); peak = juce::jmax (peak, buf.getMagnitude (0, 512)); }
        const double expected = blocks * 512.0 / samplesPerBeat;
        check (peak > 0.05f && peak <= 1.0f, "PLAY makes sound (peak " + juce::String (peak, 2) + "), never louder than full scale");
        check (std::abs (p.getPosition() - expected) < 0.01, "in time: " + juce::String (p.getPosition(), 3) + " beats after " + juce::String (expected, 3));
        for (int i = 0; i < (int) (samplesPerBeat * 26.0 / 512.0); ++i) p.process (buf, app);
        check (p.getPosition() >= 0.0 && p.getPosition() < 8.0, "the starter parts loop (back at beat " + juce::String (p.getPosition(), 2) + ")");
        p.stop();
        for (int i = 0; i < 100; ++i) p.process (buf, app);
        check (! p.isPlaying() && p.getPosition() < 0.0 && buf.getMagnitude (0, 512) < 1.0e-3f, "STOP: silent again");

        // inside a DAW: its tempo and position, and it stops when the DAW stops
        ss::SketchPlayer::Host daw;
        daw.known = true;
        daw.playing = true;
        daw.bpm = 90.0;
        daw.ppq = 16.0;
        p.play();
        p.process (buf, daw);
        check (std::abs (p.getPosition() - (16.0 + 512.0 * 90.0 / 60.0 / sr)) < 1.0e-6, "follows the DAW's position and tempo");
        daw.ppq = 300.0;
        p.process (buf, daw);
        check (std::abs (p.getPosition() - std::fmod (300.0 + 512.0 * 90.0 / 60.0 / sr, loop.lengthBeats)) < 1.0e-6, "and its jumps (looping the parts)");
        daw.playing = false;
        for (int i = 0; i < 100; ++i) p.process (buf, daw);
        check (p.getPosition() < 0.0 && buf.getMagnitude (0, 512) < 1.0e-3f, "the DAW stops: so does the sketch");
        p.stop();

        // the whole song plays once, then stops by itself
        ss::SketchSong shortSong;
        shortSong.loop = false;
        shortSong.lengthBeats = 4.0;
        shortSong.bpm = 120.0;
        shortSong.parts.push_back ({ "Bass", ss::PartType::bass, ss::Kind::bass, "Bass" });
        shortSong.notes.push_back ({ 0.0, 1.0, 45, 100, 0 });
        p.setSong (std::make_shared<const ss::SketchSong> (shortSong));
        p.play();
        for (int i = 0; i < (int) (sr * 2.5 / 512.0); ++i) p.process (buf, app);
        check (! p.isPlaying(), "the whole song plays to the end, then stops");

        const auto song = ss::sketchOfSong (plan, ss::makeArrangement (plan, parts, {}, ss::SongShape::full), parts);
        check (! song.loop && song.lengthBeats >= 32.0 && song.parts.size() == 5 && ! song.notes.empty(), "the whole song (ARRANGE): "
               + juce::String (song.lengthBeats / 4.0, 0) + " bars, played once");

        // SS_SKETCH_WAV=<file>: what PLAY sounds like, 8 bars of a few styles, to listen to
        if (const auto wav = juce::SystemStats::getEnvironmentVariable ("SS_SKETCH_WAV", {}); wav.isNotEmpty())
        {
            juce::AudioBuffer<float> all (2, 0);
            for (auto* desc : { "trap 140 BPM in A minor", "modern country 112 BPM in G", "warm worship 72 BPM in D", "90s boom bap 90 BPM in C minor" })
            {
                const auto pl = ss::planParts (ss::readBrief (desc));
                std::vector<ss::ArrangementPart> ps { { "Drums", "Drums", ss::PartType::drums, ss::Kind::drums, false },
                                                      { "Bass", "Bass", ss::PartType::bass, ss::Kind::bass, false },
                                                      { "Rhythm", "Rhythm", ss::PartType::chords, juce::String (desc).contains ("country") ? ss::Kind::guitar : ss::Kind::piano, false },
                                                      { "Pad", "Pad", ss::PartType::pad, ss::Kind::pad, false },
                                                      { "Lead", "Lead", ss::PartType::hook, ss::Kind::lead, false } };
                ss::SketchPlayer sp;
                sp.prepare (44100.0);
                sp.setSong (std::make_shared<const ss::SketchSong> (ss::sketchOfParts (pl, ps)));
                sp.play();
                const int total = (int) (44100.0 * 60.0 / pl.bpm * 4.0 * pl.bars);
                const int start = all.getNumSamples();
                all.setSize (2, start + total, true, true);
                juce::AudioBuffer<float> block (2, 512);
                for (int pos = 0; pos < total; pos += 512)
                {
                    sp.process (block, {});
                    const int n = juce::jmin (512, total - pos);
                    for (int c = 0; c < 2; ++c) all.copyFrom (c, start + pos, block, c, 0, n);
                }
            }
            juce::File f (wav);
            f.deleteFile();
            juce::WavAudioFormat fmt;
            if (auto out = std::unique_ptr<juce::AudioFormatWriter> (fmt.createWriterFor (new juce::FileOutputStream (f), 44100.0, 2, 16, {}, 0)))
                out->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
            std::cout << "  wrote " << f.getFullPathName() << std::endl;
        }

        // the song follows the DAW's tempo, unless the BPM box says otherwise
        ss::SongSession s;
        s.form.description = "worship";
        s.hostBpm = 128.0;
        check (std::abs (s.partsPlan().bpm - 128.0) < 0.01, "inside a DAW the song follows its tempo");
        s.form.bpm = 72.0;
        check (std::abs (s.partsPlan().bpm - 72.0) < 0.01, "unless the BPM box says otherwise");
    }

    void testPatterns()
    {
        section ("Your own parts: patterns (the piano roll)");
        using ss::PatternNote;
        // keys: a pattern moves with the song's key
        check (ss::keyShift (0, 9) == -3 && ss::keyShift (0, 2) == 2 && ss::keyShift (2, 0) == -2 && ss::keyShift (21, 14) == 5, "the nearest way between keys");
        std::vector<PatternNote> cMajor { { 0, 1, 60, 100 }, { 1, 1, 64, 100 }, { 2, 1, 67, 100 }, { 3, 1, 71, 100 } };   // C E G B
        const auto inD = ss::moveToKey (cMajor, 0, 2);
        check (inD[0].note == 62 && inD[1].note == 66 && inD[3].note == 73, "C major to D major: up a tone");
        const auto inCMinor = ss::moveToKey (cMajor, 0, 12);
        check (inCMinor[0].note == 60 && inCMinor[1].note == 63 && inCMinor[2].note == 67 && inCMinor[3].note == 70,
               "C major to C minor: the 3rd and 7th come down (C Eb G Bb)");
        const auto back = ss::moveToKey (inCMinor, 12, 0);
        check (back == cMajor, "and back to major");

        // a section longer than its pattern repeats it; a shorter one stops early
        ss::Pattern two;
        two.bars = 2;
        two.key = 2;
        two.notes = { { 0, 2, 50, 90 }, { 4, 4, 54, 90 }, { 9, 1, 57, 90 } };          // the last is past bar 2: kept, not played
        const auto four = ss::playPattern (two, 4, 2);
        check (four.size() == 4 && std::abs (four[2].start - 8.0) < 1e-9 && four[3].note == 54, "a 2-bar pattern in a 4-bar section plays twice");
        const auto one = ss::playPattern (two, 1, 2);
        check (one.size() == 1 && std::abs (one[0].length - 2.0) < 1e-9, "in a 1-bar section it stops early");
        const auto longer = ss::unrolled (two, 4);
        check (longer.bars == 4 && longer.notes.size() == 4, "made longer, the pattern repeats into the new bars");

        ss::PatternBook book;
        book.set ("Bass", "Chorus", two);
        const auto x = book.toXml();
        const auto again = ss::PatternBook::fromXml (*x);
        check (again == book && again.hasAny ("Bass") && ! again.hasAny ("Lead"), "your patterns are saved and read back");

        // the arrangement: one pattern per section name, unless a section is made different
        const auto plan = ss::planParts (ss::readBrief ("warm modern worship 72 BPM in D major"));
        std::vector<ss::ArrangementPart> parts { { "Drums", "Drums", ss::PartType::drums, ss::Kind::drums, false },
                                                 { "Bass", "Bass - Warm", ss::PartType::bass, ss::Kind::bass, false },
                                                 { "Keys", "Keys", ss::PartType::chords, ss::Kind::piano, false },
                                                 { "Lead", "Lead", ss::PartType::hook, ss::Kind::lead, false } };
        auto a = ss::makeArrangement (plan, parts, {}, ss::SongShape::full);
        const auto choruses = ss::sectionsUsing (a, "Bass", "Chorus");
        check (choruses.size() >= 2 && ss::patternNames (a, "Bass").contains ("Verse") && ss::mainSection (a, "Bass") == choruses.front(),
               "every Chorus shares one pattern; a card stands for the first chorus (" + juce::String ((int) choruses.size()) + " choruses)");
        const int last = choruses.back();
        const auto own = ss::makeSectionDifferent (a, last, "Bass");
        check (own == "Chorus " + juce::String ((int) choruses.size()) && a.sections[(size_t) last].patternFor ("Bass") == own
               && a.sections[(size_t) last].patternFor ("Keys") == "Chorus" && ss::sectionsUsing (a, "Bass", "Chorus").size() == choruses.size() - 1,
               "the last chorus gets its own bass pattern (" + own + "); the keys still share theirs");
        const auto ax = a.toXml();
        check (ss::Arrangement::fromXml (*ax).sections[(size_t) last].patternFor ("Bass") == own, "and the arrangement remembers it");
        ss::setPatternBars (a, "Bass", "Chorus", 16);
        bool all16 = true;
        for (int i : ss::sectionsUsing (a, "Bass", "Chorus")) all16 = all16 && a.sections[(size_t) i].bars == 16;
        check (all16 && a.sections[(size_t) last].bars != 16, "a pattern's length: every section that plays it");

        // what plays: yours where you made it, the suggestion elsewhere
        ss::PatternBook mine;
        ss::Pattern bassline;
        bassline.bars = 4;
        bassline.key = plan.key;
        bassline.notes = { { 0, 4, 38, 101 }, { 4, 4, 45, 101 }, { 8, 4, 47, 101 }, { 12, 4, 43, 101 } };
        mine.set ("Bass", "Chorus", bassline);
        const int c1 = ss::sectionsUsing (a, "Bass", "Chorus").front();
        const auto played = ss::sectionNotes (plan, a, c1, parts[1], ss::generalMidiDrums(), &mine);
        bool yours = played.size() == 16;          // 4 bars of yours, repeated through a 16-bar chorus
        for (auto& n : played) yours = yours && n.velocity == 101;
        check (yours, "a chorus plays your pattern, repeated to fill it");
        const auto verse = ss::sectionsUsing (a, "Bass", "Verse").front();
        bool suggested = false;
        for (auto& n : ss::sectionNotes (plan, a, verse, parts[1], ss::generalMidiDrums(), &mine)) suggested = suggested || n.velocity != 101;
        check (suggested, "the verse still plays the suggestion");
        const auto song = ss::arrangeParts (plan, a, parts, ss::generalMidiDrums(), &mine);
        int ours = 0;
        for (int i = 0; i < song[1].notes.getNumEvents(); ++i)
            if (song[1].notes.getEventPointer (i)->message.isNoteOn() && song[1].notes.getEventPointer (i)->message.getVelocity() == 101) ++ours;
        check (ours == 16 * (int) ss::sectionsUsing (a, "Bass", "Chorus").size(), "WHOLE SONG has your bass in every chorus that shares it (" + juce::String (ours) + " notes)");
        const auto starter = ss::starterPart (plan, a, &mine, parts[1], ss::generalMidiDrums());
        check (starter.getNumEvents() > 0 && starter.getEventPointer (0)->message.getVelocity() == 101 && ss::starterBars (plan, a, &mine, parts) == 16,
               "the Bass card plays and drags your chorus bass");
        const auto oneBox = ss::arrangePart (plan, a, parts[1], ss::generalMidiDrums(), &mine, c1);
        check (oneBox.size() == 1 && oneBox[0].notes.getStartTime() < 1.0 && oneBox[0].notes.getEndTime() <= 16 * 1920.0 + 1.0,
               "one box on its own: that part in that section, from its first beat");

        // the song keeps them: switched on where they were off, in the song's key, through a new idea
        ss::SongSession songS;
        songS.form.description = "warm modern worship 72 BPM in D major";
        songS.arrangement = ss::makeArrangement (songS.partsPlan(), parts, {}, ss::SongShape::full);
        const auto& secs = songS.arrangement.sections;
        int intro = -1;
        for (int i = 0; i < (int) secs.size(); ++i) if (secs[(size_t) i].modeFor ("Bass").isEmpty()) { intro = i; break; }
        check (intro >= 0, "a section where the bass is off (" + (intro >= 0 ? secs[(size_t) intro].name : juce::String ("none")) + ")");
        if (intro >= 0)
        {
            auto p = songS.patternAt (intro, parts[1], ss::generalMidiDrums());
            check (! songS.isMine (intro, "Bass") && ! p.notes.empty() && p.bars == secs[(size_t) intro].bars, "where it's off, the piano roll still offers the suggestion");
            p.notes = { { 0, 2, 50, 77 } };
            songS.storePattern (intro, parts[1], p);
            check (songS.isMine (intro, "Bass") && songS.arrangement.sections[(size_t) intro].modeFor ("Bass") == "on", "drawing there brings the bass in");
            ++songS.partsSeed;                    // NEW IDEA
            check (songS.patternAt (intro, parts[1], ss::generalMidiDrums()).notes.front().velocity == 77, "NEW IDEA leaves your part alone");
            songS.form.description = "warm modern worship 72 BPM in E major";
            check (songS.patternAt (intro, parts[1], ss::generalMidiDrums()).notes.front().note == 52, "a new key moves your notes with it (D to E)");
            const auto saved = songS.toXml();
            ss::SongSession reread;
            reread.fromXml (*saved);
            check (reread.isMine (intro, "Bass") && reread.patterns == songS.patterns, "your parts are saved with the song");
            songS.forgetPattern (intro, "Bass");
            check (! songS.isMine (intro, "Bass"), "RESET: back to the suggestion");
        }
        const int lastChorus = ss::sectionsUsing (songS.arrangement, "Lead", "Chorus").back();
        auto lead = songS.patternAt (lastChorus, parts[3], ss::generalMidiDrums());
        lead.notes = { { 0, 1, 74, 88 } };
        songS.storePattern (lastChorus, parts[3], lead);
        const auto ownName = songS.makeDifferent (lastChorus, parts[3], ss::generalMidiDrums());
        check (songS.patterns.find ("Lead", ownName) != nullptr && songS.patterns.find ("Lead", ownName)->notes.size() == 1,
               "made different, the section starts with a copy of your notes (" + ownName + ")");
    }

    void testPatternEditor()
    {
        section ("The piano roll's editing");
        using ss::PatternNote;
        ss::Pattern p;
        p.bars = 2;
        p.notes = { { 0, 1, 60, 100 }, { 2, 1, 64, 100 } };
        ss::PatternEditor ed;
        ed.load (p, 2);
        ed.snap = 0.25;
        ed.lastLength = 0.5;
        const int i = ed.addNote (1.37, 67);
        check (std::abs (ed.notes()[(size_t) i].n.start - 1.25) < 1e-9 && std::abs (ed.notes()[(size_t) i].n.length - 0.5) < 1e-9 && ed.numSelected() == 1,
               "a click adds a note at the snap before it, as long as the last one");
        check (ed.noteAt (1.4, 67) == i && ed.noteAt (1.9, 67) < 0 && ed.nearEnd (i, 1.72, 0.1), "finding a note under the mouse, and its end");
        ed.beginDrag();
        ed.dragMove (1.0, 2);
        check (ed.endDrag() && ed.pattern().notes.back().note == 69, "dragging moves it (and one undo puts it back)");
        ed.undo();
        check (ed.notes()[(size_t) i].n.note == 67 && std::abs (ed.notes()[(size_t) i].n.start - 1.25) < 1e-9, "undo");
        ed.redo();
        check (ed.notes()[(size_t) i].n.note == 69, "redo");
        ed.select (i, false);
        ed.beginDrag();
        ed.dragResize (1.5);
        ed.endDrag();
        check (std::abs (ed.notes()[(size_t) i].n.length - 2.0) < 1e-9 && std::abs (ed.lastLength - 2.0) < 1e-9, "dragging its end stretches it (and new notes follow)");
        ed.dragMove (100.0, 100);
        ed.endDrag();
        check (ed.notes()[(size_t) i].n.start + 1e-9 < 8.0 && ed.notes()[(size_t) i].n.note <= 127, "a note can't be dragged out of the section");
        ed.undo();

        ed.selectBox (0.0, 3.0, 59, 65, false);
        check (ed.numSelected() == 2, "a box selects the notes inside it");
        const auto clip = ed.copySelected();
        ed.paste (clip, 4.5);                    // into bar 2
        bool bar2 = false;
        for (auto& e : ed.notes()) if (e.selected && std::abs (e.n.start - 4.0) < 1e-9 && e.n.note == 60) bar2 = true;
        check (ed.notes().size() == 5 && ed.numSelected() == 2 && bar2, "Ctrl+V pastes them into the bar under the mouse, keeping their places in the bar");
        ed.selectNone();
        ed.select (0, false);
        ed.duplicateSelected();
        check (ed.notes().size() == 6 && std::abs (ed.notes().back().n.start - 1.0) < 1e-9, "Ctrl+D: a copy right after it");
        ed.transposeSelected (12);
        check (ed.notes().back().n.note == 72, "the arrow keys move selected notes up and down");
        ed.selectAll();
        ed.beginDrag();
        ed.dragVelocity (0, 60);                 // all at 100: down 40 together
        ed.endDrag();
        bool all60 = true;
        for (auto& e : ed.notes()) all60 = all60 && e.n.velocity == 60;
        ed.select (1, false);
        ed.beginDrag();
        ed.dragVelocity (1, 90);
        ed.endDrag();
        ed.selectAll();
        ed.beginDrag();
        ed.dragVelocity (0, 70);                 // up 10: the one at 90 goes to 100, the rest to 70
        ed.endDrag();
        int at70 = 0, at100 = 0;
        for (auto& e : ed.notes()) { if (e.n.velocity == 70) ++at70; if (e.n.velocity == 100) ++at100; }
        check (all60 && at100 == 1 && at70 == (int) ed.notes().size() - 1,
               "velocity: dragging one selected note's stem moves them all up or down together, keeping their differences");
        ed.eraseSelected();
        check (ed.notes().empty() && ed.canUndo(), "Delete removes them");
        ed.undo();
        check (ed.notes().size() == 6, "and undo brings them back");
        ed.clear();
        check (ed.notes().empty(), "an empty grid");

        // a section made longer: the pattern repeats into it
        ss::Pattern one;
        one.bars = 1;
        one.notes = { { 0, 1, 60, 90 } };
        ed.load (one, 1);
        ed.setVisibleBars (4);
        check (ed.bars() == 4 && ed.notes().size() == 4, "a section made longer repeats your pattern into the new bars");
        // notes past a shorter section are kept, not shown or selected
        ss::Pattern longP;
        longP.bars = 4;
        longP.notes = { { 0, 1, 60, 90 }, { 12, 1, 62, 90 } };
        ed.load (longP, 2);
        ed.selectAll();
        check (ed.numSelected() == 1 && ed.pattern().notes.size() == 2, "in a shorter section the notes after its end are kept");

        // what the roll plays: the notes on the screen, with the band or alone
        const auto plan = ss::planParts (ss::readBrief ("pop 120 BPM in C major"));
        std::vector<ss::ArrangementPart> parts { { "Bass", "Bass", ss::PartType::bass, ss::Kind::bass, false },
                                                 { "Keys", "Keys", ss::PartType::chords, ss::Kind::piano, false } };
        const auto a = ss::makeArrangement (plan, parts, {}, ss::SongShape::full);
        const std::vector<PatternNote> screen { { 0, 1, 40, 99 } };
        const int chorus = ss::sectionsUsing (a, "Bass", "Chorus").front();
        const auto sec = ss::sketchOfSection (plan, a, chorus, parts, nullptr, &screen, "Bass");
        int bassNotes = 0, keysNotes = 0;
        for (auto& n : sec.notes) (n.part == 0 ? bassNotes : keysNotes)++;
        check (sec.loop && bassNotes == 1 && keysNotes > 0 && std::abs (sec.lengthBeats - a.sections[(size_t) chorus].bars * 4.0) < 1e-9,
               "the piano roll plays the section looped: your notes, and the band");

        // light enough for a DAW: 30 seconds of five parts take a small part of 30 seconds to make (here it's about 5%)
        {
            const auto trap = ss::planParts (ss::readBrief ("trap 140 BPM in A minor"));
            std::vector<ss::ArrangementPart> five { { "Drums", "Drums", ss::PartType::drums, ss::Kind::drums, false },
                                                    { "Bass", "Bass", ss::PartType::bass, ss::Kind::bass, false },
                                                    { "Keys", "Keys", ss::PartType::chords, ss::Kind::piano, false },
                                                    { "Pad", "Pad", ss::PartType::pad, ss::Kind::pad, false },
                                                    { "Lead", "Lead", ss::PartType::hook, ss::Kind::lead, false } };
            ss::SketchPlayer sp;
            sp.prepare (48000.0);
            sp.setSong (std::make_shared<const ss::SketchSong> (ss::sketchOfParts (trap, five)));
            sp.play();
            juce::AudioBuffer<float> b (2, 512);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            const int blocks = (int) (48000.0 * 30.0 / 512.0);
            for (int k = 0; k < blocks; ++k) sp.process (b, {});
            const double share = (juce::Time::getMillisecondCounterHiRes() - t0) / 30000.0;
            check (share < 0.25, "the sketch sounds are light: 30 s of five parts took " + juce::String (share * 100.0, 1) + "% of 30 s");
        }

        // a note placed is heard straight away, playing or not
        ss::SketchPlayer player;
        player.prepare (48000.0);
        juce::AudioBuffer<float> buf (2, 480);
        player.preview (60, 100, ss::PartType::hook, ss::Kind::lead);
        float peak = 0.0f;
        for (int k = 0; k < 20; ++k) { player.process (buf, {}); peak = juce::jmax (peak, buf.getMagnitude (0, 480)); }
        check (! player.isPlaying() && peak > 0.02f, "a note you place sounds straight away (" + juce::String (peak, 3) + ")");
    }

    void testBuildingTools()
    {
        section ("The piano roll's building tools");
        using ss::PatternNote;
        ss::Pattern p;
        p.bars = 2;
        ss::PatternEditor ed;
        ed.load (p, 2);
        ed.snap = 0.25;
        ed.lastLength = 4.0;
        // chord stamp: D major at a click on F#4 (66): its root at or below (D4, 62), the rest stacked above
        check (ed.stampChord (0.1, 66, 2, { 0, 4, 7 }) == 3 && ed.numSelected() == 3, "a chord stamp places the whole chord, selected");
        juce::Array<int> notes;
        for (auto& e : ed.notes()) notes.add (e.n.note);
        check (notes.contains (62) && notes.contains (66) && notes.contains (69) && std::abs (ed.notes()[0].n.start) < 1e-9,
               "D major from D4 (62 66 69), on the beat before the click");
        // strum: low string first, each a little later, the ends kept
        ed.strum (0.05, false);
        double starts[3] {};
        for (auto& e : ed.notes()) starts[e.n.note == 62 ? 0 : e.n.note == 66 ? 1 : 2] = e.n.start;
        check (std::abs (starts[0]) < 1e-9 && std::abs (starts[1] - 0.05) < 1e-9 && std::abs (starts[2] - 0.1) < 1e-9
               && std::abs (ed.notes()[2].n.start + ed.notes()[2].n.length - 4.0) < 1e-9, "strum down: low to high, the ends stay");
        ed.undo();
        ed.strum (0.05, true);
        for (auto& e : ed.notes()) starts[e.n.note == 62 ? 0 : e.n.note == 66 ? 1 : 2] = e.n.start;
        check (std::abs (starts[2]) < 1e-9 && std::abs (starts[0] - 0.1) < 1e-9, "strum up: high to low");
        ed.undo();
        // arpeggiate: a 4-beat chord in 16ths, going up: 16 notes, D F# A D F# A...
        ed.arpeggiate (0.25, 1);
        auto pat = ed.pattern();
        check (pat.notes.size() == 16 && pat.notes[0].note == 62 && pat.notes[1].note == 66 && pat.notes[2].note == 69 && pat.notes[3].note == 62
               && std::abs (pat.notes[5].start - 1.25) < 1e-9 && std::abs (pat.notes[5].length - 0.25) < 1e-9, "arpeggiate up in 16ths");
        ed.undo();
        ed.arpeggiate (0.5, 0);
        pat = ed.pattern();
        check (pat.notes.size() == 8 && pat.notes[2].note == 69 && pat.notes[3].note == 66 && pat.notes[4].note == 62, "up and back down (in 8ths)");
        ed.undo();
        ed.arpeggiate (1.0, -1);
        pat = ed.pattern();
        check (pat.notes.size() == 4 && pat.notes[0].note == 69 && pat.notes[2].note == 62, "down, in beats");
        ed.undo();

        // quantize, humanize, legato on a loose line
        ss::Pattern loose;
        loose.bars = 1;
        loose.notes = { { 0.07, 0.3, 60, 90 }, { 1.12, 0.3, 62, 90 }, { 1.9, 0.3, 64, 90 }, { 3.03, 0.3, 65, 90 } };
        ed.load (loose, 1);
        ed.quantize (0.25, 1.0);
        pat = ed.pattern();
        check (std::abs (pat.notes[0].start) < 1e-9 && std::abs (pat.notes[1].start - 1.0) < 1e-9 && std::abs (pat.notes[2].start - 2.0) < 1e-9
               && std::abs (pat.notes[3].start - 3.0) < 1e-9, "quantize: every note onto the grid");
        ed.undo();
        ed.quantize (0.25, 0.5);
        check (std::abs (ed.pattern().notes[0].start - 0.035) < 1e-9, "quantize halfway keeps some feel");
        ed.undo();
        ed.humanize (0.02, 8, 7);
        bool moved = false, inRange = true;
        pat = ed.pattern();
        for (size_t k = 0; k < pat.notes.size(); ++k)
        {
            moved = moved || std::abs (pat.notes[k].start - loose.notes[k].start) > 1e-6 || pat.notes[k].velocity != 90;
            inRange = inRange && std::abs (pat.notes[k].start - loose.notes[k].start) <= 0.02 + 1e-9 && std::abs (pat.notes[k].velocity - 90) <= 8;
        }
        check (moved && inRange, "humanize: a little looser, never far");
        ed.undo();
        ed.legato();
        pat = ed.pattern();
        check (std::abs (pat.notes[0].length - 1.05) < 1e-9 && std::abs (pat.notes[1].length - 0.78) < 1e-9 && std::abs (pat.notes[3].length - 0.3) < 1e-9,
               "legato: each note lasts until the next one");
        ed.undo();

        // move in key (C major): C D E F up one step: D E F G; B-flat (out of key) goes to B
        ed.moveInKey (1, 0);
        pat = ed.pattern();
        check (pat.notes[0].note == 62 && pat.notes[1].note == 64 && pat.notes[2].note == 65 && pat.notes[3].note == 67, "move in key: up the scale");
        ed.moveInKey (-2, 0);
        check (ed.pattern().notes[0].note == 59, "and down (D down two steps: B)");
        ed.undo();
        ed.undo();
        // only the selected notes, when some are
        ed.select (0, false);
        ed.moveInKey (1, 9 + 12);                  // A minor
        pat = ed.pattern();
        check (pat.notes[0].note == 62 && pat.notes[1].note == 62, "a tool works on the selected notes only, when some are selected");
        ed.undo();
        ed.selectNone();

        // chop, flip
        ss::Pattern held;
        held.bars = 1;
        held.notes = { { 0, 2, 60, 90 }, { 2, 2, 67, 90 } };
        ed.load (held, 1);
        ed.chop (0.5);
        check (ed.pattern().notes.size() == 8 && std::abs (ed.pattern().notes[1].start - 0.5) < 1e-9, "chop: long notes into 8ths");
        ed.undo();
        ed.flipTime();
        pat = ed.pattern();
        check (pat.notes[0].note == 67 && std::abs (pat.notes[1].start - 2.0) < 1e-9, "flip back to front");
        ed.undo();
        ed.flipPitch (0);
        pat = ed.pattern();
        check (pat.notes[0].note == 67 && pat.notes[1].note == 60, "flip upside down (in key)");
        ed.undo();
        check (ed.pattern() == held, "and undo puts it all back");
    }

    void testDrumPatterns()
    {
        section ("Drums in the piano roll, and drum patterns between the apps");
        using ss::PatternNote;
        // the drums, by name: Groove Grabber's pads, General MIDI, and files that don't say
        check (ss::laneForNote (52) == "808" && ss::laneForNote (53) == "perc" && ss::laneForNote (54) == "shaker" && ss::laneForNote (42) == "hat",
               "Groove Grabber's pads: 52 is the 808, 53 and 54 its percussion");
        check (ss::laneForNote (52, "General MIDI") == "crash" && ss::laneForNote (70) == "shaker" && ss::laneForNote (99).isEmpty(),
               "a General MIDI file: 52 is a cymbal; notes that aren't drums are left out");
        const auto gg = ss::grooveGrabberDrums(), gm = ss::generalMidiDrums();
        const std::vector<PatternNote> onPads { { 0, 0.25, 36, 110 }, { 0, 0.25, 52, 100 }, { 1, 0.25, 38, 100 }, { 0.5, 0.25, 54, 70 } };
        const auto lanes = ss::drumsToLanes (onPads, gg);
        juce::Array<int> laneNotes;
        for (auto& n : lanes) laneNotes.add (n.note);
        check (lanes.size() == 4 && laneNotes.contains (35) && laneNotes.contains (70), "a Groove Grabber pattern kept by drum (the 808, the shaker)");
        auto backOnPads = ss::drumsFromLanes (lanes, gg);
        ss::sortNotes (backOnPads);
        auto sortedPads = onPads;
        ss::sortNotes (sortedPads);
        check (backOnPads == sortedPads, "and back on the pads, note for note");
        juce::Array<int> onGm;
        for (auto& n : ss::drumsFromLanes (lanes, gm)) onGm.add (n.note);
        check (onGm.contains (36) && onGm.contains (70) && ! onGm.contains (52) && onGm.size() == 4,
               "on a General MIDI kit the 808 plays on the kick, the shaker on GM's shaker");

        // a dropped MIDI file: its drum map, its notes from its first bar, how long it is
        const auto plan = ss::planParts (ss::readBrief ("trap 140 BPM in A minor"));
        auto dir = ss::paths::tempDir().getChildFile ("tests/drops");
        dir.createDirectory();
        const auto file = dir.getChildFile ("From Groove Grabber.mid");
        ss::NamedPart np { "Drums", ss::sequenceFromNotes (onPads, 16.0), ss::drumTag (gg) };      // starting at bar 5
        check (ss::writeMidiFile (plan, { np }, file, {}, 8), "a drum pattern written with its drum map");
        const auto dropped = ss::readMidiFile (file);
        check (dropped.has_value() && dropped->drumSource == "Groove Grabber's pads" && dropped->notes.size() == 4
               && std::abs (dropped->notes.front().start) < 1e-9 && dropped->bars == 1,
               "read back: Groove Grabber's pads, from its first bar, 1 bar long");
        check (! ss::readMidiFile (dir.getChildFile ("nothing.mid")).has_value() && ss::isMidiFile (file) && ! ss::isMidiFile (dir.getChildFile ("a.wav")),
               "only MIDI files are taken");

        // the drums' rows in the editor: up and down a drum at a time
        ss::PatternEditor ed;
        std::vector<int> rows;
        for (auto& l : ss::drumLanes()) rows.push_back (l.note);
        ed.setRows (rows);
        ss::Pattern p;
        p.bars = 2;
        p.drums = true;
        p.notes = { { 0, 0.25, 36, 100 }, { 1, 0.25, 38, 100 } };
        ed.load (p, 2);
        ed.select (0, false);
        ed.transposeSelected (1);
        check (ed.notes()[0].n.note == 37, "a kick moved up one row is the rim (the row above)");
        ed.beginDrag();
        ed.dragMove (0.0, 50);
        ed.endDrag();
        check (ed.notes()[0].n.note == 49, "dragged right up it stops on the top row (the crash)");
        ed.selectBox (0.0, 8.0, std::vector<int> { 38 }, false);
        check (ed.numSelected() == 1 && ed.notes()[1].selected, "a box over the snare's row selects the snare");
        ed.pasteOver (lanes, 1, 4.0);
        check (ed.notes().size() == 2 + 4, "a dropped pattern goes into the bar it lands on (bar 2)");
        ed.pasteOver (lanes, 1, 0.0);
        check (ed.notes().size() == 4 + 4, "dropped on bar 1, it replaces what was there");

        // the song: a drum pattern is kept by drum, plays on the drum card's kit, and never moves with the key
        std::vector<ss::ArrangementPart> parts { { "Drums", "Drums - Nashville Knock", ss::PartType::drums, ss::Kind::drums, false },
                                                 { "Bass", "Bass", ss::PartType::bass, ss::Kind::bass, false } };
        ss::SongSession song;
        song.form.description = "trap 140 BPM in A minor";
        song.arrangement = ss::makeArrangement (song.partsPlan(), parts, {}, ss::SongShape::full);
        const int chorus = ss::mainSection (song.arrangement, "Drums");
        auto drums = song.patternAt (chorus, parts[0], gg);
        bool byDrum = drums.drums && ! drums.notes.empty();
        for (auto& n : drums.notes) byDrum = byDrum && ss::laneName (n.note).isNotEmpty();
        check (byDrum, "the drum card's suggestion opens kept by drum (" + juce::String ((int) drums.notes.size()) + " hits)");
        drums.notes = lanes;
        song.storePattern (chorus, parts[0], drums);
        song.form.description = "trap 140 BPM in C minor";
        check (song.patternAt (chorus, parts[0], gg).notes == lanes, "a new key leaves the drums alone");
        const auto onKit = ss::sectionNotes (song.partsPlan(), song.arrangement, chorus, parts[0], gg, &song.patterns);
        juce::Array<int> kitNotes;
        for (auto& n : onKit) kitNotes.add (n.note);
        check (kitNotes.contains (52) && kitNotes.contains (54), "ARRANGE and WHOLE SONG play your drums on the kit's pads (808 on 52)");
        const auto whole = ss::arrangePart (song.partsPlan(), song.arrangement, parts[0], gg, &song.patterns);
        check (whole.front().text == "drums: Groove Grabber's pads", "a drum track says which drum map it's on (for Groove Grabber to read)");
        dir.deleteRecursively();
    }

    void testTransport()
    {
        section ("The transport (ARRANGE) and one part on its own (SONG)");
        const double sr = 48000.0;
        juce::AudioBuffer<float> buf (2, 480);
        const ss::SketchPlayer::Host app;
        auto loudness = [&] (ss::SketchPlayer& p, int blocks)
        {
            float peak = 0.0f;
            for (int i = 0; i < blocks; ++i) { p.process (buf, app); peak = juce::jmax (peak, buf.getMagnitude (0, buf.getNumSamples())); }
            return peak;
        };

        // two parts that take turns: the bass on beats 0-2, the keys on beats 2-4 (120 BPM: a beat is 0.5 s, 50 blocks)
        ss::SketchSong turns;
        turns.loop = true;
        turns.lengthBeats = 4.0;
        turns.bpm = 120.0;
        turns.parts.push_back ({ "Bass", ss::PartType::bass, ss::Kind::bass, "Bass" });
        turns.parts.push_back ({ "Keys", ss::PartType::pad, ss::Kind::pad, "Keys" });
        turns.notes.push_back ({ 0.0, 1.9, 45, 110, 0 });
        turns.notes.push_back ({ 2.0, 1.9, 69, 110, 1 });
        ss::SketchPlayer p;
        p.prepare (sr);
        p.setSong (std::make_shared<const ss::SketchSong> (turns));

        p.setSolo (1);
        p.play();
        const float soloQuiet = loudness (p, 90);            // beats 0-1.8: only the bass plays here, and it's left out
        const float soloKeys = loudness (p, 80);             // beats 2-3.6: the keys
        check (soloQuiet < 1.0e-3f && soloKeys > 0.02f, "one part on its own: only that part sounds (" + juce::String (soloQuiet, 4) + " / " + juce::String (soloKeys, 3) + ")");
        p.setSolo (-1);
        p.play();
        check (loudness (p, 90) > 0.05f, "and every part again");
        loudness (p, 35);                                    // beat 2.5: the keys hold their note
        p.setSolo (0);                                       // the bass on its own (it has nothing until the loop comes round)
        const float cut = loudness (p, 30), after = loudness (p, 30);
        check (after < cut * 0.5f, "switching to one part while it plays: the others fade out (" + juce::String (cut, 3) + " to " + juce::String (after, 3) + ")");
        p.stop();
        p.setSolo (-1);
        loudness (p, 50);

        // starting part-way through: a note that's already held sounds from there on (a pad, a long bass note)
        p.play (1.0);                                        // halfway through the bass note
        const float held = loudness (p, 20);
        check (held > 0.02f && std::abs (p.getPosition() - (1.0 + 20 * 480.0 / 24000.0)) < 1.0e-6,
               "PLAY from beat 1: in time from there (" + juce::String (p.getPosition(), 3) + "), with the bass note it lands in (" + juce::String (held, 3) + ")");
        p.stop();
        loudness (p, 50);

        // the ARRANGE page's transport
        SongSavantProcessor proc;
        proc.sketch.prepare (sr);
        ss::SketchSong song;
        song.loop = false;
        song.lengthBeats = 32.0;                              // 8 bars
        song.bpm = 120.0;
        song.parts.push_back ({ "Bass", ss::PartType::bass, ss::Kind::bass, "Bass" });
        for (int b = 0; b < 32; ++b) song.notes.push_back ({ (double) b, 0.5, 45, 100, 0 });
        proc.updateSketch (SongSavantProcessor::SketchMode::song, std::make_shared<const ss::SketchSong> (song));
        auto run = [&] (double beats) { for (int i = 0; i < juce::roundToInt (beats * 24000.0 / 480.0); ++i) proc.sketch.process (buf, app); };
        const auto mode = SongSavantProcessor::SketchMode::song;

        check (proc.songPosition() < 1.0e-9 && ! proc.isSketchPlaying (mode), "the playhead waits at the start");
        proc.songStep (1);
        proc.songStep (1);
        check (std::abs (proc.songPosition() - 8.0) < 1.0e-9, "fast forward: a bar at a time (bar 3)");
        proc.songStep (-1);
        check (std::abs (proc.songPosition() - 4.0) < 1.0e-9, "rewind: a bar back (bar 2)");
        proc.songPlay();
        run (6.0);
        check (proc.isSketchPlaying (mode) && std::abs (proc.songPosition() - 10.0) < 0.02, "PLAY plays from the playhead (beat " + juce::String (proc.songPosition(), 2) + ")");
        proc.songPause();
        run (2.0);
        check (! proc.isSketchPlaying (mode) && std::abs (proc.songPosition() - 10.0) < 0.02, "PAUSE stops where it is");
        proc.songPlay();
        run (2.0);
        check (std::abs (proc.songPosition() - 12.0) < 0.02, "and PLAY carries on from there");
        proc.songStop();
        run (1.0);
        check (! proc.isSketchPlaying (mode) && std::abs (proc.songPosition() - 10.0) < 1.0e-9, "STOP goes back to where it started playing");
        proc.songPlay();
        run (1.1);
        proc.songStep (-1);                                   // most of the way through bar 3 (beat 11.1): back to where it starts
        check (std::abs (proc.songPosition() - 8.0) < 0.02, "rewind while it plays: back to the bar line (" + juce::String (proc.songPosition(), 2) + ")");
        proc.songStep (-1);                                   // right on it: the bar before
        run (0.5);
        check (proc.isSketchPlaying (mode) && std::abs (proc.songPosition() - 4.5) < 0.02, "again: the bar before, and it carries on playing (" + juce::String (proc.songPosition(), 2) + ")");
        proc.songMoveTo (0.0);
        run (0.5);
        check (std::abs (proc.songPosition() - 0.5) < 0.02, "to the start while it plays: from the top");
        proc.songMoveTo (proc.songLength());
        run (0.5);
        check (! proc.isSketchPlaying (mode) && std::abs (proc.songPosition() - 32.0) < 1.0e-9, "to the end: the song's over, the playhead waits at the end");
        proc.songPlay();
        run (1.0);
        check (proc.isSketchPlaying (mode) && std::abs (proc.songPosition() - 1.0) < 0.02, "PLAY at the end plays from the top");
        proc.songMoveTo (28.0);
        run (5.0);
        check (! proc.isSketchPlaying (mode) && std::abs (proc.songPosition() - 28.0) < 1.0e-9, "the song plays to its end, then the playhead goes back");

        // the Song page's PLAY ALL and one card: the loop, every part or one
        auto loop = turns;
        proc.updateSketch (SongSavantProcessor::SketchMode::loop, std::make_shared<const ss::SketchSong> (loop));
        check (proc.sketchHasPart ("Keys") && ! proc.sketchHasPart ("FX") && ! proc.setSketchSolo ("FX"), "a card with no starter part has nothing to play");
        check (proc.setSketchSolo ("Keys"), "one card's part");
        proc.playSketch (SongSavantProcessor::SketchMode::loop);
        check (proc.sketch.getSolo() == 1, "plays that part on its own");
        proc.playSketch (mode);
        check (proc.sketch.getSolo() == -1, "while the whole song plays every part");
        proc.stopSketch();
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
        ss::ui::writeFlag (ss::ui::WelcomePanel::seenKey, true);     // the welcome only where it's wanted
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
            }
            if (name.startsWith ("welcome")) ss::ui::writeFlag (ss::ui::WelcomePanel::seenKey, false);   // as on the first run
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            ed->setSize (w, h);
            if (name.startsWith ("welcome")) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
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
                    if ((name.startsWith ("piano-roll") && ! name.startsWith ("piano-roll-drums")) || name == "arrange-yours" || name == "song-yours")
                    {
                        // the Bass card's EDIT: the piano roll opens on its first chorus
                        auto& v = e->getView();
                        const bool opened = v.showPianoRoll ("Bass", -1);
                        auto& roll = v.getPianoRoll();
                        auto& pe = roll.getEditor();
                        const auto before = pe.notes().size();
                        const auto& a = proc.song.arrangement;
                        const bool chorus = roll.getSection() >= 0 && a.sections[(size_t) roll.getSection()].name.containsIgnoreCase ("chorus");
                        check (opened && roll.isVisible() && before > 0 && chorus && ! v.getSong().isVisible(),
                               "EDIT on the Bass card opens the piano roll on its chorus, with the suggestion in it (" + juce::String ((int) before) + " notes)");
                        pe.lastLength = 2.0;
                        pe.addNote (12.0, 45);                       // an A in bar 4
                        roll.edited();
                        check (proc.song.isMine (roll.getSection(), "Bass") && pe.notes().size() == before + 1, "a note added: the chorus bass is yours now");
                        pe.selectBox (16.0, 24.0, 20, 70, false);    // bars 5 and 6 selected
                        const auto midi = roll.writeMidi();
                        check (midi.existsAsFile() && midi.getFileName().contains ("Bass - Chorus"), "its drag chip: " + midi.getFileName());
                        roll.togglePlay();
                        check (proc.isSketchPlaying (SongSavantProcessor::SketchMode::section), "play: the section, looped");
                        roll.togglePlay();
                        if (name.startsWith ("piano-roll-tools"))
                        {
                            // the building tools, through the page: open the keys, stamp the bar's chord, strum it
                            v.showPianoRoll ("Keys", roll.getSection());
                            auto& ke = roll.getEditor();
                            ke.clear();
                            roll.edited();
                            ke.lastLength = 4.0;
                            roll.setStamp (true);
                            roll.stampAt (0.0, 62);                       // bar 1 (D): D F# A from D4
                            juce::Array<int> stamped;
                            for (auto& n : ke.notes()) stamped.add (n.n.note);
                            check (ke.notes().size() == 3 && stamped.contains (62) && stamped.contains (66) && stamped.contains (69),
                                   "the stamp places the bar's own chord (D in bar 1)");
                            roll.stampAt (4.0, 61);                       // bar 2 (A): A from the A below
                            roll.shape (1, 1);                            // strum the selected chord, gently
                            bool strummed = false;
                            for (auto& n : ke.notes()) strummed = strummed || (n.selected && n.n.start > 4.001);
                            check (ke.notes().size() == 6 && strummed, "STRUM spreads the chord just stamped");
                            roll.pickStampChord (2);                      // the song's second chord, from the chips
                            roll.stampAt (8.0, 64);
                            check (ke.notes().size() == 9 && roll.numStampChords() >= 3, "a chord from the chips");
                            ke.selectNone();
                            roll.shape (2, 2);                            // arpeggiate everything in 16ths
                            check (ke.notes().size() > 30, "ARPEGGIATE breaks every chord up (" + juce::String ((int) ke.notes().size()) + " notes)");
                            roll.shape (3, 1);
                            roll.setStamp (false);
                            check (proc.song.isMine (roll.getSection(), "Keys"), "the keys are yours, with every tool kept with the song");
                            const auto before2 = ke.pattern();
                            roll.newIdea();
                            check (ke.pattern().notes.size() > 0 && ! (ke.pattern() == before2) && ke.canUndo(), "A NEW IDEA FOR THIS PART: another take (undo brings yours back)");
                            ke.undo();
                            roll.edited();
                            ke.selectBox (4.0, 8.0, 0, 127, false);
                        }
                        if (name == "arrange-yours" || name == "song-yours")
                        {
                            roll.makeDifferent (ss::sectionsUsing (a, "Bass", "Chorus").back());
                            v.showPage (name == "arrange-yours" ? 5 : 0);
                            if (name == "arrange-yours") v.getArrange().refresh();
                            check (! roll.isVisible(), "DONE: back to the page");
                            if (name == "arrange-yours")
                            {
                                const auto bit = v.getArrange().writeBit (roll.getSection(), 1);
                                check (bit.existsAsFile() && bit.getFileName().contains ("Bass - Chorus (bar"), "one box drags as MIDI: " + bit.getFileName());
                            }
                        }
                    }
                    if (name.startsWith ("piano-roll-drums"))
                    {
                        // the drum card's EDIT: the drums by name; then a Groove Grabber pattern dropped on bar 3
                        auto& v = e->getView();
                        const bool opened = v.showPianoRoll ("Drums", -1);
                        auto& roll = v.getPianoRoll();
                        auto& de = roll.getEditor();
                        bool byDrum = ! de.notes().empty();
                        for (auto& n : de.notes()) byDrum = byDrum && ss::laneName (n.n.note).isNotEmpty();
                        check (opened && roll.isDrums() && de.getRows().size() == ss::drumLanes().size() && byDrum,
                               "the drums open in the piano roll, a row per drum (" + juce::String ((int) de.notes().size()) + " hits)");
                        const auto pads = ss::grooveGrabberDrums();
                        std::vector<ss::PatternNote> groove;
                        for (int k = 0; k < 8; ++k) groove.push_back ({ k * 0.5, 0.25, 42, k % 2 == 0 ? 100 : 70 });
                        for (double b : { 0.0, 2.5 }) groove.push_back ({ b, 0.25, 36, 110 });
                        groove.push_back ({ 0.0, 0.5, 52, 100 });
                        groove.push_back ({ 1.0, 0.25, 38, 110 });
                        groove.push_back ({ 3.0, 0.25, 38, 110 });
                        const auto f = ss::paths::tempDir().getChildFile ("tests/Dropped groove.mid");
                        f.getParentDirectory().createDirectory();
                        ss::writeMidiFile (proc.song.partsPlan(), { ss::NamedPart { "Groove", ss::sequenceFromNotes (groove), ss::drumTag (pads) } }, f, {}, 1);
                        const bool dropped = roll.dropFile (f, 8.5);
                        int at808 = 0;
                        for (auto& n : de.notes()) if (n.n.note == 35 && n.n.start >= 8.0 && n.n.start < 12.0) ++at808;
                        check (dropped && at808 == 1 && roll.getStatus().startsWith ("Dropped"), "a Groove Grabber pattern dropped on bar 3, its 808 and all: " + roll.getStatus());
                        check (proc.song.isMine (roll.getSection(), "Drums"), "the drums are yours now");
                        const auto midi = roll.writeMidi();
                        const auto back = ss::readMidiFile (midi);
                        check (back.has_value() && back->drumSource.isNotEmpty(), "the drums' drag tag says which drum map it's on: " + (back.has_value() ? back->drumSource : juce::String()));
                        f.deleteFile();
                    }
                    if (name == "arrange-moving")
                    {
                        // ARRANGE: drag a section to another place, add and delete sections; UNDO and REDO each step
                        auto& ar = e->getView().getArrange();
                        ar.refresh();
                        auto xml = [] (const ss::Arrangement& a) { return a.toXml()->toString(); };
                        const auto start = ar.getArrangement();
                        const auto n = start.sections.size();
                        ar.moveSectionTo (1, 3);                                  // the first hook, after the first verse
                        const bool moved = ar.getArrangement().sections[1].name == "Verse" && ar.getArrangement().sections[2].name == "Hook";
                        ar.addSectionAt (5, "Bridge");
                        ar.addSectionAt (0, "Verse");
                        const bool added = ar.getArrangement().sections.size() == n + 2 && ar.getArrangement().sections[6].name == "Bridge"
                                           && ar.getArrangement().sections[0].name == "Verse";
                        ar.deleteSection ((int) ar.getArrangement().sections.size() - 1);
                        const auto edited = xml (ar.getArrangement());
                        const bool deleted = ar.getArrangement().sections.size() == n + 1 && ar.getArrangement().sections.back().name == "Hook";
                        for (int i = 0; i < 4; ++i) ar.undo();
                        const bool undone = xml (ar.getArrangement()) == xml (start) && ! ar.canUndo() && ar.canRedo();
                        for (int i = 0; i < 4; ++i) ar.redo();
                        const bool redone = xml (ar.getArrangement()) == edited && ! ar.canRedo() && ar.canUndo();
                        check (moved && added && deleted && undone && redone,
                               "ARRANGE: a section moved, sections added and deleted; UNDO takes each back, REDO puts it back");
                        // bars added to a section and taken off (every part goes with it); one step each
                        const int bars0 = ar.getArrangement().sections[1].bars;
                        ar.changeBars (1, 1);
                        const int more = ar.getArrangement().sections[1].bars;
                        ar.changeBars (1, -4);
                        const int fewer = ar.getArrangement().sections[1].bars;
                        ar.changeBars (1, -999);                                   // (never below a bar)
                        const int least = ar.getArrangement().sections[1].bars;
                        ar.undo();
                        ar.undo();
                        ar.undo();
                        check (more == bars0 + 1 && fewer == bars0 - 3 && least == 1 && ar.getArrangement().sections[1].bars == bars0,
                               "ARRANGE: bars added and removed (" + juce::String (bars0) + " -> " + juce::String (more) + " -> " + juce::String (fewer)
                               + " -> " + juce::String (least) + "), and UNDO puts the length back");
                        // the map shows where a dragged section lands: the place between sections nearest the mouse
                        auto& grid = ar.getGrid();
                        ar.resized();
                        const int gap = grid.gapAt (grid.xAt (4.0 * 41.0) + 1.0f);  // just past the start of bar 42
                        check (gap >= 0 && gap <= (int) ar.getArrangement().sections.size(), "dragging a section: it lands between sections (" + juce::String (gap) + ")");
                        grid.showMove (2, 5, grid.xAt (4.0 * 50.0));
                    }
                    if (name == "arrange-blank")
                    {
                        auto& v = e->getView();
                        auto& sp = v.getSong();
                        // NOT IN MY LIBRARY on the selected card: the next choice takes its place, and it isn't suggested again
                        {
                            const auto* palette = proc.song.currentPalette();
                            const auto cat = proc.catalog.catalog();
                            const int before = palette != nullptr && ! palette->slots.empty() ? palette->slots.front().current().sound : -1;
                            sp.hideSelected();
                            const int after = palette != nullptr ? palette->slots.front().current().sound : -1;
                            const bool hid = before >= 0 && cat != nullptr && ss::hidden::isHidden (cat->sounds[(size_t) before]) && after != before;
                            if (before >= 0 && cat != nullptr) ss::hidden::show (cat->sounds[(size_t) before]);
                            check (hid, "the SONG page's NOT IN MY LIBRARY: the card moves on to the next choice and remembers");
                        }
                        // BLANK SONG: ARRANGE, with one empty section; add your own; your own chords and notes in the piano roll
                        v.getSong().onBlankSong();
                        auto& ar = v.getArrange();
                        ar.refresh();
                        const bool blank = ar.isVisible() && proc.song.songShape == ss::SongShape::blank && ar.getArrangement().sections.size() == 1;
                        ar.addSectionAt (1, "Hook");
                        ar.addSectionAt (0, "Intro");
                        ar.changeBars (0, -4);
                        const bool shaped = ar.getArrangement().sections.size() == 3 && ar.getArrangement().sections[2].name == "Hook"
                                            && ar.getArrangement().sections[0].bars == 4;
                        const bool opened = v.showPianoRoll ("Bass", 2);
                        auto& roll = v.getPianoRoll();
                        const bool emptyGrid = opened && roll.getEditor().notes().empty();
                        roll.pickChord (0, "Dm");
                        roll.pickChord (1, "Bb");
                        roll.getEditor().addNote (0.0, 38);
                        roll.getEditor().addNote (4.0, 46);
                        roll.edited();
                        const auto chords = ss::sectionChords (proc.song.partsPlan(), proc.song.arrangement.sections[2]);
                        const bool ownChords = chords.size() >= 2 && chords[0].name == "Dm" && chords[1].name == "Bb";
                        const bool wrote = proc.song.isMine (2, "Bass") && proc.song.arrangement.sections[2].modeFor ("Bass") == "on";
                        {
                            // (a picture of the piano roll here, before going back to ARRANGE)
                            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                            auto f = dir.getChildFile ("piano-roll-blank.png");
                            f.deleteFile();
                            juce::FileOutputStream out (f);
                            juce::PNGImageFormat png;
                            check (out.openedOk() && png.writeImageToStream (img, out), "wrote piano-roll-blank.png");
                        }
                        roll.onDone();
                        v.showPage (5);
                        ar.refresh();
                        check (blank && shaped && emptyGrid && ownChords && wrote,
                               "BLANK SONG: one empty section; sections added; an empty grid; your own chords (Dm, Bb) and notes; the bass comes in");
                    }
                    if (name == "arrange-scrolled")
                    {
                        auto& ar = e->getView().getArrange();
                        ar.refresh();
                        for (int i = 0; i < 4; ++i) ar.addSectionAt (1, "Verse");   // a long song: 96 bars
                        ar.resized();
                        proc.songMoveTo (4.0 * 92.0);                 // bar 93: off the right of a small window
                        ar.togglePlay();
                        auto* vp = ar.findChildWithID ("map");
                        check (vp != nullptr && dynamic_cast<juce::Viewport*> (vp)->getViewPositionX() > 0,
                               "a long song in a small window: the map scrolls to keep the playhead in sight");
                    }
                    if (name == "arrange-playing")
                    {
                        auto& ar = e->getView().getArrange();
                        ar.refresh();
                        proc.songMoveTo (4.0 * 16.0);                 // bar 17: the first verse
                        ar.togglePlay();
                        const auto where = ar.positionText();
                        check (proc.isSketchPlaying (SongSavantProcessor::SketchMode::song) && where.startsWith ("Bar 17 of 72") && where.contains ("Verse"),
                               "ARRANGE plays from the playhead and shows where it is: " + where);
                    }
                    if (name == "song-playing")
                    {
                        auto& sp = e->getView().getSong();
                        const auto loop = SongSavantProcessor::SketchMode::loop;
                        sp.playPart (1);
                        const bool alone = proc.isSketchPlaying (loop) && proc.getSketchSolo() == "Bass";
                        sp.playAll();
                        const bool all = proc.isSketchPlaying (loop) && proc.getSketchSolo().isEmpty();
                        sp.playAll();
                        const bool stopped = ! proc.isSketchPlaying (loop);
                        sp.playPart (1);
                        check (alone && all && stopped && proc.getSketchSolo() == "Bass",
                               "the Song page: a card's play button plays its part on its own, PLAY ALL every part, and STOP stops");
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

                        // PLAY and STOP
                        e->getView().getSong().playAll();
                        const bool started = proc.isSketchPlaying (SongSavantProcessor::SketchMode::loop);
                        e->getView().getSong().playAll();
                        check (started && ! proc.sketch.isPlaying(), "the Song page's PLAY plays the starter parts, and STOP stops them");

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
                    check (v.getHelp().isVisible() && titles.size() >= 18 && man.numBlocks() > 120 && titles.contains ("Working with Groove Grabber") && titles.contains ("The piano roll: your own parts")
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
            if (name.startsWith ("welcome"))
                if (auto* e = dynamic_cast<SongSavantEditor*> (ed.get()))
                {
                    auto& wp = e->getView().getWelcome();
                    const bool shown = wp.isVisible();
                    wp.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
                    check (shown && ! wp.isVisible() && ss::ui::readFlag (ss::ui::WelcomePanel::seenKey),
                           "the first time it opens, the welcome shows; LET'S GO (or Esc) closes it for good");
                }
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
        shoot ("arrange-scrolled", false, 1060, 780, 5, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("arrange-empty", false, 1240, 820, 5);
        shoot ("arrange-playing", false, 1240, 820, 5, {}, "trap 142 BPM in A minor");
        shoot ("arrange-moving", false, 1240, 820, 5, {}, "trap 142 BPM in A minor");
        shoot ("arrange-blank", false, 1240, 820, 5, {}, "trap 142 BPM in A minor");
        shoot ("piano-roll", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("piano-roll-small", false, 1060, 780, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("piano-roll-tools", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("piano-roll-drums", false, 1240, 820, 0, {}, "trap 142 BPM in A minor");
        shoot ("piano-roll-tools-small", false, 1060, 780, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("arrange-yours", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("song-yours", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("song-playing", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("masterclass", false, 1240, 820, 4, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("masterclass-small", false, 1060, 780, 4, {}, "uk drill 142 bpm");
        shoot ("masterclass-tall", false, 1240, 1900, 4, {}, "uk drill 142 bpm in F minor");
        shoot ("drum-maps-empty", false, 1240, 820, 3);
        shoot ("drum-maps-kit", false, 1240, 820, 3);
        shoot ("drum-maps-kit-small", false, 1060, 780, 3);
        shoot ("setup-check-empty", false, 1140, 800, 2);
        shoot ("setup-check-results", true, 1140, 800, 2);
        shoot ("setup-check-small", true, 1060, 780, 2);
        shoot ("welcome", false, 1240, 820, 0);
        shoot ("welcome-small", false, 1060, 780, 0);
        shoot ("manual", false, 1240, 820, 0);
        shoot ("manual-drums", false, 1240, 820, 0);
        shoot ("manual-search", false, 1060, 780, 0);
        ss::ui::setLightTheme (true);
        shoot ("song-light", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("manual-light", false, 1240, 820, 0);
        shoot ("welcome-light", false, 1240, 820, 0);
        shoot ("library-light", false, 1240, 820, 1, "warm pad");
        shoot ("setup-check-light", true, 1140, 800, 2);
        shoot ("drum-maps-kit-light", false, 1240, 820, 3);
        shoot ("masterclass-light", false, 1240, 820, 4, {}, "modern country like Morgan Wallen in G");
        shoot ("arrange-light", false, 1240, 820, 5, {}, "trap 142 BPM in A minor");
        shoot ("piano-roll-light", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
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
    testSketch();
    testTransport();
    testPatterns();
    testPatternEditor();
    testBuildingTools();
    testDrumPatterns();
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
        check (ss::buildReport (real, loc).isNotEmpty(), "report for this computer");
    }

    std::cout << "\n" << checks << " checks, " << failures << " failed" << std::endl;
    if (failures == 0) home.deleteRecursively();
    return failures == 0 ? 0 : 1;
}
