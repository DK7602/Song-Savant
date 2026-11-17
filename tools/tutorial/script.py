# The tutorial's script: what's said, and what's on screen while it's said.
#
# Each entry is a title card ("card"), a chapter card ("chapter"), or a line of narration ("say") with the shots
# shown while it's spoken. A shot shows one of the captured frames (SSTests --tutorial):
#   f      the frame
#   pre    the frame shown first, until the cursor clicks (the click is what changes it to f)
#   click  where the cursor clicks (a mark: see frames.json), point: where it moves without clicking
#   drag   where the cursor grabs and drags off down and to the right (a MIDI tag into the DAW)
#   hl     marks to light up (the rest of the screen dims a little)
#   zoom   marks to move the camera in on (none: the whole window)
#   w      how much of the line's time it gets (default 1)
# UI words are in capitals as on screen; the narrator says them as words. Keep every line true to the app: change
# this when a button's name or a step changes.

SCRIPT = [
    {"card": "title"},

    {"chapter": "1", "title": "Describe your song"},
    {"say": "Song Savant has five pages: SONG, ARRANGE, MASTERCLASS, FAVS and LIBRARY. Everything starts on SONG.",
     "shots": [{"f": "song-empty", "hl": ["tabs"], "point": "tab.song"}]},
    {"say": "Describe the song you want to make in your own words: the genre, the feel, the tempo and the key. You can name an artist, too.",
     "shots": [{"pre": "song-empty", "f": "type-1", "click": "description", "hl": ["description"], "zoom": ["description", "boxes"], "w": 0.6},
               {"f": "type-2", "zoom": ["description", "boxes"], "w": 0.5},
               {"f": "type-3", "zoom": ["description", "boxes"], "w": 0.5},
               {"f": "type-4", "zoom": ["description", "boxes"], "w": 0.5},
               {"f": "type-5", "zoom": ["description", "boxes"], "w": 1.2}]},
    {"say": "Press FIND MY SOUNDS. Song Savant reads your words back underneath, and you can change the genre, the style, the key or the tempo in the boxes.",
     "shots": [{"pre": "type-5", "f": "song-found", "click": "find_my_sounds", "hl": ["find_my_sounds"], "zoom": ["form"]},
               {"f": "song-found", "hl": ["understood", "boxes"], "zoom": ["form"], "w": 1.4}]},

    {"chapter": "2", "title": "Your palette"},
    {"say": "You get a palette: a card for each part of the song. Every sound on it comes from the instruments you already own.",
     "shots": [{"f": "song-found", "hl": ["cards"]}]},
    {"say": "The arrows on a card step through other sounds that fit that part. The lock keeps a sound when you ask for new palettes.",
     "shots": [{"pre": "song-found", "f": "song-next", "click": "bassCard.next", "hl": ["bassCard"], "zoom": ["bassCard", "chordsCard"]},
               {"f": "song-next", "hl": ["bassCard.lock"], "point": "bassCard.lock", "zoom": ["bassCard", "chordsCard"]}]},
    {"say": "PALETTE 2 and PALETTE 3 are other takes on the whole song, and NEW PALETTES makes fresh ones.",
     "shots": [{"pre": "song-next", "f": "song-palette2", "click": "palette_2", "hl": ["palettes", "new_palettes"], "zoom": ["palettes", "new_palettes"]},
               {"f": "song-palette2", "hl": ["cards"], "w": 0.8}]},
    {"say": "Click a card to see why it fits your song, the other choices for that part, and how to load it in your DAW. COPY NAME copies the sound's name, ready to paste into the instrument's search.",
     "shots": [{"pre": "song-palette2", "f": "song-details", "click": "chordsCard", "hl": ["details"], "zoom": ["details"]},
               {"f": "song-details", "hl": ["why"], "zoom": ["details"], "w": 0.8},
               {"f": "song-details", "hl": ["how", "copy_name"], "point": "copy_name", "zoom": ["details"], "w": 1.2}]},
    {"say": "On the drum card, SEND TO GROOVE GRABBER builds drum kits from your own samples to fit this song.",
     "shots": [{"pre": "song-details", "f": "song-drums", "click": "drumsCard", "hl": ["drumsCard"]},
               {"f": "song-drums", "hl": ["send_to_groove_grabber"], "point": "send_to_groove_grabber", "zoom": ["details"], "w": 1.2}]},
    {"say": "And the heart keeps a sound in FAVS, so it's there the next time you need it.",
     "shots": [{"pre": "song-drums", "f": "song-heart", "click": "chordsCard.heart", "hl": ["chordsCard.heart"], "zoom": ["chordsCard", "bassCard"]}]},

    {"chapter": "3", "title": "Starter parts"},
    {"say": "Every card comes with a starter part, written in your key and tempo and played the way your style plays. PLAY ALL plays them together, and the play button on a card plays that part on its own.",
     "shots": [{"pre": "song-heart", "f": "song-playing", "click": "play_all", "hl": ["play_all", "cards"]},
               {"f": "song-playing", "hl": ["chordsCard.play"], "point": "chordsCard.play", "zoom": ["chordsCard", "bassCard"], "w": 1.2}]},
    {"say": "When you like a part, drag its MIDI tag into your DAW, and it lands on a track. ALL PARTS drags every part at once.",
     "shots": [{"f": "song-stopped", "drag": "chordsCard.midi", "hl": ["chordsCard.midi"], "zoom": ["chordsCard", "bassCard"], "w": 1.4},
               {"f": "song-stopped", "hl": ["allParts"], "point": "allParts", "zoom": ["allParts", "play_all"]}]},
    {"say": "Want a different take? NEW IDEA writes new starter parts in the same style.",
     "shots": [{"f": "song-stopped", "hl": ["new_idea"], "point": "new_idea", "zoom": ["new_idea", "allParts", "play_all"]}]},

    {"chapter": "4", "title": "Arrange the song"},
    {"say": "ARRANGE lays out the whole song: the sections across the top, and a row for each part. A lit box means that part plays in that section.",
     "shots": [{"pre": "song-stopped", "f": "arrange", "click": "tab.arrange", "hl": ["tab.arrange"]},
               {"f": "arrange", "hl": ["grid"], "w": 1.6}]},
    {"say": "Click a box to bring a part in, or take it out. The drums step through light, groove, build and full.",
     "shots": [{"pre": "arrange", "f": "arrange-box", "click": "theBox", "hl": ["theBox"], "zoom": ["theBox", "verseHeader", "introHeader"]}]},
    {"say": "Drag a section's name to move it. Right-click a section to add a section, add or remove bars, copy it, or delete it.",
     "shots": [{"f": "arrange-box", "hl": ["verseHeader"], "point": "verseHeader", "zoom": ["verseHeader", "introHeader"], "w": 0.8},
               {"pre": "arrange-box", "f": "arrange-menu", "click": "introHeader", "hl": ["menu"], "zoom": ["menu", "introHeader"], "w": 1.4}]},
    {"say": "Here the intro gets eight more bars.",
     "shots": [{"f": "arrange-longer", "hl": ["introHeader"], "zoom": ["introHeader", "verseHeader"]}]},
    {"say": "Play the whole song with the transport. Then drag WHOLE SONG into your DAW for a track for every part, with a marker at each section.",
     "shots": [{"pre": "arrange-longer", "f": "arrange-playing", "click": "transportButtons", "hl": ["transportButtons", "transport"], "zoom": ["transportButtons", "transport"]},
               {"f": "arrange-playing", "drag": "wholeSong", "label": "Whole song - A Minor 94 BPM.mid", "hl": ["wholeSong"], "w": 1.3}]},

    {"chapter": "5", "title": "Write your own parts"},
    {"say": "To change a part, press EDIT on its card, or double-click a box on ARRANGE. The piano roll opens on that part, in that section.",
     "shots": [{"pre": "song-before-edit", "f": "roll", "click": "chordsCard.edit", "hl": ["chordsCard.edit"], "zoom": ["chordsCard", "bassCard"]},
               {"f": "roll", "hl": ["rollGrid"], "w": 1.3}]},
    {"say": "Draw, move and stretch notes on the grid. The song's chords run along the top, and you can click one to pick your own.",
     "shots": [{"f": "roll", "hl": ["notes"]},
               {"f": "roll", "hl": ["ruler"], "zoom": ["ruler"]}]},
    {"say": "The panel on the right builds parts for you: stamp the chords, strum, arpeggiate, quantize, humanize, or ask for a new idea.",
     "shots": [{"f": "roll", "hl": ["panel"], "zoom": ["panel"], "w": 1.4},
               {"pre": "roll", "f": "roll-idea", "click": "a_new_idea_for_this_part", "hl": ["notes"]}]},

    {"chapter": "6", "title": "Play it on your keyboard"},
    {"say": "Have a MIDI keyboard? It plays the highlighted card's sound on SONG, and the open part here. In the app, click the MIDI light to pick your keyboard. In your DAW, select Song Savant's track.",
     "shots": [{"f": "roll-record", "hl": ["midiLight"], "point": "midiLight", "zoom": ["midiLight", "help"], "w": 1.4},
               {"f": "roll-idea", "hl": ["rollGrid"]}]},
    {"say": "Turn on the METRONOME and press RECORD. After a bar of clicks, the section loops while you play along, and your notes go in where you play them.",
     "shots": [{"pre": "roll-idea", "f": "roll-record", "click": "record", "hl": ["record", "metronome"], "zoom": ["record", "metronome", "with_the_band"]},
               {"f": "roll-record", "hl": ["notes"], "w": 1.3}]},
    {"say": "Press RECORD again to stop. UNDO takes the whole take back, and QUANTIZE tidies the timing.",
     "shots": [{"pre": "roll-record", "f": "roll-recorded", "click": "record", "hl": ["undo"], "zoom": ["record", "undo"]},
               {"f": "roll-recorded", "hl": ["quantize"], "point": "quantize", "zoom": ["panel"]}]},
    {"say": "Or press STEP INPUT and put the notes in one at a time: hold a key or a chord, let go, and the blue cursor moves on a step. No timing needed.",
     "shots": [{"pre": "roll-recorded", "f": "roll-step", "click": "step_input", "hl": ["step_input"], "zoom": ["record", "step_input", "metronome"]},
               {"f": "roll-step", "hl": ["notes"], "w": 1.4}]},
    {"say": "Press DONE when you're happy. Every chorus plays the same pattern, so change it once and every chorus follows, unless you make one different.",
     "shots": [{"f": "roll-step", "hl": ["done"], "point": "done", "zoom": ["done", "make_one_different"]},
               {"f": "roll-step", "hl": ["make_one_different"], "point": "make_one_different", "zoom": ["done", "make_one_different"]}]},

    {"chapter": "7", "title": "Learn and get coached"},
    {"say": "MASTERCLASS teaches how your style is built: its sounds, its drums and bass, the chords in your key, the arrangement and the mix.",
     "shots": [{"pre": "roll-step", "f": "lesson", "click": "tab.masterclass", "hl": ["tab.masterclass"]},
               {"f": "lesson", "w": 1.4}]},
    {"say": "Once the song is yours, COACH MY SONG compares it with what your style usually does, and shows you what's working and what to try.",
     "shots": [{"pre": "lesson", "f": "coach", "click": "coach_my_song", "hl": ["coach_my_song"]},
               {"f": "coach", "hl": ["coach.thetip", "coach.working"], "w": 1.5}]},
    {"say": "Here it noticed the longer intro: the hook now takes a while to arrive. SHOW ME takes you to the spot, FIX IT makes the change, and UNDO takes it back. If you meant it, press I MEANT THAT.",
     "shots": [{"f": "coach", "hl": ["coach.thetip"], "zoom": ["coach.thetip"], "w": 1.3},
               {"f": "coach", "hl": ["coach.theshow"], "point": "coach.theshow", "zoom": ["coach.thetip"], "w": 0.7},
               {"pre": "coach", "f": "coach-fixed", "click": "coach.thefix", "hl": ["undo"], "w": 1.1},
               {"f": "coach-fixed", "hl": ["undo"], "point": "undo", "w": 0.7}]},

    {"chapter": "8", "title": "Save and come back"},
    {"say": "SAVE SONG keeps everything: the sounds, the map, the chords and your notes.",
     "shots": [{"pre": "song-final", "f": "song-saved", "click": "save_song", "hl": ["save_song"], "zoom": ["form"]}]},
    {"say": "Your songs wait in FAVS, next to the sounds you hearted, ready to open again.",
     "shots": [{"pre": "song-saved", "f": "favs", "click": "tab.favs", "hl": ["songs", "sounds"]}]},
    {"say": "LIBRARY lists every sound Song Savant found in your instruments. Search it, and click a sound to see what it is.",
     "shots": [{"pre": "favs", "f": "library", "click": "tab.library", "hl": ["search"]},
               {"f": "library", "w": 1.2}]},
    {"say": "Press the question mark, or F1, for the full manual.",
     "shots": [{"pre": "song-final", "f": "manual", "click": "help"}]},
    {"say": "And the sun button switches between dark and light.",
     "shots": [{"pre": "song-final", "f": "song-light", "click": "theme"}]},

    {"card": "outro"},
]

TITLE = "Getting started"
OUTRO_LINE = "The right sounds. Your song."
OUTRO_SAY = "That's Song Savant: the right sounds from your own library, and a song that's ready to finish in your DAW. Now go make something great."
INTRO_SAY = ("Welcome to Song Savant. In the next few minutes, you'll describe a song, get sounds for it from the instruments you "
             "already own, starter parts to drag into your DAW, a map of the whole song, your own parts from your keyboard, "
             "and coaching on the song you make.")
DRAG_LABEL = "Keys - starter part.mid"
