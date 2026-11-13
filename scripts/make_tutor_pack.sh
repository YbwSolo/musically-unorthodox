#!/bin/bash
# Puts together the AI tutor helper that the plugin downloads when someone presses "Turn on AI tutor".
# The helper is the llama.cpp server (MIT licence), taken from that project's own published Mac builds at a
# pinned version and checked against pinned checksums. Output: dist-tutor/MusicallyUnorthodox-tutor-macos.zip
#
# To move to a newer llama.cpp: change LLAMA_TAG and the two checksums, and bump the number after the dash.
set -euo pipefail
cd "$(dirname "$0")/.."

LLAMA_TAG="b12368"
PACK_VERSION="$LLAMA_TAG-1"
SHA_arm64="21f8389c192d23b759eb6a3aee570e9249184ce821b4f301a1d6cdfd0cc77d08"
SHA_x64="bed70687d638d89946abd8bd17082fd96ee3e79ab2a40b34fda553213c946c4b"
FEED="$(grep -m1 'set(MU_UPDATE_BASE_URL' CMakeLists.txt | sed -E 's/.*"(https[^"]+)".*/\1/')"
NAME="MusicallyUnorthodox-tutor-macos.zip"

OUT="dist-tutor"
mkdir -p "$OUT"
PACK="$OUT/$NAME"

# The same pack is attached to every release. If the newest release already carries this version, reuse it,
# so a build never depends on the llama.cpp project keeping an old download available.
if curl -fsSL -o "$OUT/published.zip" "$FEED/$NAME" 2>/dev/null \
   && [ "$(unzip -p "$OUT/published.zip" tutor/VERSION 2>/dev/null)" = "$PACK_VERSION" ]; then
  mv "$OUT/published.zip" "$PACK"
  echo "Reusing the published tutor pack $PACK_VERSION"
  exit 0
fi
rm -f "$OUT/published.zip"

WORK="$(mktemp -d)"
mkdir -p "$WORK/tutor"
for ARCH in arm64 x64; do
  FILE="llama-$LLAMA_TAG-bin-macos-$ARCH.tar.gz"
  curl -fsSL -o "$WORK/$FILE" "https://github.com/ggml-org/llama.cpp/releases/download/$LLAMA_TAG/$FILE"
  WANT_VAR="SHA_$ARCH"
  GOT="$(shasum -a 256 "$WORK/$FILE" | cut -d' ' -f1)"
  if [ "$GOT" != "${!WANT_VAR}" ]; then echo "Checksum mismatch for $FILE: $GOT"; exit 1; fi
  mkdir -p "$WORK/unpacked-$ARCH" "$WORK/tutor/$ARCH"
  tar -xzf "$WORK/$FILE" -C "$WORK/unpacked-$ARCH"
  SRC="$WORK/unpacked-$ARCH/llama-$LLAMA_TAG"
  cp -a "$SRC/llama-server" "$WORK/tutor/$ARCH/"
  cp -a "$SRC"/*.dylib "$WORK/tutor/$ARCH/"
  cp "$SRC/LICENSE" "$WORK/tutor/LICENSE-llama.cpp.txt"
  chmod +x "$WORK/tutor/$ARCH/llama-server"
done
printf '%s' "$PACK_VERSION" > "$WORK/tutor/VERSION"
rm -f "$PACK"
( cd "$WORK" && zip -qry "$OLDPWD/$PACK" tutor )
rm -rf "$WORK"
echo "Built the tutor pack $PACK_VERSION"
ls -la "$OUT"
