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
BUILDNUM="$BUILD"; [ "$BUILD" = "local" ] && BUILDNUM=0

# AI tutor: the helper pack (see make_tutor_pack.sh) and the model it runs. The plugin reads both from the feed,
# so the model can be changed here later without touching the plugin's code.
TUTOR_PACK="dist-tutor/MusicallyUnorthodox-tutor-macos.zip"
MODEL_NAME="Llama 3.2 3B"
MODEL_URL="https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf"
MODEL_SHA="6c1a2b41161032677be168d354123594c0e6e67d2b9227c84f296ad037c728ff"
TUTOR_SHA=""; TUTOR_VERSION=""; MODEL_BYTES=""
if [ -f "$TUTOR_PACK" ]; then
  cp "$TUTOR_PACK" dist/
  TUTOR_SHA="$(shasum -a 256 "$TUTOR_PACK" | cut -d' ' -f1)"
  TUTOR_VERSION="$(unzip -p "$TUTOR_PACK" tutor/VERSION)"
  MODEL_BYTES="$(curl -sIL "$MODEL_URL" | tr -d '\r' | awk 'tolower($1)=="content-length:" {v=$2} END {print v}')"
fi

BUILDNUM="$BUILDNUM" VERSION="$VERSION" ZIPSHA="$SHA" TUTOR_SHA="$TUTOR_SHA" TUTOR_VERSION="$TUTOR_VERSION" \
MODEL_NAME="$MODEL_NAME" MODEL_URL="$MODEL_URL" MODEL_SHA="$MODEL_SHA" MODEL_BYTES="$MODEL_BYTES" python3 - <<'PY'
import json, os, subprocess
e = os.environ
try:
    body = subprocess.run(["git", "log", "-1", "--pretty=%B"], capture_output=True, text=True).stdout
except Exception:
    body = ""
notes = "\n".join([l for l in body.splitlines() if not l.startswith(("Co-Authored-By", "Claude-Session"))][:24]).strip()
feed = {"build": int(e["BUILDNUM"]), "version": e["VERSION"], "file": "MusicallyUnorthodox-bundles.zip", "sha256": e["ZIPSHA"], "notes": notes}
if e["TUTOR_SHA"] and e["MODEL_BYTES"].isdigit() and int(e["MODEL_BYTES"]) > 100000000:
    feed["tutor"] = {"version": e["TUTOR_VERSION"], "file": "MusicallyUnorthodox-tutor-macos.zip", "sha256": e["TUTOR_SHA"],
                     "model": {"name": e["MODEL_NAME"], "url": e["MODEL_URL"], "sha256": e["MODEL_SHA"], "bytes": int(e["MODEL_BYTES"])}}
else:
    print("NOTE: no AI tutor section in the feed (pack or model size missing)")
open("dist/latest.json", "w").write(json.dumps(feed, indent=1) + "\n")
PY
echo "Installer written to dist/"
ls -la dist
cat dist/latest.json
