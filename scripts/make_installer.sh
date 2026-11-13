#!/bin/bash
# Packages the built AU + VST3 into a double-click .pkg installer (installs to /Library/Audio/Plug-Ins)
# and zips the raw bundles. Usage: bash scripts/make_installer.sh <artefacts dir> [build number]
set -e
cd "$(dirname "$0")/.."
ART="${1:-build-mac/MusicallyUnorthodox_artefacts/Release}"
BUILD="${2:-local}"
BASE="$(grep -m1 'set(MU_VERSION_BASE' CMakeLists.txt | sed -E 's/.*"([0-9.]+)".*/\1/')"
if [ "$BUILD" = "local" ]; then VERSION="$BASE.0"; else VERSION="$BASE.$BUILD"; fi
NAME="Musically Unorthodox"

AU="$ART/AU/$NAME.component"
VST3="$ART/VST3/$NAME.vst3"
APP="$ART/Standalone/$NAME.app"
[ -d "$AU" ] || { echo "AU not found at $AU"; exit 1; }
[ -d "$VST3" ] || { echo "VST3 not found at $VST3"; exit 1; }

rm -rf dist pkgroot
mkdir -p dist pkgroot/Library/Audio/Plug-Ins/Components pkgroot/Library/Audio/Plug-Ins/VST3
cp -R "$AU" pkgroot/Library/Audio/Plug-Ins/Components/
cp -R "$VST3" pkgroot/Library/Audio/Plug-Ins/VST3/
if [ -d "$APP" ]; then
  mkdir -p pkgroot/Applications
  cp -R "$APP" pkgroot/Applications/
fi

# Ad-hoc sign so Gatekeeper is happier on Apple Silicon (replace with a Developer ID later).
codesign --force --deep --sign - "pkgroot/Library/Audio/Plug-Ins/Components/$NAME.component" || true
codesign --force --deep --sign - "pkgroot/Library/Audio/Plug-Ins/VST3/$NAME.vst3" || true
[ -d "pkgroot/Applications/$NAME.app" ] && codesign --force --deep --sign - "pkgroot/Applications/$NAME.app" || true

# After installing, hand the two plugin bundles to the person at the keyboard, so the plugin can update
# itself later without asking for an administrator password.
rm -rf pkgscripts && mkdir pkgscripts
cat > pkgscripts/postinstall <<'POST'
#!/bin/bash
OWNER="$(stat -f%Su /dev/console 2>/dev/null)"
if [ -n "$OWNER" ] && [ "$OWNER" != "root" ]; then
  chown -R "$OWNER" "/Library/Audio/Plug-Ins/Components/Musically Unorthodox.component" 2>/dev/null || true
  chown -R "$OWNER" "/Library/Audio/Plug-Ins/VST3/Musically Unorthodox.vst3" 2>/dev/null || true
fi
exit 0
POST
chmod +x pkgscripts/postinstall

pkgbuild --root pkgroot --scripts pkgscripts --identifier com.musicallyunorthodox.mixassistant.pkg --version "$VERSION" \
         --install-location / "dist/MusicallyUnorthodox-$VERSION-build$BUILD.pkg"

ZIP="MusicallyUnorthodox-$VERSION-build$BUILD-bundles.zip"
( cd pkgroot/Library/Audio/Plug-Ins && zip -qry "../../../../dist/$ZIP" Components VST3 )
rm -rf pkgroot pkgscripts

# Update feed read by the plugin: which build is newest, where its bundles are, and a checksum.
# Stable names so "releases/latest/download/<name>" always points at the newest build.
cp "dist/$ZIP" dist/MusicallyUnorthodox-bundles.zip
SHA="$(shasum -a 256 "dist/$ZIP" | cut -d' ' -f1)"
NOTES="$(git log -1 --pretty=%B 2>/dev/null | grep -v -e '^Co-Authored-By' -e '^Claude-Session' | head -24 | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read().strip()))' 2>/dev/null || echo '""')"
BUILDNUM="$BUILD"; [ "$BUILD" = "local" ] && BUILDNUM=0
cat > dist/latest.json <<JSON
{ "build": $BUILDNUM, "version": "$VERSION", "file": "MusicallyUnorthodox-bundles.zip", "sha256": "$SHA", "notes": $NOTES }
JSON
echo "Installer written to dist/"
ls -la dist
