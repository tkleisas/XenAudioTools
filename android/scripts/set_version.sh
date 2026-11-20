#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Set Play versionCode and versionName in version.properties.
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 VERSION_CODE VERSION_NAME" >&2
    echo "example: $0 2 0.1.1" >&2
    exit 1
fi

CODE="$1"
NAME="$2"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FILE="$ROOT/version.properties"

[[ "$CODE" =~ ^[1-9][0-9]*$ ]] || { echo "versionCode must be a positive integer" >&2; exit 1; }
[[ "$NAME" =~ ^[0-9]+(\.[0-9]+){1,3}([-+][A-Za-z0-9.-]+)?$ ]] || {
    echo "versionName must look like 0.1.1" >&2
    exit 1
}

CURRENT_CODE="$(sed -n 's/^versionCode=//p' "$FILE" 2>/dev/null || true)"
if [[ -n "$CURRENT_CODE" && "$CODE" -le "$CURRENT_CODE" ]]; then
    echo "Refusing to decrease versionCode: $CODE <= $CURRENT_CODE" >&2
    exit 1
fi

{
    echo "# Bump versionCode for every Play Store upload."
    echo "versionCode=$CODE"
    echo "versionName=$NAME"
} > "$FILE"

echo "UPIC Studio $NAME ($CODE)"
