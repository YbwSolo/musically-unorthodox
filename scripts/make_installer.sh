#!/bin/bash
# Packages the built AU + VST3 into a double-click .pkg installer (installs to /Library/Audio/Plug-Ins)
# and zips the raw bundles. Usage: bash scripts/make_installer.sh <artefacts dir> [build number]
set -e
cd "$(dirname "$0")/.."
ART="${1:-build-mac/MusicallyUnorthodox_artefacts/Release}"
BUILD="${2:-local}"
VERSION="$(grep -m1 'project(MusicallyUnorthodox VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
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

pkgbuild --root pkgroot --identifier com.musicallyunorthodox.mixassistant.pkg --version "$VERSION" \
         --install-location / "dist/MusicallyUnorthodox-$VERSION-build$BUILD.pkg"

( cd pkgroot/Library/Audio/Plug-Ins && zip -qry "../../../../dist/MusicallyUnorthodox-$VERSION-build$BUILD-bundles.zip" Components VST3 )
rm -rf pkgroot
echo "Installer written to dist/"
ls -la dist
