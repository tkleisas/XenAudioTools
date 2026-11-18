#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

JUCE_DIR="${JUCE_DIR:-../ans-1964/third_party/JUCE}"
BUILD_DIR="${BUILD_DIR:-build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DXEN_JUCE_DIR="$JUCE_DIR"
cmake --build "$BUILD_DIR" -j"$(nproc)"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo
echo "VST3: $BUILD_DIR/vst3/XenakisUPIC_artefacts/$BUILD_TYPE/VST3/Xenakis UPIC.vst3"
echo "App:  $BUILD_DIR/app/XenakisUPICStudio_artefacts/$BUILD_TYPE/Xenakis UPIC Studio"
