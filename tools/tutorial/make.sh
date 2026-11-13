#!/usr/bin/env bash
# Makes the Song Savant video tutorial: the app's real screens (SSTests --tutorial, on the tests' made-up computer),
# a narrator (Piper, an offline voice) and the song's starter parts underneath. Needs the SSTests build, xvfb-run,
# ffmpeg, fc-match, curl, and python3 with numpy and Pillow.
#
#   bash tools/tutorial/make.sh ["OUT.mp4"]
#
# WORK (default /tmp/ss-tutorial) keeps the voice, the pictures and the spoken lines between runs.
# VOICE is a Piper voice name; the default, LJ Speech, is public domain. Check a voice's licence before using
# another one in a video you'll share (several are non-commercial).
# SONG is the description the tutorial types (it needs a palette with every card filled on the made-up computer).
set -euo pipefail
cd "$(dirname "$0")/../.."
WORK=${WORK:-/tmp/ss-tutorial}
VOICE=${VOICE:-en_US-ljspeech-high}
SONG=${SONG:-modern west coast rap, 94 BPM in A minor}
OUT=${1:-$WORK/Song Savant Tutorial.mp4}
mkdir -p "$WORK/voices"

# the voice
if [ ! -x "$WORK/venv/bin/piper" ]; then
  python3 -m venv "$WORK/venv"
  "$WORK/venv/bin/pip" install -q piper-tts
fi
IFS=- read -r LANGCODE NAME QUALITY <<< "$VOICE"
for ext in onnx onnx.json; do
  f="$WORK/voices/$VOICE.$ext"
  [ -s "$f" ] || curl -sSfL -o "$f" "https://huggingface.co/rhasspy/piper-voices/resolve/main/${LANGCODE%%_*}/$LANGCODE/$NAME/$QUALITY/$VOICE.$ext"
done

# the pictures and the music, from a fresh run of the app
cmake --build build --target SSTests -j"$(nproc)"
xvfb-run -a -s "-screen 0 1920x1200x24" ./build/SSTests_artefacts/Release/SSTests --tutorial "$WORK/capture" "$SONG"

# the video
python3 tools/tutorial/make_video.py "$WORK/capture" "$OUT" --piper "$WORK/venv/bin/piper" --voice "$WORK/voices/$VOICE.onnx"
