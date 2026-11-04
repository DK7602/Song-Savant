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
- **The logo matches in both themes** (the owner, November 2026). The light theme keeps its own look (the filled-in
  owl; the title's deep navy letters with blue and purple shine and gold swirls) but at exactly the dark theme's size
  and place, and its title gets the dark title's underline with the little sound wave. `tools/make_light_logo.py` makes
  `Resources/owl-light.png` and `wordmark-light.png` on the dark pictures' canvases from the light artwork cut-outs
  (`docs/design/owl-light-cutout.png`, `wordmark-light-cutout.png`): the owls' gold outlines cover the same area, the
  titles' letters too (the light title 6% wider so it spans nearly the same width). Clicking the logo switches light /
  dark, like the owner's other apps.
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

### Phase 0: Setup and reality check *(done)*
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

### Phase 1: Sounds *(done)*
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

### Phase 2: Coach, then **version 1.0** *(1.0.0 released November 2026)*
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
  light, groove, build, full); click a section's name to change its length, copy, move or remove it; RESET MAP
  (called START OVER until 1.0.1, which read like a play button) goes back to the shape. **WHOLE SONG** drags it all into Cubase: a track per part, each section's starter parts in
  place, a marker per section. The song keeps its map (`Source/core/Arrangement.*`, `Source/ui/ArrangePage.*`).
  The header's SETUP CHECK became a small icon next to the light/dark switch (it's a one-off tool now), to make
  room.
- **Groove Grabber hand-off** for drums *(built in 0.10.0, with Groove Grabber 1.0.12; see decision 5)*.
- **The manual** *(0.10.2; the owner asked for one, November 2026)*: `Resources/Manual.md`, read inside the app and the
  plug-in like Groove Grabber's (the **?** button left of the light/dark switch, or **F1**): contents on the left, a
  search box that highlights every match, Esc or CLOSE to leave (`Source/ui/HelpPanel.*`, adapted from Groove Grabber's
  viewer). Plain language, one chapter per page plus drums and the 808, Groove Grabber, Key Compass, the plug-in, where
  files live, troubleshooting and a quick reference card. **Update it in the same commit whenever behaviour changes.**
- **Hearing the parts, and Song Savant inside the DAW** *(1.0.0; the owner asked for it with the 1.0 release)*:
  **PLAY** on the Song page (the starter parts, looped: what ALL PARTS drags; PLAY ALL and a play button per card since
  1.0.1) and **PLAY SONG** on the Arrange page (a full transport since 1.0.1; the
  whole song, with a gold playhead on the map: what WHOLE SONG drags) play the parts with simple built-in **sketch
  sounds** (`Source/core/SketchPlayer.*`: synthesised drums on the General MIDI notes, a bass, keys / a guitar strum /
  an organ / strings by the card's kind of sound, a pad and a lead; never louder than full scale). They're for hearing
  the idea, not the real sounds. In a DAW the plug-in **follows the DAW**: the song's tempo is the project's (unless
  the BPM box says otherwise; the Song page says when the style usually sits elsewhere), and PLAY plays along with the
  DAW's play button and position (the loop along the timeline; the whole song from bar 1), stops when the DAW stops,
  and plays on its own when the DAW is stopped. The song was already saved with the project.
  - **Not in 1.0: sending a part out as MIDI** to a real instrument in the DAW. A JUCE plug-in has one MIDI output,
    and Cubase can't pick one channel out of it without the Input Transformer, so all the parts would land on one
    instrument. Revisit with real-sound previews (Phase 3).
- **Version 1.0.0** *(November 2026)*: a welcome screen the first time it opens (three steps, LET'S GO, OPEN THE
  MANUAL; `Source/ui/WelcomePanel.*`, shown once: the `welcomeSeen` setting), the manual, and a GitHub Release
  (push the tag `v1.0.0`: the workflow builds the installers and publishes them). **Still for the owner before
  selling:** a code-signing certificate (so Windows stops saying "Windows protected your PC") and the installer
  tool's commercial licence (Inno Setup prints "Non-commercial use only"); see Things to remember.
- **Transport and one part on its own** *(1.0.1; the owner asked for both, November 2026)*:
  - **ARRANGE** has a DAW-style transport instead of PLAY SONG: to the start, rewind, play / stop (gold), pause, fast
    forward, to the end (`TransportButton` in `Source/ui/Widgets.*`). The playhead stays on the map when stopped (it's
    where PLAY starts). STOP goes back to where you started playing or last moved the playhead (like Cubase's "return
    to start position on stop"); PAUSE stays put; the song's end goes back like STOP. Rewind and fast forward step by a
    bar (rewind first goes to the start of the bar it's in, unless it's just past the line) and repeat while held. A
    **bar ruler** above the section names: click or drag to move the playhead (to the nearest bar line). A readout
    shows the bar, the section and the time. A long song in a small window scrolls to follow the playhead.
  - Inside a DAW that's playing, the DAW decides where the song is (a plug-in can't move the DAW's playhead), so the
    moving buttons and the ruler rest until it stops, and the readout says so.
  - Starting part-way through, the sketch picks up notes already held there (a pad's chord, a long bass note), and so
    does a part coming back in.
  - **SONG**: PLAY became **PLAY ALL**, and every card with a starter part has a round **play button** (bottom right of
    its picture) that plays just that part. While one part plays, clicking another card switches to that card's part,
    in time; PLAY ALL and a card's button switch between all and one without starting over. The cards being heard
    show a small moving level meter. (`SketchPlayer::setSolo`, `SongSavantProcessor::setSketchSolo`, and the `song...`
    transport calls.)
  - START OVER on ARRANGE became **RESET MAP**: the owner read it as a transport button.
- **The piano roll: your own parts** *(1.1.0; the owner asked for an FL Studio-style MIDI editor with full control,
  November 2026: "simple but great", melodic parts first, separate sections, sketch sounds are fine for now)*.
  - **Patterns per section name.** A part plays one pattern per section name ("Chorus": every chorus), unless a section
    is made different (MAKE ONE DIFFERENT: its own pattern, "Chorus 3", starting as a copy; `ArrangementSection::patterns`).
    Only edited patterns are stored (`PatternBook` in `SongSession::patterns`, saved with the song); the rest stay
    Song Savant's suggestion, made fresh, so NEW IDEA still changes them and never touches yours. Keyed by the card's
    role and the pattern name, so a new palette or a new arrangement keeps them where the names match.
  - **Notes in beats**, with the song's key they were written in (`Source/core/Patterns.*`): a new key moves them
    (the nearest way, and the 3rd, 6th and 7th between major and minor); the tempo can change freely.
  - **Section length** (the roll's LENGTH, or ARRANGE's Length menu) is the sections'; a pattern has its own length.
    A longer section repeats the pattern (made part of it the moment you edit); a shorter one stops early and the
    notes after the end are kept.
  - **The card on SONG stands for the part's first chorus** (`mainSection`, else the first section it plays in): its MIDI
    tag, play button, PLAY ALL and ALL PARTS use your pattern there when you made one (YOUR PART on the card).
  - **Editing** (`Source/core/PatternEdit.*`, no drawing, all tested): click adds a note at the snap before the mouse,
    as long as the last one made; drag moves (the whole selection), the right end stretches, right-click (or ERASE)
    deletes; Ctrl / SELECT draws a selection box; velocity lane; Ctrl+C / X / V (into the bar under the mouse) / D / A,
    Delete, arrows (a semitone, Shift an octave), Ctrl+Z / Y (200 steps); snap from a bar to 1/32 and triplets.
    The clipboard is the processor's, so notes copy between sections and parts.
  - **The screen** (`Source/ui/PianoRollPage.*`): PART and SECTION rows, LENGTH, MAKE ONE DIFFERENT, START FROM (the
    suggestion, an empty grid, another section, another part's notes here), the drag tag (that part of that section as
    MIDI), DONE; the tools, snap, zoom, ghost notes (the other melodic parts, faint), play the section looped
    (`SketchMode::section`, `sketchOfSection`) alone or with the band; the keyboard (key notes brighter, each bar's chord
    gold, the chords along the top), velocity lane, scroll bars. Notes sound as you place them (`SketchPlayer::preview`,
    a lock-free queue). Every change is stored with the song straight away: no save button.
  - **Light in a DAW:** the grid's rows and lines are drawn once into a picture and only again when the view changes;
    the playhead repaints a thin strip; ghost notes are worked out once per part / section, not per repaint; no threads.
  - **ARRANGE:** double-click a box opens it (the first click's on / off is put back); drag a box out for that part in
    that section, a part's name for the whole part (with markers); a pencil marks your patterns, a star a section's own.
  - 1.1.1 (the owner's first try): with several notes selected, dragging a velocity stem moves them all by the same
    amount from where each was (it used to set them all to one value); in DRAW a double-click on a note deletes it.
  - **1.2.0: the building tools**, a panel on the right of the piano roll (`PatternEditor` does the work, all tested;
    each is one undo step, on the selected notes or all of them):
    - **Chord stamp** (`stampChord`): STAMP CHORDS on, then each click places the chord picked, its root at or just
      below the click, as long as the last note. THIS BAR (the song's chord in the clicked bar), the section's own
      chords as chips, MORE (the key's chords with their numerals, and sus2 / sus4 / maj7 / 7 / m7); TRIAD, 7TH (the
      7th in the key) or SPREAD (root, fifth, the third up an octave).
    - **Strum** (down or up, about 20 or 45 ms a note, so the same at any tempo), **arpeggiate** (up, down, up and
      back down; 8ths, 16ths, 8th triplets; for notes that start together), **quantize** (to the snap, all the way or
      halfway), **humanize** (a little or more; timing in ms, velocity +-6 or 12), **legato**, **chop** (beats, 8ths,
      16ths, 8th triplets), **flip** (back to front; upside down, kept in the key), **in key +/-** (the song's scale).
    - **A new idea for this part**: the part's suggestion with another seed, over what's there (undo brings it back).
  - **1.3.0: drums in the piano roll, and patterns dropped in** *(the owner asked for drums next, and to drag drum
    patterns between Groove Grabber and Song Savant both ways, November 2026)*.
    - **A row per drum**, not a keyboard (`Source/core/DrumLanes.*`): Crash, Ride, Open Hat, Closed Hat, Pedal Hat,
      High / Mid / Low Tom, Clap, Snare, Rim, Kick, 808, Shaker, Perc, top to bottom. Drum patterns are kept by drum,
      on General MIDI notes with the 808 on 35 (`Pattern::drums`, never moved by a key change), and turned into the
      drum card's own notes only when played or written (`drumsFromLanes`: a Groove Grabber kit's pads, the 808 on 52;
      a mapped kit's drum map; or General MIDI, where the 808 falls back to the kick). So a pattern survives a change
      of kit. Moving a hit up or down goes a row at a time (`PatternEditor::setRows` / `shifted`). New hits are 16ths.
    - The drum tools: QUANTIZE, HUMANIZE, CHOP (with 16th and 32nd rolls) and FLIP (back to front); no chord stamp,
      strum, arpeggio, legato, in-key or ghost notes. A new idea writes another beat in the style.
    - **Dropping a MIDI pattern in** (`readMidiFile`, `PatternEditor::pasteOver`, one undo step): on the roll (from the
      bar it lands on, replacing the bars it covers), on an ARRANGE box (that part in that section, from bar 1) or on a
      SONG card (the part's first chorus, from bar 1). Notes are moved to start from their first bar. Drums are read
      by drum: the file's "drums:" text, the drum card's own map, then Groove Grabber's pads or General MIDI
      (`laneForNote`), so a file from either app lands right. Hits it can't place are left out.
    - **Every drum MIDI file says its layout**: a text event "drums: <the kit's drum map>" (the card's MIDI tag, ALL
      PARTS, ARRANGE's boxes and parts, WHOLE SONG, the roll's drag tag). Groove Grabber 1.0.21 writes "drums: Groove
      Grabber's pads" and reads both; dropping a pattern on its SAUCE page makes it one of MY GROOVES (up to four
      bars, on 16ths).
    - Still to come for drums: hearing the real kit (Phase 3), and a step-grid view.
- **Changing the song's sections on ARRANGE** *(1.4.0; the owner asked to drag sections to change their order and to add
  and delete them from a right-click menu, November 2026)*.
  - **Drag a section's name** left or right: a gold line shows the gap it lands in (`ArrangementGrid::gapAt`), and it
    moves there with its parts and patterns (`moveSectionTo`). A click without a drag still opens its menu.
  - **Right-click anywhere in a section** (or click its name) for its menu: **Add a section before / after** (one more
    of a section the song has: a copy of the first, so it plays the same patterns; or a new kind, from the style's own
    names first, then the usual ones the style doesn't name, so a trap song is offered a Hook, never a Chorus:
    `sectionNamesInSong`, `newSectionNames`, `addSection`: a new kind gets the style's length, progression and parts for
    that kind of section), Length, Copy, Move left / right, **Delete this section**. Right-click after the last section
    adds one at the end.
  - **UNDO / REDO** on ARRANGE (buttons by the transport, Ctrl+Z / Ctrl+Y): every change to the map is a step (moves,
    adds, deletes, boxes, lengths, RESET MAP; 200 steps). The steps belong to the map: a new style, shape or palette
    (a new arrangement signature) forgets them. A double-click on a box takes the first click's step back instead of
    clicking the box round again.
  - 1.4.1 (the owner asked for adding and removing bars from the right-click menu): **Add bars** / **Remove bars**
    (1, 2, 4 or 8, at the end of the section; `ArrangePage::changeBars`), shown with the length they make. A section
    is one length for all its parts (the arrangement has no per-part lengths), so the menu says so: every part goes
    with it. Length lists the section's own length too when it's an odd one.
- **A blank song, your own chords, and sounds that aren't there** *(1.5.0; the owner asked for "an option to build my own
  arrangement / chords / notes using the recommended sounds", and said Analog Lab V no longer has "Smoker", November 2026)*.
  - **BLANK SONG** (SONG page, next to PLAY ALL; or "A blank song" in ARRANGE's shape box): `SongShape::blank`. The map
    starts with one empty 8-bar Verse, every part off. Nothing is suggested in a blank song: an unedited pattern is an
    empty grid (`patternAt`, `sectionNotes`), new kinds of section start empty (`addSection`), START FROM's suggestion
    becomes your pattern. Its signature is just "blank", so a new style, description or palette leaves it alone.
  - **A blank song keeps its own map and notes**: going into one puts the other song's arrangement and patterns aside
    (`SongSession::setSongShape`, `arrangementAside`, `patternsAside`, saved with the song), coming out brings them back.
  - **+ ADD SECTION** after the last section on ARRANGE (any song).
  - **Your own chords** (any song): click a bar's chord along the top of the piano roll: the key's chords with their
    numerals, Richer (sus, 7ths), Any chord (12 roots x major, m, 7, maj7, m7, sus2, sus4, dim), Song Savant's chord
    here, All of Song Savant's chords. Stored per section (`ArrangementSection::chords` + `ownChords`) and set in every
    section of that name (`setSectionChord`, `clearSectionChords`, `styleChords`). Suggested parts follow them (they
    read `sectionChords`); your own notes don't move.
  - **Sounds that aren't in your library**: NOT IN MY LIBRARY? on the SONG page's details (asks first) and NOT IN MY
    LIBRARY / SUGGEST IT AGAIN on the LIBRARY page. `Source/core/HiddenSounds.*`: one sound key per line in
    <app data>/Hidden sounds.txt; makePalettes skips them; `SongSession::forgetSound` takes one out of the palettes
    (the next choice takes its place, unlocked). Analog Lab's "Smoker" is left out for everyone
    (`retiredArturiaPresets`, counted as left out like the Prophet V3 presets).
- **Maximise in the app** *(1.5.1; the owner asked for a full-screen button by minimise and close, November 2026)*:
  JUCE's standalone window asks for minimise and close only, so the editor adds maximise when it finds itself in a
  `DocumentWindow` (`SongSavantEditor::parentHierarchyChanged`; in a DAW there's none, the window is the host's). The
  title bar's double-click maximises too. The size limit went up to 3840 x 2400 so a maximised window fills big screens.
- **FAVORITES** *(1.6.0; the owner asked for favorites, "so I can save sounds/arrangements and use/edit later", a tab between
  MASTERCLASS and LIBRARY, November 2026)*. Small files only: nothing runs in the background.
  - **Favorite sounds** (`favorites::` in `Source/core/Favorites.*`): the heart on a SONG card and on a LIBRARY sound (and
    its FAVORITE button). Kept in Documents/Song Savant/Favorite sounds.xml with the sound's name, instrument, library and
    kind (so the list reads right without the library), read again when the file changes (app and plug-in). **They
    don't steer the palettes** (the owner's choice): they wait in the list until you pick one. USE IT puts one on a card
    of the palette, locked (`SongSession::putSound`; the cards it fits first); COPY NAME; how to load it; REMOVE.
  - **Saved songs** (`mysongs::`): SAVE SONG (SONG page, under FIND MY SOUNDS), Ctrl+S anywhere, SAVE THIS SONG / SAVE
    CHANGES and SAVE AS NEW on FAVORITES. One file per song in Documents/Song Savant/My Songs (`<name>.songsavant`: a
    `SongSavantSong` with name, style, key, tempo, size, blank, saved, around the session's own `Song` XML, so
    everything comes back: palettes with locks and choices, the arrangement, your patterns and chords, the blank
    song). `SongSession::savedAs` remembers which one it is (saved with the project too). OPEN (asks first when the
    song you have isn't saved), RENAME, DELETE (twice); a copy goes to My Songs/Backups before saving over or deleting
    (newest 200 kept), and BACKUPS brings one back as its own song. Opening clears ARRANGE's undo steps.
  - The header's note line ("Saved ...", "Opened ...") replaces the version for a few seconds. With six tabs, the tabs
    get a little tighter on a narrow window so the logo keeps its room.
- **The drag test is gone** *(1.0.2; the owner asked whether to keep it, November 2026)*. Phase 0's question (can
  presets and MIDI be dragged between Song Savant and Cubase?) is answered, nothing read its WORKED / DIDN'T WORK
  answers but the report, and to a buyer "Drag test with Cubase" looks like a step they must do. The setup check is
  now the scan and **"Something missing?"**: the report (saved after every scan, for asking for help) and a few
  things to try first. Gone with it: the drop target on that page, the test MIDI clip there, the scan's preset picks
  and the preset it copied out of a library for the test, the report's "Drag tests" section, and the answers saved
  with the project (old projects still open; the answers are ignored). `describeDroppedFile` and
  `extractEmbeddedPreset` stay (tested) for later: reading a MIDI part dropped from the DAW, loading a factory
  preset by drag. Also fixed: the ? and setup buttons showed a plain gold circle while their page was open.

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
