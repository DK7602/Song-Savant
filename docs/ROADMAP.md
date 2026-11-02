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
- **Loading a sound** (confirmed by the Phase 0 drag tests, Cubase 14 Artist): dragging a `.vstpreset` file from
  outside Cubase into the project **does not work**, so Song Savant doesn't rely on it. The flow is: Song Savant
  shows the exact preset (name, instrument, library) with a **"copy name" button**, and you paste the name into
  Cubase's preset search. **Confirmed working:** in MediaBay (F5), select **All Media** at the top of the File
  Browser (MediaBay only searches the selected folder), paste the name, then drag the result into the project:
  Cubase creates the instrument track with the sound loaded. The instrument's own preset browser works too.
  **MediaBay only lists Steinberg presets**: Analog Lab sounds are found in Analog Lab's own search, Cymatics sounds
  in each plug-in's preset list, so Song Savant says where to paste for each source. The
  same idea works in every DAW. **MIDI drag works both ways**: Song Savant's MIDI clips drop into Cubase, and MIDI
  parts dragged from Cubase onto Song Savant arrive as `.mid` files (so Song Savant can read a song's chords and
  parts later).

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

**What the first real scan (setup check 0.1.0, October 2026) proved:**
- **Steinberg: read MediaBay's own database**, not the `.vstsound` files. `%LOCALAPPDATA%\Steinberg\Steinberg MediaBay
  Server\mediabay3.db` (SQLite, read-only from a copy) has one row per file and preset (~44,500 on the owner's PC) in
  the table `media`, with `url` (`vstsound://<library GUID>/path` or a file path), `MediaType`, `FileName`,
  `PlugInName`, `MediaLibraryName` and the MediaBay tags as columns (`MusicalCategory`, `MusicalInstrument`,
  `MusicalStyle`, `MusicalSubStyle`, `MusicalCharacter`, `MusicalMoods`, `MusicalProperties`, `MediaRating`,
  `MediaComment`...); `volumes` maps libraries. That covers HALion, HALion Sonic, Retrologue, Padshop, Groove
  Agent and the free libraries.
- `.vstsound` containers ("Steinberg Resource File" header) hold readable preset names but no plain VST3 presets
  (except Cubase's own effect presets), so copying a factory instrument preset out of one needs more work.
  Loose `.vstpreset` files (the owner's own) read fine but carry no tags.
- **Arturia: fully readable.** `C:\ProgramData\Arturia\Presets\db.db3`: `Preset_Id` (~12,000 presets: name, type_v2,
  subtype_v2, instrument_key, sound_designer, comment, file_path, favorite...), `Instruments`, `Types_V2`,
  `Subtypes_V2`, `Characteristics` (+ `Characteristics_Types`: characteristics, genres, styles) linked through
  `Preset_Characteristics`, `Genres`, `SubGenres`. **Ignore `Purchasable_Packs*`**: those are presets for sale, not
  owned.
- **Cymatics:** presets are files (`C:\ProgramData\Cymatics\<plug-in>\Presets\...\*.cymatics`, Daydream `*.json`,
  Pandora `*.preset`): names and folders only, so plugin-level plus preset names.
- Plug-in types: newer VST3 plug-ins keep `moduleinfo.json` in `Contents/Resources/` (read since 0.2.0: 66 of the
  owner's 128). The rest (all the Steinberg instruments, Analog Lab V, FM8, SSD Sampler 5, MT-PowerDrumKit, older
  Cymatics) don't say. Cubase's own plug-in list (`VstPlugInfoV4.xml`) has names, bus counts and flags but **no
  categories**. So Phase 1 **asks each plug-in directly, the way DAWs do, in a separate helper process** (a
  crashing plug-in can't take Song Savant down), caches the answers, and adds built-in profiles for well-known
  plug-ins.
- MediaBay coverage (0.2.0): 12,783 presets, 11,466 with a category, 9,460 with character tags, 5,954 with a
  style (Pop, Electronica/Dance, Urban, Ambient, Rock, Classical, Jazz, Country...); moods are almost never set.
  Presets made for "HALion Sonic SE" (5,807) load in HALion Sonic. Also in MediaBay: Groove Agent kits
  (`GAInstrument`), MIDI loops, track presets, chord pads.
- Arturia: 12,166 presets in the database, of which 2,914 are flagged as Analog Lab factory presets.
  **Settled:** some presets show "buy" in the owner's Analog Lab, so Song Savant only suggests presets flagged
  `analoglab_factory = 1` plus the owner's own (presets in non-factory packs). The others stay visible in the
  browser, marked "not owned". (Later: a setting to say which extra Arturia instruments you own.)
- **Settled (0.6.1):** Arturia's database also keeps factory presets for instruments that Analog Lab Pro no longer
  has. The owner tried "75 Lead Soft" (Prophet V3): not in Analog Lab, and Analog Lab's instrument list has
  Prophet-5 and Prophet-VS but no Prophet. So presets for **retired instruments** (`retiredArturiaInstruments()`:
  Prophet V3 so far) are left out, including layered presets that use one (`Preset_Metadata` InstrumentPart1/2),
  and so are presets whose file is missing (only when most files can be found, so odd paths never empty the list).
  The Library shows how many were left out. The setup report now lists, per instrument, how many Analog Lab preset
  files are on disk or missing, the `ALVersionFirst` / `ALVersionLast` tags and the instruments inside layered
  presets, to find any other retired instruments.
- Privacy: the scan never lists or opens files or folders named like keys, passwords, licences or credentials,
  and replaces the user's home folder with `~` in every form (including `file://` links).

**The app never suggests a preset you don't own.** Suggestions always come from the scanned catalog.
Tags guessed from a preset's name or category are shown as guesses.

### 5. Drums and the sibling apps
- **Drums are a part in every palette.**
- **The drum card wants a full kit you play yourself** (settled November 2026, from the owner's 0.6.2 test: no
  Groove Agent kits came up for trap; HALion Sonic "Beats" loops such as "Backstreet Girls" and Analog Lab's
  "808 Kick" did, and a loop has no usable drum map in Cubase). `drumForm()`: Groove Agent and Drum Machine presets
  and "Drumset" kits score +4; ready-made loops ("Beats") and single drums (kick, snare...) -8, so they only show up
  further down the choices. Equal scores are now shuffled fairly (by song and seed) instead of alphabetically, and
  names like "Casette808" also count as "808".
- **The 808 goes with the drums** (the owner's way of working, settled November 2026). In trap and country rap the
  drum card says "808 in the kit", and the **Bass** card suggests a bass from Analog Lab or HALion that **sits above
  the 808** (plucked, synth or punchy basses; deep subs and 808-style basses count against it) so the two don't turn
  the low end to mud. Two deep subs always stay among the alternatives, with a note to play the same notes as the
  808 or use one instead of it. (`PartSpec::note`, `alsoOffer`, `alsoOfferNote`.) Phase 2's starter parts should
  write the 808 and bass lines on the same notes, and Groove Grabber's kit should get the song's key to tune the 808.
- **The Groove Grabber hand-off** *(built in 0.10.0 with Groove Grabber 1.0.12; the owner approved changing Groove
  Grabber for it)*. Selecting the drum card shows **SEND TO GROOVE GRABBER** next to COPY NAME (just "TO GROOVE
  GRABBER" when the window is narrow). It writes the song to `Documents/Song Savant/Songs/<name>.sssong` (JSON:
  `app` "Song Savant", `format` 1, `name`, `description`, `genre` and `style` ids (the shared ones), `era`, `key`
  0..23 and `bpm` (the ones the starter parts use, so the 808 matches them even when Song Savant picked the key),
  `sections` (the arrangement's names and bars), `drums`, `sent`). The name is the Key Compass song's, or the first
  words of the description. Groove Grabber offers a song sent in the last day under its description ("Song Savant
  sent ...: USE IT", with an X to hide it) and lists the newest eight in its KEY COMPASS menu; using one sets the
  description, genre, style, era, key and tempo and builds four kits, tuned to the key. Groove Grabber doesn't use
  the sections yet (it builds kits, not songs).
- **Saved Groove Grabber kits are drum choices** (`Documents/Groove Grabber/Kits/<genre>/*.ggkit`): a kit, tagged
  with its genre, style and decade, its character from Groove Grabber's sliders (punchy, warm, dry, live,
  electronic...) and "808" when it has one. Your kit made for the song's style scores +4 more than another kit
  (+1, and +3 for the style), and one made for another genre -3. The Groove Grabber plug-in itself stays among the
  choices (to build new kits). How to load it: Groove Grabber's MY KITS, LOAD (it plays the kit itself), or EXPORT
  for Groove Agent. **A kit you save comes back by itself** *(0.10.1: the owner found the round trip too complicated,
  November 2026)*: the Song page looks at Groove Grabber's Kits folder every 3 seconds, reads new kits into the
  library without REFRESH LIBRARY, and puts the one built for this song (Groove Grabber keeps the song's description
  with the kit; or one saved after SEND TO GROOVE GRABBER) on the drum card of every palette, locked, so ARRANGE uses
  it. So the whole trip is: SEND TO GROOVE GRABBER, USE IT, SAVE KIT on the one you like.
  With a Groove Grabber kit (or the plug-in) on the drum card, the starter drum parts play its pads
  (General MIDI drums on 36..51, toms on 43/47/50, the 808 on 52, percussion on 53..55), and in styles built on the
  808 the kit's 808 plays with the kick. Code: `Source/core/GrooveGrabberLink.*`.
  Groove Agent kits, SSD Sampler 5, MT-PowerDrumKit and other drum instruments appear as alternatives.
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
- [x] Owner ran the scan (0.1.0) and sent the report: findings are under decision 4.
- [x] 0.2.0: plug-in types (moduleinfo in `Contents/Resources`, Cubase's plug-in list), a closer look at the
  MediaBay and Arturia databases, things dragged from Cubase onto Song Savant are noted, clearer drag directions.
- [x] Owner ran 0.2.0: MediaBay and Arturia fully readable; MIDI drag works both ways; preset files don't drop
  into Cubase (so "copy name" it is); Cubase's plug-in list has no categories.
- [x] 0.3.0: privacy fixes (user name in `file://` links; private-looking names skipped), Arturia ownership counts.
- [x] Settled which Arturia presets are playable, and confirmed the MediaBay search-and-drag flow. **Phase 0 done.**

### Phase 1: Sounds *(current)*
Built in steps, each one a build the owner can try:
- **1a Library:** the catalog (below) and a sound browser: search, filter by kind of sound and instrument,
  details, copy name, how to load it. *(Built in 0.4.0: owner to try it on the real libraries.)*
- **1b Music knowledge:** genres, styles, decades and reference artists (same list and IDs as Groove Grabber),
  plus which parts each style uses and what they should sound like. *(Done: `Source/core/Music.*` reads
  descriptions like "modern country like Morgan Wallen, warm, 120 BPM in A minor"; the styles and the 400+ artists
  and songs are copied from Groove Grabber by `tools/sync_from_groove_grabber.py` into `MusicData.inc` (re-run it
  when Groove Grabber's tables change); `Source/core/Parts.*` has each genre's band, style tweaks (country rap 808,
  Americana banjo, gospel organ, ambient worship pads...), decade words, and your words first.)*
- **1c Palettes:** the song form and palette suggestions (every part, drums included), alternatives, lock and
  regenerate, "why it works", save and recall. *(Built in 0.5.0: SONG tab. `Source/core/Palette.*` scores each
  owned sound per part: right kind (first choice 10, then -3 each), your words +4, the part's words +2.5/+1.5,
  avoided words -4, tagged with the genre +3, rating, your own presets +0.5, guessed kinds -1.5. Three palettes,
  each a different take; 6 alternatives per part; locked parts survive NEW PALETTES (a new seed).
  `Source/core/SongSession.*` saves the form, seed, locks and choices with the project and rebuilds them the same
  way. Owner to try it on the real library.)*
- **1d Polish:** plug-in scan in a helper process, light theme, the mockup's look throughout. *(In 0.6.0:
  light theme with a sun / moon switch in the header, remembered. **Plug-in suggestions:** instruments whose
  presets Song Savant can't read are suggested as the plug-in itself ("SSD Sampler 5: pick a kit in the plug-in"),
  from built-in profiles of well-known instruments (`Source/core/PluginProfiles.*`: what each is good for and
  describing words) or, for others, the kinds the plug-in declares (Instrument|Drum, Piano, Synth...). Plug-ins
  whose presets are already listed (HALion, Analog Lab, Cymatics with presets) aren't suggested twice, effects never
  are. A plug-in suggestion scores 3 points less than a preset that matches as well (a preset is more specific), and
  each part always offers up to two of them among its alternatives (so SSD Sampler 5 still shows up next to dozens of
  Groove Agent kits). COPY NAME copies the name the DAW lists the plug-in under. **Song Savant Scanner** (`Source/scanner/`):
  a small helper program, installed next to the app, that loads one plug-in the way a DAW does and says what it
  is. After reading the libraries, Song Savant runs it (in the background, one plug-in at a time, 20 seconds
  each, 5 minutes at most per run) on VST3 plug-ins that don't say what they are and that it doesn't know by
  name, and remembers each answer in `plugin-types.json` (in Song Savant's settings folder) until that plug-in is
  updated. A plug-in that crashes or hangs only takes the helper down. The plug-in version finds the helper through
  the installer's registry entry. Song page details now show the sound's tags as chips and "why it works" as a
  ticked list, like the mockup. If the library is read again, the palettes find their sounds in the new list.)*

- Library catalog from what Phase 0 proved: **MediaBay's database** (Steinberg), **Arturia's database**, Cymatics
  preset files (names and folders), the owner's own `.vstpreset` files. Cached; rebuilt when those change.
- Plug-in scan in a helper process (instrument or effect, maker, categories), cached, plus built-in profiles
  for well-known plug-ins (plugin-level suggestions). *(Done in 0.6.0, see 1d.)*
- Song form: genre, style, mood, energy, BPM, **key**, reference artist or song; or pick a Key Compass song.
- Palettes (several per song) with every part **including drums**. Flexible slots; lock or regenerate any
  part; alternatives for each slot; "why it works".
- Preset browser with filters, favourites.
- Send to DAW: **copy preset name** (paste into Cubase's preset search), show where it is (instrument, library),
  drag MIDI.
- Save and recall palettes.
- Light and dark themes.

### Phase 2: Coach, then **version 1.0**
- **Style knowledge** (`Resources/StyleGuide.json`, `Source/core/StyleGuide.*`): for each of the 10 genres and the
  40 styles, what it is, tempo and feel, keys, typical sounds and instruments, drums, bass, harmony (with chord
  progressions written as Roman numerals), arrangement, mix and tips, plus the rules the starter parts follow (drum
  groove, swing, how the bass, chords and hook play). A style adds to or replaces its genre's entries. Shared by the
  starter parts and the Masterclass page.
- **Starter MIDI parts** *(built in 0.8.0; the owner confirmed they drag into Cubase and work, November 2026)*: for the palette on the Song page, 8-bar parts in the song's key
  and tempo: drums (from the style's groove; General MIDI notes, or a Song Savant drum map with the same name as
  the drum sound), bass (808 that follows the kick, root-fifth, 8ths, offbeat, walking...), chords (pads, strums,
  stabs, neo-soul voicings...), and a hook idea. The chords come from the Key Compass song when one is picked,
  otherwise from the style's progressions. Drag one part, or all of them (one track each), into Cubase. NEW IDEA
  makes another take. *(0.8.0: each card with a part has a MIDI tag on its picture to drag; ALL PARTS next to the
  palette tabs drags them all, one track each, and shows the chords and tempo; NEW IDEA takes the next progression
  and varies the drums and hook. A guitar card strums or picks and a piano doesn't strum (`chordStyleFor`). The
  drums use the drum card's Song Savant drum map when one has the same name. `Tests` render every style's parts;
  `SS_DUMP_PARTS=<folder>` writes them all out to listen to.)*
- **MASTERCLASS page** *(built in 0.8.0; the owner asked for a learning page, November 2026; the name pairs with
  Song Savant: the savant gives the masterclass)*: follows the genre and style on the Song page, or pick another one to learn. Shows
  the style's sound, the instruments and sounds it uses (and the best ones in your own library), drums, bass, chords
  in your key, arrangement, mix tips, and artists to listen to. *(0.8.0: a tab between SONG and LIBRARY; genre and
  style pickers, BACK TO MY SONG, USE FOR MY SONG; cards in two columns: At a glance, The sounds it uses, In your
  library (the best pick per part), Drums with the groove drawn as a grid, Bass, Chords (each progression in your
  key), Arrangement, Mix, Tips, Listen to. The header's tabs size to their names and the logo shrinks to fit.)*
- **Map a drum kit** *(built in 0.7.0, the owner's idea; owner to try it)*. Kits such as Analog Lab's "Beat Box"
  have no drum map in Cubase: the GM Map is wrong and "create drum map from instrument" is greyed out. The owner
  saves the kit as a preset from the plug-in window in Cubase (that file holds the plug-in's whole state, so it
  works for Analog Lab too) and drops it on the **DRUM MAPS** page. Song Savant finds the plug-in (by the preset's
  class ID in the plug-in's own info, then by name), runs **Song Savant Scanner --map**, which opens the plug-in in
  its own process, loads the preset, plays every key from C0 to C6 offline and listens to each hit
  (`Source/core/KitMap.*`: how low, bright, noisy and long it is, and how many bursts), names it (Kick, 808, Snare,
  Clap, Closed/Open Hat, Cymbal, Low/Mid/High Tom, Perc) and saves a **Cubase drum map** (`.drm`) in
  Documents/Song Savant/Drum Maps. The owner loads it once in Cubase (MIDI > Drum Map Setup > Load). A preset that
  plays one sound across the keys is spotted ("no drum map needed"). The tests map a tiny test kit plug-in
  (`Tests/TestKit`, tests only) end to end. To check on the owner's PC: that the helper can open Analog Lab and
  HALion with a saved preset, that the names are right, and that Cubase loads the `.drm`.
  - **Confirmed on the owner's PC (0.7.0):** the helper opens Analog Lab with a saved preset and plays it, and
    **Cubase loads Song Savant's `.drm` in Drum Map Setup** (the file layout is right).
  - **0.7.1, from the owner's first try** (Analog Lab "Beat Box": it mapped; C0-B2 right, 19 kicks, 11 snares and
    6 toms; C3-C6 wrong, nearly all called Snare, though they're a mix of toms, kicks and percussion played higher
    up). Higher-pitched sounds cross zero often, which the old "noisy" test took for a snare. Now each hit is also
    checked for **how clearly it has a pitch** (how well it matches itself one period later, 40 Hz-1 kHz): only
    noisy sounds can be snares, claps or hats; a tom also needs a low body (energy below ~400 Hz), so a cowbell is
    Perc. **Rename by hand:** click a pad to pick another name (Kick, Rim, Shaker, Cowbell...), type one, or leave the
    key out; typed names are kept and the drum map is saved again straight away.
  - **0.7.2, from the owner's second try:** still wrong at C3-F4 (toms called Perc and Snare) and C6 (perc called
    Snare). Beat Box is laid out in **zones**: kicks C0-F#1, snares G1-F2, toms F#2-F4, perc F#4-C6, each zone one
    recording stretched over several keys, higher on each key (how the old samplers Analog Lab copies work). Played
    two octaves up, a tom is short and bright. Now the helper checks each key against the key below (slowed down by
    a semitone, are they the same wave? `pitchedCopyMatch`), keys that match form a zone, and the whole zone is called
    what its **lowest third** sounds like (`groupPitchedZones`). "One sound across the keyboard" now means one
    recording over most of the keys. Also: shift-click a second pad to rename a whole range; step 4 on the page gives
    the load steps for the DAW in use (Cubase/Nuendo: MIDI > Drum Map Setup..., Remove the old map, Load..., set the
    track's Drum Map in the Inspector; other DAWs: Cubase maps only for now); and "<kit> - what Song Savant heard.txt"
    is saved next to each map (per key: the zone match, pitch, noise, decay...), for the owner to send back if
    names are still wrong.
  - **0.7.3, from the owner's video:** shift-click didn't work, because a click opens the rename menu and closing
    the menu also forgot the first pad. Now the first pad is remembered (`RangePicker`), and you can **drag across
    pads** to pick a run of them. The zone fix helped (C3-F3 now toms, C6 now Perc) but F#3-F4 were still called
    Snare.
  - **0.7.4, tuned on the owner's "what Song Savant heard" file** (Beat Box, 73 keys): the zones were found exactly
    (each key matches the one below at 1.00, and the match drops at every zone boundary). F#3-F4 is a second set of
    toms with a breathy, noisy hit and no clear pitch, while the real snares (G1-F2) are short (about 0.15 s). A
    noisy sound that isn't very noisy or bright (under 2,200 crossings a second, little above 5 kHz), is low (60-400
    Hz) and rings for 0.2 s or more is now a Tom. Replaying the owner's numbers gives C0-F#1 Kick, G1-F2 Snare,
    F#2-F4 Tom, F#4-C6 Perc, which is what the owner hears. (The owner's file isn't kept in the repo; the test uses
    round numbers like it.)
  - **Settled:** with drum maps fixed, Analog Lab and HALion kits may be suggested as freely as Groove Agent kits
    (the owner, November 2026): any drum preset that isn't a loop or a single drum counts as a kit (+4), and their
    "How to load it" points to the DRUM MAPS page.
  - Next: use these maps for the starter drum parts.
- **Starter MIDI parts** (chords, bass, rhythm parts, hook idea) in key and tempo; follow Key Compass chords.
  Drag to DAW.
- **Arrangement map** *(built in 0.9.0; the owner asked for it next, November 2026)*. The **ARRANGE** tab (after
  SONG): sections across (intro, verse, pre-chorus, chorus, bridge, drop...), the palette's parts down, a box for each
  part in each section. Each genre has its usual song shape in the style knowledge (bars, who plays: drums light /
  groove / build / full, bass, chords, pad, hook; "all" brings in the extras; which progression: the main one, the
  verse's, the bridge's). Shapes: the full song, a short one (the second verse goes, shorter intro and outro), or the
  picked Key Compass song's own sections and chords. Click a box to switch a part on or off (the drums step through
  light, groove, build, full); click a section's name to change its length, copy, move or remove it; START OVER goes
  back to the shape. **WHOLE SONG** drags it all into Cubase: a track per part, each section's starter parts in
  place, a marker per section. The song keeps its map (`Source/core/Arrangement.*`, `Source/ui/ArrangePage.*`).
  The header's SETUP CHECK became a small icon next to the light/dark switch (it's a one-off tool now), to make
  room.
- **Groove Grabber hand-off** for drums *(built in 0.10.0, with Groove Grabber 1.0.12; see decision 5)*.
- **The manual** *(0.10.2; the owner asked for one, November 2026)*: `Resources/Manual.md`, read inside the app and the
  plug-in like Groove Grabber's (the **?** button left of the light/dark switch, or **F1**): contents on the left, a
  search box that highlights every match, Esc or CLOSE to leave (`Source/ui/HelpPanel.*`, adapted from Groove Grabber's
  viewer). Plain language, one chapter per page plus drums and the 808, Groove Grabber, Key Compass, the plug-in, where
  files live, troubleshooting and a quick reference card. **Update it in the same commit whenever behaviour changes.**
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
- **Never keep fonts, images, timers or threads in statics.** In a DAW, JUCE shuts down when the last instance of the
  plug-in closes, but a static lives until the DAW unloads the plug-in at quit, and letting go of graphics there can
  freeze Windows. Groove Grabber 1.0.12 froze Cubase on quit that way (fixed in 1.0.13); Song Savant 0.10.1 stopped
  keeping its settings file in a static for the same reason.
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
