<p align="center"><img src="docs/design/logo-dark.png" width="360" alt="Song Savant"></p>

# Song Savant

**Your instruments. Your song. The right sounds.** Describe the song you're making and Song Savant looks
through the instruments and presets you already own, then suggests a complete sound palette: rhythm, bass,
leads, pads and drums, each with alternatives, plus starter MIDI parts to get the song moving.

> **Version 1.2.0.** New in 1.2.0: the piano roll's **building tools**: a chord stamp (the song's own chords, bar by bar), strum, arpeggiate, quantize, humanize, legato, chop, flip, move in key, and a new idea for a part. New in 1.1.1: in the piano roll, velocity moves selected notes together from where each was, and a double-click deletes a note. New in 1.1.0: **the piano roll** (a card's EDIT, or double-click a box on ARRANGE): change any melodic part in any section note by note, or write your own from an empty grid, copy and paste between sections and parts, hear it looped, and drag it into your DAW; every chorus shares one pattern unless you make one different. New in 1.0.2: the setup check drops the old drag test and becomes "Something missing?" (the report to send when a sound you own doesn't show up). New in 1.0.1: a **transport** on the Arrange page (to the start, rewind, play / stop, pause, fast forward, to the end, and a bar ruler to click), and on the Song page **PLAY ALL** or a **play button on each card** for one part on its own. New in 1.0: **PLAY** your starter parts and **PLAY SONG** on the Arrange page (simple built-in sketch sounds; inside Cubase it follows the project's tempo and plays along with Cubase), and a **welcome screen**. Also: **the manual** inside the app (the **?** button or F1). **SEND TO GROOVE GRABBER** on the drum card (Groove Grabber builds kits
> for your song, tuned to its key, and the kit you save there lands on the drum card by itself), the **ARRANGE** page (your song's shape, which parts play where, and the
> whole song dragged into your DAW with a marker per section), **starter MIDI parts** (drag a card's part, or all of them, into your
> DAW, in your key and tempo), the **MASTERCLASS** page (learn any genre or style: its sounds, drums, bass, chords in
> your key, arrangement, mix and artists, plus the best matches in your own library), and the **DRUM MAPS** page
> (names every pad of a kit you saved in your DAW and writes a Cubase drum map). Describe your song ("modern country like Morgan Wallen, warm,
> 120 BPM in A minor") and Song Savant suggests a palette of sounds you already own, one card per part, with
> alternatives, locks and new takes. Instruments whose presets it can't read (SSD Sampler, FM8...) are suggested as
> the plug-in itself, and a small helper program (Song Savant Scanner) safely finds out what unlabelled plug-ins
> are. Light and dark themes (the sun / moon button). The Library holds every sound you own (Steinberg via MediaBay, Analog Lab,
> Cymatics, your own presets) in one searchable list: filter by kind of sound and instrument, see where a
> sound lives, and copy its name (Steinberg sounds: paste into MediaBay; Analog Lab and Cymatics: into the
> instrument's own browser). The Setup Check from Phase 0 is still
> there as a page. The plan and every decision so far are in [docs/ROADMAP.md](docs/ROADMAP.md).

| Format | Where it works |
|---|---|
| **VST3** | Cubase, Cakewalk, FL Studio, Ableton Live, Studio One, Reaper, Bitwig... (Windows + Mac) |
| **AU** | Logic Pro, GarageBand (Mac) |
| **Standalone app** | Windows, Mac, Linux |

## Install

1. Download the Windows installer: on GitHub open **Releases** and download `Song-Savant-...-Windows-Setup.exe`
   from the newest release (or, for a test build: **Actions**, the newest **Build Song Savant** run,
   **Song-Savant-Windows** under *Artifacts*). Run it. (Windows may say "Windows protected your PC" because the app
   isn't signed yet: click **More info > Run anyway**.)
2. Open **Song Savant** from the Start menu. The welcome screen and the manual (the **?** button) take it from there.

## The setup check (when a sound you own doesn't show up)

1. In Song Savant, click the clipboard icon in the header.
2. Click **SCAN MY SETUP**. It only reads; it never changes or moves your files. It takes a minute or two.
3. **Something missing?** The report is saved by itself as `Documents/Song Savant/Setup Report.txt` (**OPEN REPORT**,
   **SHOW FILE**, **COPY TEXT**). Send it along when you ask for help. The page lists a few things to try first.

## Building

Needs CMake 3.24+ and a C++20 compiler (Visual Studio 2022, Xcode 15+, GCC 11+). JUCE and SQLite are fetched
automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**Tests:** `SSTests` builds a fake computer (plug-ins, Steinberg presets with tags, a VST Sound container, an
Arturia-style database, a Cymatics folder, Key Compass songs, Groove Grabber kits), scans it and checks what was
found and what the report says, and runs the real Song Savant Scanner on the real plug-in. `--shots DIR` also
draws the screens to PNGs.

| Folder | |
|---|---|
| `Source/core` | scanner, preset and database readers, report, MIDI (no UI) |
| `Source/ui` | theme and screens |
| `Source/scanner` | Song Savant Scanner: opens plug-ins in their own process (what a plug-in is; mapping a drum kit) |
| `Tests` | the test runner |
| `packaging` | Windows installer (Inno Setup), macOS package |
| `docs` | roadmap and design (mockups, logo, icons) |

Credits: [JUCE](https://juce.com), [SQLite](https://sqlite.org) (public domain).
