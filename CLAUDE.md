# Song Savant: notes for Claude

- **Read `docs/ROADMAP.md` first.** It records every product decision; keep it up to date in the same commit
  as any change to a decision. Starter MIDI parts are part of version 1.0 and must not be dropped.
- Same foundation and conventions as Groove Grabber (https://github.com/DK7602/Groove-Grabber): JUCE 8 + CMake,
  one codebase for the standalone app and the plug-in, `Source/core` (no UI) and `Source/ui`, a headless test
  runner (`SSTests`), GitHub Actions building Windows / macOS / Linux with installers.
- Maker label "Grabber Audio", manufacturer code `Grbr`, plug-in code `SgSv`: never change the codes.
- **The in-app manual** is `Resources/Manual.md` (the ? button / F1, `Source/ui/HelpPanel.*`): update it in the same
  commit whenever behaviour, a button's name or a step changes. `{VERSION}` is filled in at run time.
- Build and test (Linux needs the JUCE dependencies listed in `.github/workflows/build.yml`):
  `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ./build/SSTests_artefacts/Release/SSTests`
  Add `--shots DIR` (under `xvfb-run` on a headless machine) to render the screens to PNGs.
- The owner is a musician, not a programmer: explain things in plain language, no jargon.
- The window zooms (1.8.0): `SongSavantEditor::resized` lays `MainView` out at the window's size divided by
  `ss::ui::zoomFor` (1 up to 1440 x 920, then the smaller of width / 1440 and height / 820) and scales it up, so a big
  window has bigger words. Give popup menus their component (`withTargetComponent`) so they open at the same zoom, and
  draw cached pictures at the graphics context's physical pixel scale (the piano roll's grid does). Groove Grabber does
  the same.
- COACH MY SONG (1.9.0, MASTERCLASS): the rules are in `Source/core/Coach.*` (no UI; offline, from the StyleGuide), the
  page in `Source/ui/CoachView.*`. Tips only, no score (the owner's choice). A FIX IT changes only the tempo, the map or
  your patterns, and the coach's UNDO restores a `coach::Snapshot` only while nothing else changed the song (`sameSong`).
  A new check needs a stable tip id (I MEANT THAT stores it in `SongSession::coachDismissed`) and a test in `testCoach`.
- The video tutorial: `bash tools/tutorial/make.sh` makes "Song Savant Tutorial.mp4" (about 5 minutes). `SSTests --tutorial
  DIR "description"` (`Tests/TestTutorial.cpp`) runs the real window at 1920 x 1080 on the tests' made-up computer through
  the workflow and saves each step as a picture with where things are (`frames.json` "marks", from the pages'
  `tutorialArea` getters), plus `beat.wav` (the song's starter parts, played by the sketch player). Menus are drawn (a
  real popup closes itself under xvfb). `tools/tutorial/script.py` is what's said and shown: change it when a button's
  name or a step changes, and keep it true to the app. `make_video.py` (adapted from Groove Grabber's) adds the cursor,
  highlights, subtitles, cards, the narrator (Piper, offline; LJ Speech is public domain) and the music;
  `--preview 12,60` writes stills, `--schedule` lists when each part starts.
- Never commit anything read from the owner's computer (scan reports can contain their file names).
