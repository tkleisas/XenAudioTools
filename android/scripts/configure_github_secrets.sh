#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Upload the local upload-key material to GitHub Actions secrets using gh.
# The secret values are never printed.
set -euo pipefail

PROPS="${UPIC_KEYSTORE_PROPERTIES:-$HOME/.upicstudio/keystore.properties}"
if [[ ! -f "$PROPS" ]]; then
    echo "Missing $PROPS" >&2
    exit 1
fi

STORE="$(sed -n 's/^storeFile=//p' "$PROPS")"
PASS="$(sed -n 's/^storePassword=//p' "$PROPS")"
ALIAS="$(sed -n 's/^keyAlias=//p' "$PROPS")"
KEYPASS="$(sed -n 's/^keyPassword=//p' "$PROPS")"

if [[ ! -f "$STORE" ]]; then
    echo "Missing keystore: $STORE" >&2
    exit 1
fi
if ! command -v gh >/dev/null 2>&1; then
    echo "gh CLI is required" >&2
    exit 1
fi

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT
chmod 600 "$TMP"

base64 -w0 "$STORE" > "$TMP"
gh secret set ANDROID_KEYSTORE_BASE64 < "$TMP"
printf '%s' "$PASS" > "$TMP"
gh secret set ANDROID_KEYSTORE_PASSWORD < "$TMP"
printf '%s' "$ALIAS" > "$TMP"
gh secret set ANDROID_KEY_ALIAS < "$TMP"
printf '%s' "$KEYPASS" > "$TMP"
gh secret set ANDROID_KEY_PASSWORD < "$TMP"

echo "GitHub signing secrets updated."
echo "GOOGLE_PLAY_SERVICE_ACCOUNT_JSON must be configured separately for publishing."
