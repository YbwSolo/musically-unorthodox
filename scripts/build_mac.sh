#!/bin/bash
# Builds Musically Unorthodox (AU + VST3 + Standalone, universal arm64/x86_64) and validates the AU.
# Run from anywhere:  bash scripts/build_mac.sh            (Release)
#                     bash scripts/build_mac.sh Debug
# Needs: Xcode command line tools (xcode-select --install) and CMake (brew install cmake).
set -e
cd "$(dirname "$0")/.."
CONFIG="${1:-Release}"

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake not found. Install it with:  brew install cmake"; exit 1
fi

cmake -B build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DMU_BUILD_TESTS=ON
cmake --build build-mac --config "$CONFIG" --target MusicallyUnorthodox_AU MusicallyUnorthodox_VST3 MusicallyUnorthodox_Standalone MUTests -- -quiet

echo
echo "== DSP tests =="
"./build-mac/${CONFIG}/MUTests" || { echo "DSP tests failed"; exit 1; }

echo
echo "== auval =="
# COPY_PLUGIN_AFTER_BUILD already put the AU in ~/Library/Audio/Plug-Ins/Components.
# Reset the AU cache so Logic and auval see the fresh build.
killall -9 AudioComponentRegistrar 2>/dev/null || true
auval -v aufx Muxa Muno || { echo "auval failed - open Logic's Plug-in Manager and rescan, then run: auval -v aufx Muxa Muno"; exit 1; }

echo
echo "Done. Installed:"
echo "  ~/Library/Audio/Plug-Ins/Components/Musically Unorthodox.component"
echo "  ~/Library/Audio/Plug-Ins/VST3/Musically Unorthodox.vst3"
echo "  build-mac/${CONFIG}/Musically Unorthodox.app  (standalone)"
echo "Restart Logic (or rescan in Plug-in Manager) and look under Audio Units > Musically Unorthodox."
