#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build, sign, and verify the Play Store AAB and release APK.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

: "${JAVA_HOME:?Set JAVA_HOME to a JDK 17+ installation}"
: "${ANDROID_HOME:?Set ANDROID_HOME to the Android SDK}"

export PATH="$JAVA_HOME/bin:$PATH"
BT="$(find "$ANDROID_HOME/build-tools" -mindepth 1 -maxdepth 1 -type d | sort -V | tail -1)"

if [[ ! -f "$HOME/.upicstudio/keystore.properties" &&
      ! -f "$ROOT/keystore.properties" ]]; then
    echo "No upload keystore configured." >&2
    echo "Run android/scripts/create_upload_keystore.sh or create keystore.properties." >&2
    exit 1
fi

./gradlew :app:bundleRelease :app:assembleRelease --no-daemon

AAB="app/build/outputs/bundle/release/app-release.aab"
APK="app/build/outputs/apk/release/app-release.apk"

"$JAVA_HOME/bin/jarsigner" -verify "$AAB"
"$BT/zipalign" -c -P 16 -v 4 "$APK" >/dev/null
"$BT/apksigner" verify --verbose --print-certs "$APK"

echo
echo "AAB: $ROOT/$AAB"
sha256sum "$AAB"
echo "APK: $ROOT/$APK"
sha256sum "$APK"
