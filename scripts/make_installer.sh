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

# AI tutor: the helper pack (see make_tutor_pack.sh) and the models it can run. The plugin reads these from the
# feed, so a model can be changed here later without touching the plugin's code. Listed lightest first; each Mac
# takes the smartest one it can run. Checksums and sizes are checked on every build by the tutor check.
TUTOR_PACK="dist-tutor/MusicallyUnorthodox-tutor-macos.zip"
TUTOR_SHA=""; TUTOR_VERSION=""
if [ -f "$TUTOR_PACK" ]; then
  cp "$TUTOR_PACK" dist/
  TUTOR_SHA="$(shasum -a 256 "$TUTOR_PACK" | cut -d' ' -f1)"
  TUTOR_VERSION="$(unzip -p "$TUTOR_PACK" tutor/VERSION)"
fi

BUILDNUM="$BUILDNUM" VERSION="$VERSION" ZIPSHA="$SHA" TUTOR_SHA="$TUTOR_SHA" TUTOR_VERSION="$TUTOR_VERSION" python3 - <<'PY'
import json, os, subprocess
e = os.environ
MODELS = [
    {"id": "light", "name": "Llama 3.2 3B",
     "url": "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf",
     "sha256": "6c1a2b41161032677be168d354123594c0e6e67d2b9227c84f296ad037c728ff", "bytes": 2019377696},
    {"id": "smart", "name": "Qwen3.5 4B",
     "url": "https://huggingface.co/bartowski/Qwen_Qwen3.5-4B-GGUF/resolve/main/Qwen_Qwen3.5-4B-Q4_K_M.gguf",
     "sha256": "13c16f426047e2de38cd075bdade4a7bcbc8c774384876f677740cda65f8a983", "bytes": 3013027808,
     "minMemoryGb": 16, "appleSilicon": True},
]
try:
    body = subprocess.run(["git", "log", "-1", "--pretty=%B"], capture_output=True, text=True).stdout
except Exception:
    body = ""
notes = "\n".join([l for l in body.splitlines() if not l.startswith(("Co-Authored-By", "Claude-Session"))][:24]).strip()
feed = {"build": int(e["BUILDNUM"]), "version": e["VERSION"], "file": "MusicallyUnorthodox-bundles.zip", "sha256": e["ZIPSHA"], "notes": notes}
if e["TUTOR_SHA"]:
    light = MODELS[0]
    feed["tutor"] = {"version": e["TUTOR_VERSION"], "file": "MusicallyUnorthodox-tutor-macos.zip", "sha256": e["TUTOR_SHA"],
                     # "model" is what plugins up to build 8 read; newer ones read "models"
                     "model": {k: light[k] for k in ("name", "url", "sha256", "bytes")},
                     "models": MODELS}
else:
    print("NOTE: no AI tutor section in the feed (the helper pack is missing)")
open("dist/latest.json", "w").write(json.dumps(feed, indent=1) + "\n")
PY
echo "Installer written to dist/"
ls -la dist
cat dist/latest.json
