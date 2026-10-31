# Song Savant: Roadmap and Decisions

This file records every decision made while planning Song Savant, so nothing is lost between work sessions.
**Read this first before working on the project.** When a decision changes, update this file in the same commit.

---

## What Song Savant is

A songwriting and sound coach. You describe the song you're making (genre, style, mood, energy, BPM, key, and
optionally a reference artist or song). Song Savant looks through **the instruments and presets you already own**
and suggests a complete sound palette, one sound per part (rhythm, bass, leads, pads, drums, and so on), with
alternatives for every part. Then it helps you build the song: starter MIDI parts, which parts play in each
section, and getting the sounds into your DAW.

Owner: DK7602. Sibling apps by the same owner: **Groove Grabber** (drum kits from your own samples,
https://github.com/DK7602/Groove-Grabber), **Key Compass** (songs with key, tempo, sections and chords) and
**Sample Snagger** (samples and stem splitting).

---

## Decisions

### 1. Same foundation as Groove Grabber: one codebase, app and plugin
- C++ with **JUCE 8**, built with CMake, the same as Groove Grabber.
- One codebase produces the **standalone app** and the **plugin** (VST3 on Windows/Mac/Linux, AU on Mac, LV2 on Linux).
- The standalone app is the main place to browse and build palettes early on. The plugin becomes really
  useful in **Phase 2** (it follows the DAW's tempo, can play the starter parts in sync, and saves the palette
  inside the DAW project).
- The UI is drawn with JUCE (like Groove Grabber), not a web view. Gold ornaments and the owl and logo are image files.
- GitHub Actions builds Windows, macOS and Linux on every push, runs the tests and makes installers.
- Windows is the main platform (the owner uses Windows and Cubase Artist 14). Mac comes along with the same code.

### 2. Works in every popular DAW
- **VST3** covers Cubase, Cakewalk (Sonar / by BandLab), FL Studio, Ableton Live, Studio One, Reaper, Bitwig.
- **AU** covers Logic Pro and GarageBand on Mac.
- **Pro Tools** needs AAX (an Avid developer account and PACE signing). Later, only if customers ask.
- **Dragging MIDI** into the DAW works everywhere.
- **Loading a sound** is DAW-dependent. Cubase gets the best experience: a `.vstpreset` dragged into the
  project creates an instrument track with that sound (to be confirmed by the Phase 0 drag test). Other DAWs:
  drag the MIDI and use "copy preset name" or load the preset file in the instrument's own browser.

### 3. Built-in music knowledge, not cloud AI, for version 1
- No internet needed, no per-use cost, no accounts or billing. This is the same approach as Groove Grabber, which
  already understands genres, styles, decades, artists and songs offline.
- Reuse Groove Grabber's genres, styles, decades and artist knowledge (it already knows Luke Combs,
  Morgan Wallen, Chris Stapleton, Elevation Worship, Maverick City, Hillsong and hundreds more). Song Savant adds
  **instrumentation**: for each style, which parts a song uses and what each part should sound like
  (instrument types plus character words such as warm, bright, wide, gritty).
- The genre and style list should be **the same in both apps**. Long term, the style and artist code is a
  shared module used by both.
- An **optional online AI mode** (free-typed requests, richer explanations) can come later (Phase 4).

### 4. Three levels of instrument support
No program can write a custom reader for every plugin, so:
1. **Full:** every preset is read with its tags. Steinberg first (`.vstpreset` files with MediaBay tags, factory
   content in `.vstsound` containers), then **Analog Lab Pro** (Arturia's preset database) if the Phase 0
   test shows it can be read. Other formats join this level over time (e.g. Vital's JSON presets, NI's NKS).
2. **Plugin-level:** Song Savant knows what a plugin is good at and suggests the plugin itself
   ("try Cymatics Plinko for Lead 2"), and you choose the preset in the plugin. This covers the long tail (the
   owner's Cymatics plugins and most plugins on the market). The list of known plugins grows over time.
3. **Unknown:** listed, and you can tag it yourself.

**The app never suggests a preset you don't own.** Suggestions always come from the scanned catalog.
Tags guessed from a preset's name or category are shown as guesses.

### 5. Drums and the sibling apps
- **Drums are a part in every palette.**
- The drum slot hands the song (genre, style, key, BPM, reference artist) to **Groove Grabber**, which builds the
  kits. Saved Groove Grabber kits (`Documents/Groove Grabber/Kits/<genre>/*.ggkit`, JSON) show up in Song Savant.
  Groove Agent kits, SSD Sampler 5, MT-PowerDrumKit and other drum instruments appear as alternatives.
- **The owner has approved changing Groove Grabber** so it can receive a song from Song Savant (Phase 2). That
  work happens in the Groove Grabber repo.
- Song Savant reads **Key Compass** songs the same way Groove Grabber does
  (`%APPDATA%/KeyCompass/Songs/*.kcsong`, XML: `KeyCompassSong` > `SongMap` with `bpm`, `keyOverride` /
  `lastSetKey` and `Section` elements with `name` and `chords`). Key, tempo, sections and **chords** fill in
  automatically.
- All the apps share one idea of "the song". Integration is through files in known folders, the same way
  Groove Grabber already reads Key Compass.

### 6. Starter MIDI parts are in version 1
- **Version 1.0 includes starter MIDI parts.** Don't drop this feature: the owner explicitly asked for it to be kept.
- Parts in the song's key and tempo: chords, bass line, rhythm parts and a hook or melody idea. They follow
  the real chord progression when a Key Compass song is used. Drums come from Groove Grabber's grooves.
- Drag each part (or all of them) into the DAW as MIDI.
- Plus an **arrangement map**: which parts play in the intro, verse, chorus, bridge and so on.

### 7. Name and label
- Product name: **Song Savant**. Maker label: **Grabber Audio** (same as Groove Grabber, for now; the owner may
  rename both before selling). Plugin manufacturer code `Grbr` (same as Groove Grabber), plugin code `SgSv`.
- Changing the label text later is safe; **never change the manufacturer or plugin codes**, or DAWs will treat
  it as a different plugin and saved projects won't find it.
- The plan is to **sell it** one day: keep it polished, cross-platform and careful about third-party formats.

### 8. Design
- Mockups, logo and icons are in `docs/design/` (light and dark versions). These are compressed copies; the
  owner may supply full-size originals.
- Colors: deep navy and black, **gold**, royal blue and purple, with soft glows. Light theme: warm ivory and
  marble with the same accents.
- Changes agreed for the design:
  - Add a **drum** slot.
  - **Flexible parts**: the style decides which parts a song needs (worship leans on pads and ambient guitar
    swells, country on steel guitar or fiddle). You can add, remove and rename slots.
  - Add a **Key** field next to BPM.
  - The palette's **play button plays all the parts together** in a short loop (Phase 3).
  - The home screen's 8 feature tiles become a **sidebar**, not a ring around the big logo.
  - "Why it works" reasons must be specific ("fills the low end without clashing with your piano"), not filler.
  - The mockup's instrument list was invented by ChatGPT (it showed Vital and Kontakt, which the owner doesn't
    have). Show the real scanned instruments.
  - The icon needs a simplified version for 16 px.

---

## The owner's setup (October 2026)

Windows, **Cubase Artist 14** (also has Artist 12/13, Elements 12/13 licences).

- **Steinberg:** HALion 7, HALion Sonic, Groove Agent SE, Retrologue, Padshop, plus free HALion libraries:
  Alto Glockenspiel, Bouncy Vibrations, Colors Free, Guitar Harmonics Essential, LoFi Piano, Navia Harp Free,
  Taped Vibes, X-Stream.
- **Arturia:** Analog Lab **Pro** (shows as "Analog Lab V" in Cubase), version 5.12.6.
- **Cymatics:** DrumPod, Ocean Pluck, Quake, Galaxy, RWND, Daydream, GAMEOVER, Invader Lite, Monarch,
  Night Drive, Pandora, Paradigm Lite, Plinko, Ripple, Sketch, MIDI Printer.
- **Drums:** Groove Agent SE, Groove Grabber, SSD Sampler 5, MT-PowerDrumKit, Cymatics DrumPod.
- **Other:** Scaler Detector, VoxaVersa Singer, Sample Snagger, Native Instruments FM8 (seen as a standalone app;
  not in Cubase's instrument list).

---

## Phases

### Phase 0: Setup and reality check *(current)*
- [x] Project set up like Groove Grabber: JUCE app + plugin, tests, GitHub Actions, Windows installer.
- [x] This roadmap.
- [x] **Setup scanner**: a test build the owner runs on their PC. It reports:
  installed plugins; `.vstpreset` files and their tags; Steinberg `.vstsound` containers (can preset names and
  tags be read from them?); preset databases (Steinberg MediaBay, Arturia, others: tables, counts, sample
  rows); each instrument maker's folders (what file types their presets use); Key Compass songs, Groove
  Grabber kits and Sample Snagger.
- [x] **Drag test**: drag a real `.vstpreset` and a MIDI clip into Cubase and record whether it worked.
- [x] Windows installer builds on GitHub (download from the Actions run's *Artifacts*: `Song-Savant-Windows`).
- [ ] Owner runs the scan and sends `Documents/Song Savant/Setup Report.txt`.
- [ ] Decide, from the report, how to read each library (update decision 4 above with what's confirmed).

### Phase 1: Sounds
- Library scan (the real catalog, using what Phase 0 proved), cached so rescans only look at new files.
- Song form: genre, style, mood, energy, BPM, **key**, reference artist or song; or pick a Key Compass song.
- Palettes (several per song) with every part **including drums**. Flexible slots; lock or regenerate any
  part; alternatives for each slot; "why it works".
- Preset browser with filters, favourites.
- Send to DAW: drag `.vstpreset` (Cubase), copy preset name, reveal the file.
- Save and recall palettes.
- Light and dark themes.

### Phase 2: Coach, then **version 1.0**
- **Starter MIDI parts** (chords, bass, rhythm parts, hook idea) in key and tempo; follow Key Compass chords.
  Drag to DAW.
- **Arrangement map** (which parts play in each section).
- **Groove Grabber hand-off** for drums (and the matching change in Groove Grabber).
- Plugin: follows DAW tempo, plays parts in sync, saves the palette with the project.
- Release 1.0.

### Phase 3: Hearing it
- Preview sounds by loading the instrument in the background and recording a short phrase. Run it in a
  **separate process**, so a crashing instrument can't take down the app or the DAW. Expected to work for
  Steinberg instruments; Analog Lab previews are an open question (plugins can't be told which preset to load
  by name).
- Sound analysis of the previews (brightness, low end, attack, width), using the same approach as Groove Grabber's
  analyzer, for real "these sounds fit together" scoring.
- "Play the whole palette" loop.
- Learns your taste (thumbs up and down, favourites, go-to sounds).

### Phase 4: Later
- More instrument readers (full level), more plugin profiles.
- Mac polish and notarised installer.
- Optional online AI mode.
- Deeper Cubase automation (Cubase's MIDI Remote scripting API, which can trigger Cubase commands).
- Pro Tools (AAX) if there's demand.

---

## Things to remember
- **Before selling:** the Windows installer tool (Inno Setup 6.7) prints "Non-commercial use only" in the build
  log. Check its licence (buy one or switch installers) before selling Song Savant or Groove Grabber, and sign
  the installers so Windows stops warning "Windows protected your PC".
- The repo started empty, so GitHub made the first branch (`claude/tender-hamilton-6xru5d`) the default branch.
  The owner may want a `main` branch (releases with `[release]` in the commit message only publish from `main`).
- Spotify no longer gives new apps tempo and key data (since 2024). Don't plan on it.
- Steinberg factory content is in `.vstsound` containers. Groove Agent's factory sounds are copy-protected.
- Arturia keeps presets in a local database. It's undocumented and an update could change it, so reading it has to fail
  gracefully.
- A plugin cannot see or control other plugins in the DAW. Loading a sound means drag-and-drop, preset files,
  or hosting the instrument ourselves (only for previews, Phase 3).
