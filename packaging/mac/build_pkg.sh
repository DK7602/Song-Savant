#!/usr/bin/env bash
# Builds "Song-Savant-<version>-macOS.pkg" that installs:
#   AU         -> /Library/Audio/Plug-Ins/Components
#   VST3       -> /Library/Audio/Plug-Ins/VST3
#   Standalone -> /Applications
#
# usage: packaging/mac/build_pkg.sh <artefacts dir> <output dir>
set -euo pipefail

ART="${1:?artefacts dir, e.g. build/SongSavant_artefacts/Release}"
OUT="${2:-dist}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
VERSION="$(grep -Eo 'project\(SongSavant VERSION [0-9.]+' "$ROOT/CMakeLists.txt" | awk '{print $3}')"
ID="com.grabberaudio.songsavant"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT" "$WORK/au" "$WORK/vst3" "$WORK/app"

cp -R "$ART/AU/Song Savant.component"   "$WORK/au/"
cp -R "$ART/VST3/Song Savant.vst3"      "$WORK/vst3/"
cp -R "$ART/Standalone/Song Savant.app" "$WORK/app/"

pkgbuild --root "$WORK/au"   --install-location "/Library/Audio/Plug-Ins/Components" --identifier "$ID.au"   --version "$VERSION" "$WORK/au.pkg"
pkgbuild --root "$WORK/vst3" --install-location "/Library/Audio/Plug-Ins/VST3"       --identifier "$ID.vst3" --version "$VERSION" "$WORK/vst3.pkg"
pkgbuild --root "$WORK/app"  --install-location "/Applications"                      --identifier "$ID.app"  --version "$VERSION" "$WORK/app.pkg"

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Song Savant ${VERSION}</title>
    <welcome file="welcome.html" mime-type="text/html"/>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline>
        <line choice="au"/>
        <line choice="vst3"/>
        <line choice="app"/>
    </choices-outline>
    <choice id="au"   title="Audio Unit (Logic Pro, GarageBand, ...)" description="Installs to /Library/Audio/Plug-Ins/Components"><pkg-ref id="$ID.au"/></choice>
    <choice id="vst3" title="VST3 (Cubase, Ableton Live, FL Studio, Studio One, Reaper, Bitwig ...)" description="Installs to /Library/Audio/Plug-Ins/VST3"><pkg-ref id="$ID.vst3"/></choice>
    <choice id="app"  title="Standalone app" description="Installs Song Savant.app to /Applications"><pkg-ref id="$ID.app"/></choice>
    <pkg-ref id="$ID.au">au.pkg</pkg-ref>
    <pkg-ref id="$ID.vst3">vst3.pkg</pkg-ref>
    <pkg-ref id="$ID.app">app.pkg</pkg-ref>
</installer-gui-script>
XML

mkdir -p "$WORK/resources"
cat > "$WORK/resources/welcome.html" <<'HTML'
<html><body style="font-family:-apple-system,Helvetica;">
<h2>Song Savant</h2>
<p>Suggests sounds from the instruments and presets you already own for the song you're making.</p>
<p>This is the Phase 0 setup check: open Song Savant from Applications and click <b>SCAN MY SETUP</b>.</p>
</body></html>
HTML

productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/resources" --package-path "$WORK" \
             "$OUT/Song-Savant-${VERSION}-macOS.pkg"

# the raw bundles too, for copying by hand
mkdir -p "$WORK/bundles"
cp -R "$WORK/au/"* "$WORK/vst3/"* "$WORK/app/"* "$WORK/bundles/"
( cd "$WORK/bundles" && zip -qry "$WORK/bundles.zip" . )
mv "$WORK/bundles.zip" "$OUT/Song-Savant-${VERSION}-macOS-bundles.zip"

echo "Built $OUT/Song-Savant-${VERSION}-macOS.pkg"
