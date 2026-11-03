# Song Savant manual

## Welcome

Song Savant helps you build a song with the instruments you already own. You describe the song in your own words, and it looks through every preset and instrument on your computer and suggests a **palette**: one sound for each part of the song (drums, bass, keys, pad, guitar, lead...), each with alternatives. Then it helps you write and shape the song: starter MIDI parts in your key and tempo, a map of which parts play in each section, and lessons on the style you're writing in.

This manual covers version {VERSION}. Open it any time with the **?** button in the header or the **F1** key. Type in the search box at the top to find any word in it.

The workflow, start to finish:

1. **SONG:** describe your song and click **FIND MY SOUNDS**. You get three palettes of sounds you own.
2. **Pick and load:** step through the choices for each part, lock the ones you love, and use **COPY NAME** to load each sound in your DAW.
3. **Starter parts:** drag a card's **MIDI** tag (or **ALL PARTS**) into your DAW for parts already in your key and tempo.
4. **Drums:** send the song to **Groove Grabber** for kits made from your own samples, tuned to your key.
5. **ARRANGE:** lay the parts out across the song (intro, verse, chorus...) and drag the **WHOLE SONG** into your DAW, with a marker for each section.
6. **The piano roll:** change any part in any section, or write your own from an empty grid (a card's **EDIT**, or double-click a box on ARRANGE).
7. **MASTERCLASS:** learn how your style is built: its sounds, drums, bass, chords in your key, arrangement and mix.

Song Savant never suggests a sound you don't own, and it never changes or moves your files. It only reads them.

## Install and first run

Run the Windows installer (Song-Savant-...-Windows-Setup.exe). Windows may say "Windows protected your PC" because the app isn't signed yet: click **More info**, then **Run anyway**.

| What installs | Where |
| --- | --- |
| The Song Savant app | C:\Program Files\Song Savant (Start menu: Song Savant) |
| The VST3 plug-in | C:\Program Files\Common Files\VST3\Song Savant.vst3 |
| Song Savant Scanner (a small helper, see below) | next to the app |

Pick "Standalone app + plug-in" if you're not sure. To update, run the new installer over the old one: your songs and settings stay. Close your DAW first if the plug-in is in use.

**First run:** a welcome screen shows what Song Savant does in three steps; **LET'S GO** closes it (it won't show again), and **OPEN THE MANUAL** opens this manual. Song Savant reads your sound libraries in the background. That takes a few seconds, and the LIBRARY page shows its progress. Then it may spend a few minutes finding out what some of your plug-ins are (see "Song Savant Scanner" under LIBRARY). You can use the app while it does.

**The app or the plug-in?** Use the **app** for most things: it's quick to open next to your DAW, and dragging MIDI from it into your DAW works the same. The **plug-in** (an instrument in your DAW's list, by Grabber Audio) saves your song, palettes and arrangement inside the DAW project, so they're there when you open the project again. It doesn't make any sound yet.

## The screen at a glance

Every page sits under one header. Hover over almost anything to see a tip.

**The header, left to right**

| Control | What it does |
| --- | --- |
| Logo | Click it to switch between the light and dark look (like Groove Grabber) |
| ? | This manual (also the F1 key) |
| Sun / moon | Light or dark look, the same as clicking the logo. Song Savant remembers your choice. |
| Clipboard with a tick | The setup check: what Song Savant found on this computer |
| SONG | Describe your song and get your palettes and starter parts |
| ARRANGE | Your song's sections, and which parts play in each |
| MASTERCLASS | Lessons on any genre or style |
| LIBRARY | Every sound you own, in one searchable list |
| DRUM MAPS | Names the pads of a drum kit that has no drum map in Cubase |

The version number sits under the tabs. The window can be resized from its bottom-right corner, and everything fits itself to the size.

## SONG: describe your song

The top panel is where you tell Song Savant about the song.

**The description box.** Type the song the way you'd describe it to a musician. Song Savant understands:

- **Genres and styles:** "modern country", "trap", "90s boom bap", "worship", "pop rock", "lofi", "outlaw country", "UK drill"...
- **Decades:** "80s", "90s", "2000s", "today".
- **Artists and songs:** "like Morgan Wallen", "Elevation Worship", "Chris Stapleton", "Dr. Dre", and hundreds more (the same list Groove Grabber knows).
- **Describing words:** warm, dark, bright, airy, gritty, punchy, uplifting, dreamy, live, vintage... Your own words count the most when it picks sounds.
- **Tempo and key:** "120 BPM", "in A minor", "key of E", "F# maj".

For example: "modern country like Morgan Wallen, warm, 120 BPM in A minor".

**The boxes under it** let you pick instead of type, and what you pick wins over the words:

| Box | What it does |
| --- | --- |
| Genre | "Genre: from my words" reads it from the description, or pick one of the 10 genres |
| Style | "Style: any", or one of the genre's styles (Modern Nashville, Atlanta Trap, Ambient Worship...) |
| Key | "Key: from my words", or pick a key. The starter parts are written in it. |
| BPM | Type a tempo, or leave it empty to use the description's (or the style's usual tempo) |
| KEY COMPASS | Take the key, tempo, sections and chords from a Key Compass song (see Working with Key Compass) |

Click **FIND MY SOUNDS** (or press Enter in the description box). The gold line under the boxes shows what Song Savant understood, for example "Your song: Country | Modern Nashville | Morgan Wallen | warm | 120 BPM | A Minor". If something is missing from that line, it wasn't understood: use the boxes, or say it another way.

## SONG: your palette

**FIND MY SOUNDS** makes three palettes, **PALETTE 1**, **PALETTE 2** and **PALETTE 3**. Each is a different take on your song: palette 1 has the best match for every part, the others try other good sounds. Click a palette's tab to see it.

Each palette has a **card for every part** the style uses. A country song gets drums, bass, acoustic and electric guitar, steel guitar or fiddle and so on; a worship song leans on pads and ambient guitar; trap gets drums with the 808 in the kit and a bass that sits above it. Cards marked **EXTRA** are optional parts: nice to have, not needed.

**On each card**

- The part's name and what it should sound like ("Warm bass, electric or a soft sub").
- The sound Song Savant picked, and the instrument that plays it (HALion Sonic, Analog Lab V, Groove Agent SE...).
- **The arrows** at the bottom step through the choices for that part ("2 of 7").
- **The lock** (top right of the picture) keeps that sound. Locked parts stay when you click **NEW PALETTES**.
- **The MIDI tag** on the picture is the starter part for that card: drag it into your DAW (see SONG: starter MIDI parts).
- **EDIT** (top left of the picture, on the bass, keys, guitar, pad, strings and lead cards) opens the part in the piano roll (see The piano roll: your own parts).
- **YOUR PART** (under EDIT) means you've changed this part in the piano roll: the card plays and drags your version.

Click a card to see its details in the panel at the bottom:

| Area | What it shows |
| --- | --- |
| The sound | Its name, instrument and library, and its tags as chips (Warm, Bright, Pop, Country...) |
| WHY IT WORKS | Why it fits this part of your song, as a short list |
| OTHER CHOICES FOR THIS PART | The alternatives. Click one to put it on the card. |
| HOW TO LOAD IT | The steps to load this sound in your DAW |
| COPY NAME | Copies the sound's name, ready to paste (see the next chapter) |
| SEND TO GROOVE GRABBER | On the drum card only (see Working with Groove Grabber) |

**NEW PALETTES** makes three fresh palettes for the same song. Locked parts keep their sound in all of them, so you can lock what you love and keep looking for the rest.

**Plug-ins as choices.** Some instruments don't let Song Savant read their presets (SSD Sampler, FM8, Groove Grabber, many synths). Those are suggested as the instrument itself ("Pick a kit in the plug-in"), and you choose the sound inside it. Each part always offers up to two of them among its choices.

Song Savant remembers your song, your palettes, your choices and your locks, in the app and in your DAW project (plug-in).

## SONG: loading a sound in your DAW

Your DAW can't be told which preset to load from outside, so Song Savant gives you the exact name and tells you where to paste it. Click the card, click **COPY NAME**, then:

| The sound is from | Where to paste the name |
| --- | --- |
| Steinberg (HALion, HALion Sonic, Retrologue, Padshop, Groove Agent) | Cubase's MediaBay: press F5, click **All Media** at the top of the File Browser on the left, paste the name in the search box, and drag the result into your project. Cubase makes the instrument track with the sound loaded. Or load the instrument and paste the name in its own preset browser. |
| Arturia (Analog Lab) | Analog Lab's own search box. Load Analog Lab V on an instrument track first. MediaBay doesn't list Analog Lab sounds. |
| Cymatics | The plug-in's own preset list (Plinko, Daydream, Pandora...). |
| A plug-in suggestion | COPY NAME copies the name your DAW lists the plug-in under. Load it on an instrument track and pick a sound inside it. |
| Groove Grabber kits | Groove Grabber's MY KITS page (see Working with Groove Grabber). |
| Your own preset files | The instrument's preset browser. |

**HOW TO LOAD IT** on the card always gives the right steps for that sound. A short note under COPY NAME says where to paste it after you click.

## SONG: starter MIDI parts

Every card that plays notes has a starter part: 8 bars in your song's key and tempo, following the style's chords, groove and feel.

| Part | What it plays |
| --- | --- |
| Drums | The style's groove (kick, snare, hats, toms, crash), with a fill in the last bar |
| Bass | The style's bass line: an 808 that follows the kick, root and fifth, 8ths, offbeats, a walking line... |
| Keys, guitar, strings | The chords, played the way that instrument would: a guitar strums or picks, a piano doesn't strum, keys can play stabs or neo-soul voicings |
| Pad | Long, held chords |
| Lead or melody | A hook idea that fits the chords |

**To use one part:** drag the card's **MIDI** tag onto your DAW's track (or onto the empty space under your tracks to make a new one). Hover the tag to see what it plays.

**To use them all:** drag **ALL PARTS** (next to NEW IDEA) into your DAW. You get one track per part, named after the part and its sound. The chip shows the chords and tempo.

**NEW IDEA** writes another take: the style's next chord progression, a varied drum pattern and a new hook. Click it as often as you like. Parts you've changed in the piano roll (YOUR PART) stay as you made them.

**Your own parts.** Click a card's **EDIT** to change its part in the piano roll. Once you have, the card's MIDI tag, its play button, PLAY ALL and ALL PARTS all use your version: the card stands for the song's first chorus (or the first section the part plays in).

**PLAY ALL** (next to NEW IDEA) lets you hear every starter part together before you drag them in, and the **play button** on each card (bottom right of its picture) plays that card's part on its own: see Hearing your song.

**The key.** The line above the cards says which key the parts are in ("STARTER PARTS IN A MINOR"). If you didn't give a key, Song Savant picks one that suits the style and says "Key picked for you". With a Key Compass song, the parts follow that song's own chords.

**Which drum notes?** The drum part uses General MIDI notes (kick on C1, snare on D1, closed hat on F#1), which Groove Agent and most drum instruments follow. If the drum card is a kit you mapped on the DRUM MAPS page, the part uses that kit's pads instead. If it's a Groove Grabber kit, it uses Groove Grabber's pads, and in styles built on the 808 the kit's 808 plays with the kick. Hover the drum card's MIDI tag to see which notes it uses.

## Hearing your song (PLAY)

You can hear your song's parts in Song Savant before dragging anything into your DAW.

**On the SONG page** the starter parts play as an 8-bar loop, in your key and tempo:

- **PLAY ALL** (next to NEW IDEA) plays every card's part together. It turns into **STOP**.
- **The play button on a card** (the round button at the bottom right of the card's picture) plays **just that card's part**, on its own. Its button turns into a stop square; click it again to stop.
- **While one part plays on its own, click another card** (anywhere on it) to hear that card's part instead. It changes in time, without starting over.
- **Switch between the two whenever you like:** while one part plays, PLAY ALL brings every part back in; while they all play, a card's play button leaves just that part. The loop keeps going either way.
- **Which parts you're hearing:** each card that's playing shows a small moving level meter at the top left of its picture.
- Click **NEW IDEA** while it plays to hear another take.

**On the ARRANGE page** the whole song plays, section by section, with only the parts you switched on in each. The buttons above the map work like your DAW's transport:

| Button | What it does |
| --- | --- |
| To the start (bar and arrow pointing left) | Moves the playhead to bar 1 |
| Rewind (two arrows pointing left) | Back a bar: to the start of the bar it's in, or the bar before if it's right at the start. Hold it down to keep going back |
| Play / Stop (the gold button) | Plays from the playhead. While it plays it's a stop square: **STOP** stops and the playhead goes back to where you started playing, so you can hear the same part again |
| Pause (two bars) | Stops right where it is: Play carries on from there |
| Fast forward (two arrows pointing right) | On a bar. Hold it down to keep going |
| To the end (arrow pointing right and a bar) | Moves the playhead to the end of the song. Play there starts again from bar 1 |

- **The playhead** is the gold line down the map, with a marker on the bar ruler above the sections. It shows where the song will play from, even when it's stopped.
- **Click or drag on the bar ruler** (the strip of ticks above the section names, one tick per bar) to move the playhead to that bar. While it plays, it carries on from there.
- **Moving the playhead while it plays** (any of the buttons, or the ruler) carries on playing from the new place, and STOP then goes back to there.
- **Where you are:** the box next to the buttons shows the bar ("Bar 17 of 72"), the section you're in, and the time, with the song's length.
- At the end of the song it stops by itself and the playhead goes back to where you started.
- In a small window the map scrolls sideways. While it plays it follows the playhead.

Song Savant plays them with **simple built-in sketch sounds**: synthesised drums, a bass, keys or a guitar strum, a pad and a lead. They're there to hear the idea (the groove, the chords, the hook, how the song builds), not your real sounds. For the real thing, load the sounds on the cards in your DAW and drag the parts in.

In the app, the sound comes out of your computer's speakers or audio interface. If you hear nothing, click **Options** at the top left of the window, then **Audio/MIDI Settings**, and pick your output.

Inside Cubase it plays through the track Song Savant is on, in time with Cubase (see Using Song Savant as a plug-in).

## Drums and the 808

**The drum card wants a full kit you play yourself:** a Groove Agent kit, a Drum Machine kit, a kit from Analog Lab or HALion, or a Groove Grabber kit. Ready-made drum loops (they play their own beat) and single drums (just a kick or a snare) are moved down the choices, because they're hard to use as your drum track.

**The 808 goes with the drums.** In trap, country rap and other 808 styles, the drum card says "808 in the kit". The **Bass** card then suggests a bass that **sits above the 808** (plucked, synth or punchy basses) so the two don't turn the low end to mud. Two deep sub basses always stay among the bass choices, with a note: play the same notes as the 808, or use one instead of it.

**No pad names in Cubase?** Kits from Analog Lab, HALion and some other instruments show note numbers instead of Kick, Snare, Hat in Cubase's drum editor. The DRUM MAPS page fixes that (see DRUM MAPS).

## Working with Groove Grabber

Groove Grabber builds drum kits from your own samples. The two apps work together, so your song gets kits made for it. You need **Groove Grabber 1.0.12 or newer** for this, and **1.0.15 or newer** so Cubase doesn't freeze when it quits after using Groove Grabber. Groove Grabber has its own manual for everything else it does: its **?** button or F1.

**The whole trip, in three clicks**

1. In Song Savant, click the **drum card**, then **SEND TO GROOVE GRABBER** (next to COPY NAME; it says "TO GROOVE GRABBER" when the window is narrow).
2. In Groove Grabber, a bar under your description says **Song Savant sent "...": USE IT**. Click **USE IT**. Groove Grabber sets the genre, style, decade, key and tempo, and builds four kits, with the kick, 808 and toms tuned to your key.
3. Listen to the four kits, then click **SAVE KIT** on the one you like (or its heart).

That's it. Within a few seconds the kit you saved appears on Song Savant's **drum card** by itself, locked, in every palette. The starter drum parts and the ARRANGE page use it straight away. Keep Song Savant open while you save the kit.

**Good to know**

- Groove Grabber can be the app or the plug-in in your DAW. If it wasn't open when you sent the song, the song waits in Groove Grabber's **KEY COMPASS** menu under SONG SAVANT SONGS for a day.
- Song Savant sends the key the starter parts use, even when it picked the key for you, so the 808 always matches your parts.
- Only a kit made for the song you sent goes on the drum card. Every other kit you save shows up among the drum choices, with your kits made for the song's style near the top.
- The Groove Grabber plug-in itself stays among the drum choices, so you can build new kits any time.

**Playing the kit in Cubase:** put Groove Grabber on an instrument track, open **MY KITS** and click **LOAD** on your kit. Then drag the drum card's MIDI tag (or the drums from ALL PARTS or WHOLE SONG) onto that track. Every Groove Grabber kit uses the same pads, so the part plays the right drums with any of them. To use Groove Agent instead, click **EXPORT** in Groove Grabber and follow its steps.

## Working with Key Compass

If you wrote the song in Key Compass, Song Savant can start from it.

1. On the SONG page, click **KEY COMPASS**. Your Key Compass songs are listed with their key and tempo.
2. Pick one. Song Savant takes its key, tempo, sections and chords. The gold line says "(key, tempo and chords from Key Compass: ...)".
3. The starter parts now play that song's chords, and the ARRANGE page can use its own sections ("Key Compass: ..." in the shape box).

To stop using it, click **KEY COMPASS** again and choose **Don't use ... any more**.

## ARRANGE: your song's shape

The ARRANGE page lays your palette's parts out across the whole song. Make your palette on the SONG page first.

**The map.** Sections run across the top (Intro, Verse, Pre-Chorus, Chorus, Bridge, Hook, Drop, Outro...) with their bars and chords. Your parts run down the side. A coloured box means that part plays in that section. Each genre starts with its usual song shape: in a trap song the drums and 808 come in with the hook; a worship song builds from a quiet start.

**Changing it**

- **Click a box** to switch a part on or off in that section.
- **Double-click a box** (bass, keys, guitar, pad, strings, lead) to open that part of that section in the piano roll.
- **Drag a box** into your DAW for just that part in that section, or **drag a part's name** (on the left) for that part across the whole song, with the section markers.
- **A pencil** on a box means it plays your own pattern (from the piano roll). **A star** means that section has a pattern of its own (MAKE ONE DIFFERENT).
- **The drums** step through no drums, **light**, **groove**, **build** and **full** as you click: light is a sparse pattern, groove the main beat, build adds energy into the next section, and full is everything with a crash.
- **Click a section's name** for its menu: **Length** (2 to 32 bars), **Copy this section** (right after it, playing the same patterns), **Move left**, **Move right**, **Remove this section**.
- **The shape box** (top) picks the song's shape: **The full song**, **A short song** (one verse and chorus fewer, shorter intro and outro), or **Key Compass: ...** (your Key Compass song's own sections and chords).
- **RESET MAP** (it used to say START OVER) puts the map back the way it came for that shape, undoing your changes to it. It doesn't play anything.

Your changes stay while the song stays the same. Swapping a sound (a different drum kit, say) keeps them. Changing the style, the shape or the parts starts a fresh map.

**WHOLE SONG** (top right) drags it all into your DAW: one track per part with each section's starter parts in place, the tempo and key, and a **marker for every section** (in Cubase, on the Marker track). The chip shows the length in bars and minutes.

**The transport** (the round buttons under the title: to the start, rewind, play / stop, pause, fast forward, to the end) plays the whole song with the sketch sounds, with a gold line moving across the map. Click or drag on the bar ruler above the sections to move it. See Hearing your song.

The panel at the bottom explains how songs in your style are usually built.

## The piano roll: your own parts

The piano roll is where you change a part note by note, or write your own from an empty grid (with chord stamps, strums, arpeggios and the other building tools on the right), then hear it and drag it into your DAW. It edits the melodic parts: bass, keys, guitar, pad, strings and lead. (Drums come in a later version: for now, change them in Groove Grabber or in your DAW.)

**Opening it:** click **EDIT** on a card (it opens the part's first chorus), or **double-click a box** on ARRANGE (that part in that section). **DONE** (or Esc) goes back.

**Sections and patterns.** Every section name has its own pattern for each part: change the chorus bass once, and **every chorus** plays it. A section that should be different (a bigger last chorus, say) can have its own pattern: **MAKE ONE DIFFERENT** asks which one, and it starts as a copy you can change. It shows a star on ARRANGE and in the SECTION row.

**Across the top**

| Control | What it does |
| --- | --- |
| PART | The part you're editing. A star (*) means you've changed it somewhere. |
| SECTION | The pattern you're editing (Intro, Verse, Chorus...). Dim ones: the part doesn't play there yet; draw a note to bring it in. |
| 8 BARS | How long this section is (every section that plays this pattern). Longer: your pattern repeats into the new bars. Shorter: it stops early, and the notes after the end are kept in case you make it longer again. |
| MAKE ONE DIFFERENT | Gives one of the sections that share this pattern a pattern of its own |
| START FROM | Start this pattern again from Song Savant's suggestion, an empty grid, a copy of another section, or a copy of another part's notes here |
| The drag tag (BASS - CHORUS) | Drag this part of this section into your DAW as MIDI |
| DRAW / SELECT / ERASE | What a click does (see below) |
| SNAP | What notes line up to: a bar, 1/2, a beat, 1/8, 1/16, 1/32, triplets, or off |
| - / + | Zoom out and in (or Ctrl + mouse wheel) |
| GHOSTS | Shows the other parts' notes faintly behind yours, so you see how they fit |
| Play (gold) | Plays this section, looped. Space does too. |
| ALONE / WITH THE BAND | Hear just this part, or with everything else in the section |
| UNDO / REDO | Ctrl+Z / Ctrl+Y |

**The grid.** The keyboard is down the left: click a key to hear it. The rows of your song's key are brighter, and in each bar the notes of that bar's chord are lit gold (the brightest is the chord's root), so you can see which notes fit. The chords run along the top. Under the notes, the **velocity** lane shows how hard each note plays.

**Editing with the mouse**

- **Click** an empty spot: a new note (as long as the last one you made). You hear it.
- **Drag** a note to move it; **drag its right end** to make it longer or shorter.
- **Double-click** or **right-click** a note to delete it (or drag with the right button over several). With **ERASE** picked, a normal click deletes.
- **Ctrl + drag** (or **SELECT**) on an empty spot draws a box: the notes in it are selected. Shift adds to what's selected. Dragging a selected note moves them all.
- **Velocity:** drag a note's stem up or down. With several notes selected, they all move up or down together by the same amount, each from where it was (so the louder ones stay louder).
- Mouse wheel: up and down; Shift + wheel: across; Ctrl + wheel: zoom.

**Keys**

| Keys | What they do |
| --- | --- |
| Ctrl+C / Ctrl+X | Copy / cut the selected notes |
| Ctrl+V | Paste them into the bar under the mouse (or where they came from), in this section or another, in this part or another |
| Ctrl+D | A copy right after the selection |
| Ctrl+A | Select every note |
| Delete | Delete the selected notes |
| Up / Down arrow | Move the selected notes up or down a semitone (with Shift: an octave) |
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Space | Play / stop |
| Esc | Select nothing (again: DONE) |

**Copying a whole section:** Ctrl+A, Ctrl+C, pick another section, Ctrl+V. Or use **START FROM** > A copy of another section.

**The building tools** (the panel on the right)

*Chord stamp:* turn on **STAMP CHORDS**, pick a chord, and every click on the grid places the whole chord: its root at (or just below) the note you click, as long as the last note you made. Drag right away to move it. You hear it as it lands.

| Choice | What it stamps |
| --- | --- |
| THIS BAR | The song's own chord in the bar you click: click along the bars and the chords follow the song |
| The song's chords (D, A, Bm...) | That chord, wherever you click |
| MORE | The other chords in your key (with their numbers: I, ii, IV...), and a few richer ones: sus2, sus4, maj7, 7, m7 |
| TRIAD / 7TH / SPREAD | Three notes; four, with the 7th that fits the key; or spread wide (root, fifth, and the third an octave up) |

Click **STAMP CHORDS** again (or DRAW) to go back to single notes.

*Shape the notes:* these change the **selected notes**, or **every note** when none is selected. Each is one step: Ctrl+Z puts it back.

| Tool | What it does |
| --- | --- |
| STRUM | Notes that start together play one after another, like a guitar: down (the low note first) or up, gentle or wider |
| ARPEGGIATE | Breaks each chord into single notes: up, down, or up and back down, in 8ths, 16ths or 8th triplets |
| QUANTIZE | Lines the notes' starts up with the grid (the SNAP), all the way or halfway (keeps some feel) |
| HUMANIZE | A little looser: tiny changes in timing and in how hard each note plays (a little, or more) |
| LEGATO | Each note (or chord) lasts until the next one starts: no gaps |
| CHOP | Cuts long notes into repeated beats, 8ths, 16ths or 8th triplets |
| FLIP | Back to front, or upside down (kept in the key) |
| IN KEY + / IN KEY - | Moves the notes up or down to the next note of your song's key (a note outside the key goes to the nearest one in it) |

*Ideas:* **A NEW IDEA FOR THIS PART** writes another take on this part in your song's style (a new bass line, a new hook...), over what's there. Click it again for another; Ctrl+Z brings yours back.

**What happens to your notes**

- They're kept with the song **as you make them** (saved in your project, or by the app). There's no save button to forget.
- ARRANGE, WHOLE SONG, the card's MIDI tag, PLAY ALL and ALL PARTS all play your version.
- **NEW IDEA and NEW PALETTES leave them alone.** The parts you haven't touched still change with NEW IDEA.
- **Change the key** and your notes move with it (to minor or major too). Change the tempo and they keep their place in the bar.
- **START FROM > Song Savant's suggestion** puts a pattern back to the suggestion (and it follows NEW IDEA again). Undo brings yours back.

## MASTERCLASS: learn a style

The MASTERCLASS page teaches how a genre or style is put together. It follows the genre and style of your song on the SONG page, so the lesson is about the song you're writing.

**Pick another genre or style** in the boxes at the top to study it. The page says "Studying another style". **BACK TO MY SONG** follows your song again. **USE FOR MY SONG** makes the style you're studying your song's style and takes you to the SONG page.

**The cards**

| Card | What it covers |
| --- | --- |
| At a glance | Tempo, feel and the keys it uses |
| The sounds it uses | The instruments and sounds that make the style |
| In your library | The best sound you own for each part |
| Drums | How the drums go, with the groove drawn as a grid (and its swing) |
| Bass | What the bass plays and how |
| Chords | The style's chord progressions, written out in your song's key |
| Arrangement | How songs in the style are laid out |
| Mix | How it's usually mixed |
| Tips | The things that make it sound right |
| Listen to | Artists and songs to study |

## LIBRARY: every sound you own

The LIBRARY page lists every sound Song Savant found, from every library it can read, in one place.

| Library | What Song Savant reads |
| --- | --- |
| Steinberg (HALion, HALion Sonic, Retrologue, Padshop, Groove Agent, Drum Machine) | MediaBay's database: every preset with its tags (category, style, character, rating) |
| Arturia (Analog Lab) | Arturia's preset database. Presets you'd have to buy are marked NOT OWNED and never suggested. Presets for instruments Analog Lab no longer has are left out. |
| Cymatics | The preset files in each plug-in's folder (names and folders, so the kind of sound is a guess) |
| Your own .vstpreset files | Marked YOURS |
| Groove Grabber | The kits you saved, as drum kits |
| Other instruments | Plug-ins whose presets it can't read, suggested as the instrument itself |

**Finding sounds**

- **The search box:** every word must match somewhere (name, instrument, library, kind, tags, description): "warm piano", "country", "Retrologue pad".
- **Kinds of sound:** click kinds on the left (Piano, Keys, Bass, Pad, Lead, Drums...) to show only those.
- **The instrument box:** only one instrument's sounds ("All instruments" shows everything).
- **ONLY SOUNDS I OWN:** hides Analog Lab sounds that show a buy button.
- **ONLY MY OWN PRESETS:** shows only presets you saved yourself.

Click a sound to see where it lives and **HOW TO LOAD IT**, and click **COPY NAME** to copy its name.

**REFRESH LIBRARY** reads your libraries again, for example after installing a new instrument or library. Groove Grabber kits you save show up by themselves; you don't need to refresh for those.

**Song Savant Scanner.** Many plug-ins don't say what they are (a synth? drums? an effect?). After reading your libraries, Song Savant asks each of those plug-ins itself, the way a DAW does, using a small helper program, Song Savant Scanner, one plug-in at a time. The LIBRARY page shows "Finding out what your other plug-ins are: 3 of 12". A plug-in that crashes or hangs only takes the helper down, never Song Savant or your DAW. The answers are remembered until the plug-in is updated.

## DRUM MAPS: name a kit's pads

Some drum kits (from Analog Lab, HALion and other instruments) have no drum map in Cubase. The drum editor shows note numbers instead of Kick, Snare, Hat, Cubase's GM Map is wrong for them, and "Create Drum Map from Instrument" is greyed out. The DRUM MAPS page fixes that: Song Savant plays every key of the kit, listens to each sound, names it, and writes a Cubase drum map.

1. In Cubase, load the kit on an instrument track and pick the sound you want.
2. In the plug-in window's top bar, click the small arrow next to the up and down preset arrows, choose **Save Preset...** and give it a name.
3. Drag that preset onto the DRUM MAPS page (from MediaBay, or from Documents > VST3 Presets), or click **CHOOSE A PRESET...**.
4. Wait about a minute while Song Savant opens the kit in the background (with Song Savant Scanner) and plays every key from C0 to C6. Then load the drum map in Cubase, as step 4 on the page says:
   - Open **MIDI > Drum Map Setup...** (or click the Drum Map field in the instrument track's Inspector, then Drum Map Setup...).
   - If an older map for the same kit is in the list, select it and click **Remove**.
   - Click **Load...**, pick the kit's .drm file in Documents > Song Savant > Drum Maps, then **OK**.
   - Set the track's **Drum Map** field (in the Inspector) to the kit's name.

**The pads.** The page shows every key Song Savant heard and what it called it: Kick, 808, Snare, Clap, Closed Hat, Open Hat, Cymbal, Low, Mid and High Tom, Perc. Many kits play one recording across several keys, higher on each key. Song Savant spots those zones and names the whole zone after its lowest keys.

**Renaming pads.** If a name is wrong, click the pad and pick the right one, or choose **Type a name...**, or **Leave this key out of the map**. To rename a run of pads at once, drag across them (or click one and shift-click another): they're numbered for you (Conga, Conga 2...). The drum map is saved again straight away; load it in Cubase again (step 4) to see the change.

| Button | What it does |
| --- | --- |
| CHOOSE A PRESET... | Pick a kit preset file instead of dragging it |
| MAP IT AGAIN | Listens to the same kit again (after a Song Savant update, say) |
| SHOW THE DRUM MAP | Shows the .drm file in File Explorer |

Next to each drum map, Song Savant saves "<kit> - what Song Savant heard.txt": what it measured on every key. If names are still wrong, send that file.

A preset that plays one sound up and down the keyboard (an 808, a single drum) doesn't need a drum map, and the page says so. Song Savant makes Cubase drum maps for now; other DAWs are planned.

When the drum card's sound has a drum map with the same name, its starter drum parts use that map's pads.

## The setup check

The clipboard icon in the header opens the setup check: a one-off look at what's on this computer. You don't need it to use Song Savant, but it helps when something isn't found.

- **SCAN MY SETUP** looks at your plug-ins, preset files, sound libraries and preset databases. It only reads, and takes a minute or two. **STOP** stops it, and **ALSO LOOK IN A FOLDER...** adds a folder it should look in too.
- **Something missing?** If a sound or instrument you own doesn't show up in the LIBRARY or on the cards, the report shows what Song Savant could and couldn't read on this computer. It's saved as Documents\Song Savant\Setup Report.txt after every scan. **OPEN REPORT**, **SHOW FILE** and **COPY TEXT** get it to you: attach it (or paste it) when you ask for help. It never contains your user name: your home folder shows as ~.
- **Try these first** (listed on the page): click REFRESH LIBRARY after installing something; open Cubase's MediaBay once so it lists new Steinberg libraries; instruments are found as VST3 plug-ins (one installed only as VST2 or AAX isn't listed); for sounds on another drive, ALSO LOOK IN A FOLDER... then SCAN AGAIN.

## Using Song Savant as a plug-in

In Cubase (or any VST3 DAW), add Song Savant on an instrument track: it's in the instrument list, made by Grabber Audio (under Tools when the list is sorted by category). Everything works the same as in the app, plus:

- **It follows Cubase's tempo.** The song's tempo is your project's tempo: the starter parts, the whole song and what you send to Groove Grabber all use it. The gold "Your song" line shows it ("120 BPM (Cubase)"), and says if the style usually sits somewhere else ("the style is usually 70-80 BPM"): change Cubase's tempo to try it. To use another tempo anyway, type it in the **BPM** box.
- **It plays in time with Cubase.** Click **PLAY ALL** or a card's play button (or the gold play button on ARRANGE), then press play in Cubase: the sketch plays along, from wherever Cubase is. On the SONG page the 8 bars loop along Cubase's timeline; on ARRANGE the whole song starts at bar 1 of your project, so it lines up with WHOLE SONG once you've dragged it in. When Cubase stops, the sketch stops. With Cubase stopped, Song Savant plays on its own.
- **While Cubase is playing, Cubase decides where you are.** Song Savant can't move Cubase's playhead, so on ARRANGE the rewind, fast forward, to the start and to the end buttons (and the bar ruler) rest until Cubase stops: move with Cubase's own transport then. The box next to the buttons says so. Stop and Pause still silence Song Savant.
- **The sound** comes out of Song Savant's track in Cubase: turn that track down or mute it whenever you like.
- **It's saved with the project:** your song (description, palettes, choices, locks and arrangement) is there next time you open it.

## Where your files live

| What | Where |
| --- | --- |
| The setup report | Documents\Song Savant\Setup Report.txt |
| Drum maps (and what Song Savant heard) | Documents\Song Savant\Drum Maps |
| Songs sent to Groove Grabber | Documents\Song Savant\Songs |
| Starter parts you drag | a Song Savant folder in Windows' temporary files (your DAW copies them into the project) |
| Settings and what your plug-ins are | AppData\Roaming\Song Savant |
| Groove Grabber kits it reads | Documents\Groove Grabber\Kits |
| Key Compass songs it reads | AppData\Roaming\KeyCompass\Songs |

## Tips and troubleshooting

| Problem | Try this |
| --- | --- |
| A sound wasn't suggested though you have it | Check it's in the LIBRARY (search its name). If it's missing, click REFRESH LIBRARY. Steinberg sounds come from MediaBay: open Cubase's MediaBay once so it lists new libraries. |
| A suggested sound isn't in your instrument | Tell us its name and instrument. Analog Lab presets for instruments it no longer has are left out, and sounds you'd have to buy are never suggested. |
| Pasting the name in MediaBay finds nothing | Click **All Media** at the top of MediaBay's File Browser first: MediaBay only searches the folder that's selected. |
| A word or artist you typed changed nothing | Check the gold "Your song" line. Use the Genre and Style boxes, or say it another way ("like Morgan Wallen"). |
| The drum card shows loops or single drums | Your library may have few full kits for that style. Look further through the choices, send the song to Groove Grabber, or map an Analog Lab or HALion kit on DRUM MAPS. |
| The drum part plays the wrong drums | Check which notes it uses (hover the drum card's MIDI tag). For Analog Lab and HALion kits, make a drum map on DRUM MAPS. |
| Cubase freezes when you quit after using Groove Grabber | Install Groove Grabber 1.0.15 or newer. If it still happens, force-close Cubase and send Documents\Groove Grabber\Groove Grabber log.txt: its last lines show where it got stuck. |
| Your Groove Grabber kit didn't land on the drum card | Song Savant needs to be open when you click SAVE KIT, and the kit has to be built for the song you sent. Saved kits always show up among the drum choices. |
| In the piano roll, Space or Ctrl+C work Cubase instead of Song Savant | Click the piano roll's grid first, so Song Savant's window has the keyboard. |
| Nothing plays when I click play | In the app: Options (top left) > Audio/MIDI Settings, and pick your output. In Cubase: check Song Savant's track isn't muted or turned down. |
| Dragging MIDI into Cubase made a silent track | Drop it on the lane of a track that has the instrument, or set the new MIDI track's output to that instrument. |
| The markers didn't appear in Cubase | Cubase puts them on a Marker track; add one if your project has none (Project > Add Track > Marker). |
| The Library is empty | Wait for "Reading your libraries..." to finish, then click REFRESH LIBRARY. Run the setup check to see what was found. |
| Windows says "Windows protected your PC" | Click More info, then Run anyway. The installer isn't signed yet. |

## Quick reference card

| Do this | How |
| --- | --- |
| Open this manual | ? in the header, or F1 |
| Get palettes for a song | Describe it on SONG, then FIND MY SOUNDS (or Enter) |
| See another sound for a part | The card's arrows, or click a sound under OTHER CHOICES |
| Keep a sound | The card's lock |
| New takes, keeping locked parts | NEW PALETTES |
| Load a sound | Click the card, COPY NAME, paste where HOW TO LOAD IT says |
| One starter part into your DAW | Drag the card's MIDI tag |
| Every starter part | Drag ALL PARTS |
| Another take on the parts | NEW IDEA |
| Hear all the parts | PLAY ALL on SONG (looped) |
| Hear one part on its own | The play button on its card; click another card to switch |
| Hear the whole song | ARRANGE: the gold play button (plays from the playhead) |
| Move around the song | ARRANGE: to the start, rewind, fast forward, to the end; or click the bar ruler |
| Stop here / go back | ARRANGE: Pause stays where it is; Stop goes back to where you started |
| Drums from your own samples | Drum card, SEND TO GROOVE GRABBER, then USE IT and SAVE KIT in Groove Grabber |
| Start from a Key Compass song | KEY COMPASS on the SONG page |
| Change who plays where | ARRANGE: click boxes; click a section's name for its menu |
| Change a part note by note | A card's EDIT, or double-click a box on ARRANGE: the piano roll |
| Write your own part | The piano roll: START FROM > An empty grid, then click to add notes |
| The same change in every chorus | Edit the Chorus pattern once (MAKE ONE DIFFERENT for one that isn't) |
| Copy notes | The piano roll: select, Ctrl+C, then Ctrl+V (another bar, section or part) |
| Place whole chords | The piano roll: STAMP CHORDS, pick THIS BAR (or a chord), click the grid |
| Strum, arpeggiate, quantize, humanize... | The piano roll's panel on the right (the selected notes, or all of them) |
| Another take on one part | The piano roll: A NEW IDEA FOR THIS PART |
| One part of one section into your DAW | Drag its box on ARRANGE (or the piano roll's drag tag) |
| One part, the whole song | Drag the part's name on ARRANGE |
| The whole song into your DAW | ARRANGE: drag WHOLE SONG |
| Learn your style | MASTERCLASS |
| Name a kit's pads in Cubase | DRUM MAPS: drop the kit's preset, then load the .drm in MIDI > Drum Map Setup |
| Light or dark | Click the logo, or the sun / moon button |
