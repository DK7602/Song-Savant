<p align="center"><img src="docs/design/logo-dark.png" width="360" alt="Song Savant"></p>

# Song Savant

**Your instruments. Your song. The right sounds.** Describe the song you're making and Song Savant looks
through the instruments and presets you already own, then suggests a complete sound palette: rhythm, bass,
leads, pads and drums, each with alternatives, plus starter MIDI parts to get the song moving.

> **Status: Phase 1d (polish), version 0.6.2.** Describe your song ("modern country like Morgan Wallen, warm,
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

## Phase 0: run the setup check

1. Download the Windows build: on GitHub open **Actions**, click the newest **Build Song Savant** run, and
   download **Song-Savant-Windows** under *Artifacts*. Unzip it and run `Song-Savant-...-Windows-Setup.exe`.
   (Windows may say "Windows protected your PC" because the app isn't signed yet: click **More info > Run anyway**.)
2. Open **Song Savant** from the Start menu and click **SCAN MY SETUP**. It only reads; it never changes or moves
   your files. It takes a minute or two.
3. Try the **drag tests** with a Cubase project open: drag each card **from Song Savant into Cubase** (onto the
   empty area under your tracks), then click *Worked* or *Didn't work* for each. You can also drag things from
   Cubase onto Song Savant: it notes what arrives.
4. Click **SAVE REPORT**. The report is saved as `Documents/Song Savant/Setup Report.txt`. Send that file back.

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
| `Source/scanner` | Song Savant Scanner: loads one plug-in in its own process and says what it is |
| `Tests` | the test runner |
| `packaging` | Windows installer (Inno Setup), macOS package |
| `docs` | roadmap and design (mockups, logo, icons) |

Credits: [JUCE](https://juce.com), [SQLite](https://sqlite.org) (public domain).
