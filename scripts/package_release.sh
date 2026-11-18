#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT/build}"
VERSION="${2:-0.1.0}"
OUT_DIR="${3:-$ROOT/dist}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
HOST_OS="$(uname -s)"

if [[ -z "${PLATFORM:-}" ]]; then
    case "$HOST_OS" in
        Darwin)                 PLATFORM="macos-universal" ;;
        MINGW*|MSYS*|CYGWIN*)  PLATFORM="windows-x86_64" ;;
        *)                      PLATFORM="linux-x86_64" ;;
    esac
fi

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "missing build directory: $BUILD_DIR" >&2
    exit 1
fi
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

VST3_SRC="$BUILD_DIR/vst3/XenakisUPIC_artefacts/$BUILD_TYPE/VST3/Xenakis UPIC.vst3"
APP_BASE="$BUILD_DIR/app/XenakisUPICStudio_artefacts/$BUILD_TYPE/Xenakis UPIC Studio"
APP_SRC=""
if [[ -d "$APP_BASE.app" ]]; then
    APP_SRC="$APP_BASE.app"
elif [[ -f "$APP_BASE.exe" ]]; then
    APP_SRC="$APP_BASE.exe"
elif [[ -f "$APP_BASE" ]]; then
    APP_SRC="$APP_BASE"
fi

[[ -d "$VST3_SRC" ]] || { echo "missing $VST3_SRC" >&2; exit 1; }
[[ -n "$APP_SRC" ]] || { echo "missing $APP_BASE(.app|.exe)" >&2; exit 1; }

if [[ "$HOST_OS" == "Darwin" ]]; then
    codesign --force --deep --sign - "$APP_SRC" >/dev/null 2>&1 \
        || echo "warning: ad-hoc codesign failed for $APP_SRC"
    codesign --force --deep --sign - "$VST3_SRC" >/dev/null 2>&1 \
        || echo "warning: ad-hoc codesign failed for $VST3_SRC"
fi

rm -rf "$OUT_DIR/staging"
mkdir -p "$OUT_DIR/staging/vst3" "$OUT_DIR/staging/studio"

VST3_ROOT="$OUT_DIR/staging/vst3"
VST3_COPY="$VST3_ROOT/$(basename "$VST3_SRC")"
cp -Rp "$VST3_SRC" "$VST3_ROOT/"
cp -Rp "$ROOT/presets" "$VST3_ROOT/"
cp -p "$ROOT/README.md" "$ROOT/NOTICE.md" "$ROOT/LICENSE" "$VST3_ROOT/"
cp -Rp "$ROOT/LICENSES" "$VST3_ROOT/"
cp -p "$ROOT/packaging/INSTALL.txt" "$VST3_ROOT/"
mkdir -p "$VST3_ROOT/docs"
cp -p "$ROOT/docs/concepts.md" "$VST3_ROOT/docs/"

if [[ "$PLATFORM" == windows-* ]]; then
    VST3_BIN="$VST3_COPY/Contents/x86_64-win/$(basename "$VST3_SRC")"
    if [[ -f "$VST3_BIN" ]]; then
        while IFS= read -r dll; do
            [[ -n "$dll" ]] && cp -p "$dll" "$(dirname "$VST3_BIN")/"
        done < <(ldd "$VST3_BIN" | grep -oE '/mingw64/bin/[^ ]+\.dll' | sort -u)
    fi
fi

( cd "$VST3_ROOT" && zip -qry "$OUT_DIR/xenakis-upic-${VERSION}-${PLATFORM}-vst3.zip" . )

STUDIO_NAME="xenakis-upic-studio-${VERSION}"
STUDIO_ROOT="$OUT_DIR/staging/studio/$STUDIO_NAME"
mkdir -p "$STUDIO_ROOT"
if [[ -d "$APP_SRC" ]]; then
    cp -Rp "$APP_SRC" "$STUDIO_ROOT/"
else
    cp -p "$APP_SRC" "$STUDIO_ROOT/"
fi
cp -Rp "$ROOT/presets" "$STUDIO_ROOT/"
cp -p "$ROOT/README.md" "$ROOT/NOTICE.md" "$ROOT/LICENSE" "$STUDIO_ROOT/"
cp -Rp "$ROOT/LICENSES" "$STUDIO_ROOT/"
cp -p "$ROOT/packaging/INSTALL.txt" "$STUDIO_ROOT/"
mkdir -p "$STUDIO_ROOT/docs"
cp -p "$ROOT/docs/concepts.md" "$STUDIO_ROOT/docs/"

if [[ "$PLATFORM" == windows-* && "$APP_SRC" == *.exe ]]; then
    while IFS= read -r dll; do
        [[ -n "$dll" ]] && cp -p "$dll" "$STUDIO_ROOT/"
    done < <(ldd "$APP_SRC" | grep -oE '/mingw64/bin/[^ ]+\.dll' | sort -u)
fi

if [[ "$PLATFORM" == linux-* ]]; then
    ( cd "$OUT_DIR/staging/studio" && tar -czf "$OUT_DIR/${STUDIO_NAME}-${PLATFORM}.tar.gz" "$STUDIO_NAME" )
else
    ( cd "$OUT_DIR/staging/studio" && zip -qry "$OUT_DIR/${STUDIO_NAME}-${PLATFORM}.zip" "$STUDIO_NAME" )
fi

rm -rf "$OUT_DIR/staging"

shopt -s nullglob
files=( "$OUT_DIR"/xenakis-upic-*.zip "$OUT_DIR"/xenakis-upic-*.tar.gz )
if (( ${#files[@]} == 0 )); then
    echo "no release archives were produced" >&2
    exit 1
fi
if command -v sha256sum >/dev/null 2>&1; then
    ( cd "$OUT_DIR" && sha256sum "${files[@]##*/}" > SHA256SUMS )
else
    ( cd "$OUT_DIR" && shasum -a 256 "${files[@]##*/}" > SHA256SUMS )
fi

ls -lh "$OUT_DIR"
