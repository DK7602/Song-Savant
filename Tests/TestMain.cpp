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
#include <map>
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
        check (vst3 == 6, "6 VST3 plug-ins, got " + juce::String (vst3));
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
            check (presetTable != nullptr && presetTable->rows == 6 && presetTable->sampleRows.size() == 3, "Preset_Id: row count and sample rows");
            if (presetTable != nullptr && ! presetTable->sampleRows.isEmpty())
                check (presetTable->sampleRows[0].contains ("name=Classic Jun Keys") && presetTable->sampleRows[0].contains ("..."), "sample row text (cut at 14 columns)");
            check (insight (arturia, "Presets by instrument").contains ("Analog Lab | 5"), "Arturia: presets by instrument");
            check (insight (arturia, "Presets by type").contains ("Keys | 1"), "Arturia: presets by type");
            check (insight (arturia, "Characteristics, genres and styles").contains ("Characteristics | Warm | 3"), "Arturia: characteristics");
            check (insight (arturia, "Genres").contains ("2 | Worship"), "Arturia: genres");
            check (insight (arturia, "Analog Lab factory presets by instrument").contains ("Analog Lab | Analog Lab V | 4"), "Arturia: factory presets by instrument");
            check (insight (arturia, "Packs (name | factory | presets)").contains ("Analog Lab Factory | 1 | 5"), "Arturia: packs");
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
        check (text.contains ("[OK]  Plug-ins: 6 VST3 plug-ins, 4 of them instruments"), "plug-in summary line");
        check (text.contains ("Preset file -> Cubase:           WORKED"), "drag test answers");
        check (text.contains ("MIDI clip -> Cubase:             DIDN'T WORK"), "drag test answers (2)");
        check (text.contains ("table Preset_Id (6 rows)"), "database tables listed");
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
            if (it.title == "Arturia (Analog Lab)") arturiaRow = it.status == 1 && it.detail.contains ("6 presets, 2 instruments");
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

        auto ar = ss::readArturia (fc.arturiaDb, problem);
        check (ar.size() == 5, "Arturia: 5 presets (hidden one left out), got " + juce::String ((int) ar.size()));
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
        check (cat.sources.size() == 3 && cat.sounds.size() == 51 + 5 + 3, "catalog: 3 sources, 59 sounds, got " + juce::String ((int) cat.sounds.size()));
        check (cat.sounds.front().kind == ss::Kind::keys || cat.sounds.front().kind == ss::Kind::piano, "sorted by kind");
        check (cat.instruments().contains ("Analog Lab V") && cat.instruments().contains ("HALion Sonic") && cat.instruments().contains ("Cymatics Plinko"), "instruments");

        ss::SoundFilter f;
        f.text = "warm pad";
        auto hits = ss::findSounds (cat, f);
        check (hits.size() == 1 && cat.sounds[(size_t) hits[0]].name == "Heaven Pad", "search: every word must match");
        f.text = "";
        f.kinds = { ss::Kind::bass };
        hits = ss::findSounds (cat, f);
        check (hits.size() == 3, "filter by kind (owned only): 3 basses, got " + juce::String ((int) hits.size()));
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
        check (ss::findSounds (cat, f).size() == 3, "your own presets");

        check (ss::howToLoad (*keys0).contains ("All Media") && ss::howToLoad (*pad).contains ("Analog Lab V")
               && ss::howToLoad (*byName["Jupiter Brass"]).startsWith ("Not owned"), "how to load");
        check (ss::whereToPaste (*keys0).contains ("MediaBay") && ss::whereToPaste (*pad).contains ("Analog Lab's search")
               && ss::whereToPaste (*cyByName["Glass Bell"]).contains ("Cymatics Plinko's preset list"), "where to paste depends on the library");
        check (ss::guessKind ("Bassoon Solo") == ss::Kind::woodwinds && ss::guessKind ("Deep Sub 808 Bass") == ss::Kind::bass
               && ss::guessKind ("Kick 808") == ss::Kind::drums && ss::guessKind ("Warm Rhodes") == ss::Kind::keys, "guessing a kind from words");
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
        for (auto& p : countryRap) if (p.role == "Bass") check (p.want.contains ("808"), "country rap bass is an 808");
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

    void testPalettes (const sstest::FakeComputer& fc)
    {
        section ("Palettes");
        std::atomic<bool> cancel { false };
        const auto cat = ss::buildCatalog (fc.loc, cancel);
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
        check (drums != nullptr && ! drums->pick.isValid() && ss::whyItWorks (cat, *drums).startsWith ("Nothing in your library"), "no drums in this library: said plainly");
        auto* keys1 = slotFor (pals[0], "Keys");
        auto* keys2 = slotFor (pals[1], "Keys");
        check (keys1 != nullptr && keys2 != nullptr && keys1->pick.isValid() && keys2->pick.isValid() && keys1->pick.sound != keys2->pick.sound,
               "each palette is a different take");
        check (keys1 != nullptr && keys1->alternatives.size() == 6, "alternatives for each part");
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
        auto shoot = [&] (const juce::String& name, bool withResult, int w, int h, int page, const juce::String& search = {}, const juce::String& song = {})
        {
            SongSavantProcessor proc;
            proc.locationProvider = [loc = fc.loc] { return loc; };
            proc.page = page;
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
                }
            }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = dir.getChildFile (name + ".png");
            f.deleteFile();
            juce::FileOutputStream out (f);
            juce::PNGImageFormat png;
            check (out.openedOk() && png.writeImageToStream (img, out), "wrote " + f.getFileName());
            std::cout << "  " << f.getFullPathName() << std::endl;
        };
        shoot ("song-empty", false, 1240, 820, 0);
        shoot ("song", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("song-small", false, 1060, 780, 0, {}, "country");
        shoot ("library", false, 1240, 820, 1);
        shoot ("library-search", false, 1240, 820, 1, "warm pad");
        shoot ("library-small", false, 1060, 780, 1);
        shoot ("setup-check-empty", false, 1140, 800, 2);
        shoot ("setup-check-results", true, 1140, 800, 2);
        shoot ("setup-check-small", true, 1060, 780, 2);
        ss::ui::setLightTheme (true);
        shoot ("song-light", false, 1240, 820, 0, {}, "warm modern worship like Elevation Worship, 72 BPM in D major");
        shoot ("library-light", false, 1240, 820, 1, "warm pad");
        shoot ("setup-check-light", true, 1140, 800, 2);
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
    testMusic();
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
