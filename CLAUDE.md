# Song Savant: notes for Claude

- **Read `docs/ROADMAP.md` first.** It records every product decision; keep it up to date in the same commit
  as any change to a decision. Starter MIDI parts are part of version 1.0 and must not be dropped.
- Same foundation and conventions as Groove Grabber (https://github.com/DK7602/Groove-Grabber): JUCE 8 + CMake,
  one codebase for the standalone app and the plug-in, `Source/core` (no UI) and `Source/ui`, a headless test
  runner (`SSTests`), GitHub Actions building Windows / macOS / Linux with installers.
- Maker label "Grabber Audio", manufacturer code `Grbr`, plug-in code `SgSv`: never change the codes.
- Build and test (Linux needs the JUCE dependencies listed in `.github/workflows/build.yml`):
  `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ./build/SSTests_artefacts/Release/SSTests`
  Add `--shots DIR` (under `xvfb-run` on a headless machine) to render the screens to PNGs.
- The owner is a musician, not a programmer: explain things in plain language, no jargon.
- Never commit anything read from the owner's computer (scan reports can contain their file names).
