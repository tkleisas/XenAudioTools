#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT/build}"
VERSION="${2:-0.1.0}"
OUT_DIR="${3:-$ROOT/dist}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "missing build directory: $BUILD_DIR" >&2
    exit 1
fi
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

VST3_SRC="$BUILD_DIR/vst3/XenakisUPIC_artefacts/$BUILD_TYPE/VST3/Xenakis UPIC.vst3"
APP_SRC="$BUILD_DIR/app/XenakisUPICStudio_artefacts/$BUILD_TYPE/Xenakis UPIC Studio"

[[ -d "$VST3_SRC" ]] || { echo "missing $VST3_SRC" >&2; exit 1; }
[[ -x "$APP_SRC" ]] || { echo "missing $APP_SRC" >&2; exit 1; }

rm -rf "$OUT_DIR/staging"
mkdir -p "$OUT_DIR/staging/vst3" "$OUT_DIR/staging/studio"

VST3_ROOT="$OUT_DIR/staging/vst3"
cp -a "$VST3_SRC" "$VST3_ROOT/"
cp -a "$ROOT/presets" "$VST3_ROOT/"
cp "$ROOT/README.md" "$ROOT/NOTICE.md" "$ROOT/LICENSE" "$VST3_ROOT/"
cp "$ROOT/packaging/INSTALL.txt" "$VST3_ROOT/"
mkdir -p "$VST3_ROOT/docs"
cp "$ROOT/docs/concepts.md" "$VST3_ROOT/docs/"
( cd "$VST3_ROOT" && zip -qr "$OUT_DIR/xenakis-upic-${VERSION}-linux-vst3.zip" . )

STUDIO_NAME="xenakis-upic-studio-${VERSION}"
STUDIO_ROOT="$OUT_DIR/staging/studio/$STUDIO_NAME"
mkdir -p "$STUDIO_ROOT"
cp "$APP_SRC" "$STUDIO_ROOT/Xenakis UPIC Studio"
cp -a "$ROOT/presets" "$STUDIO_ROOT/"
cp "$ROOT/README.md" "$ROOT/NOTICE.md" "$ROOT/LICENSE" "$STUDIO_ROOT/"
cp "$ROOT/packaging/INSTALL.txt" "$STUDIO_ROOT/"
mkdir -p "$STUDIO_ROOT/docs"
cp "$ROOT/docs/concepts.md" "$STUDIO_ROOT/docs/"
( cd "$OUT_DIR/staging/studio" && tar -czf "$OUT_DIR/${STUDIO_NAME}-linux-x86_64.tar.gz" "$STUDIO_NAME" )

rm -rf "$OUT_DIR/staging"
( cd "$OUT_DIR" && sha256sum xenakis-upic-*.zip xenakis-upic-*.tar.gz > SHA256SUMS )
ls -lh "$OUT_DIR"
