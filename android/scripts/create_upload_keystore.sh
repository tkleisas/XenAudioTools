#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Create a local Play Store upload keystore and keystore.properties.
# Back up both files. Enroll in Play App Signing before the first upload.
set -euo pipefail

DIR="${UPIC_KEYSTORE_DIR:-$HOME/.upicstudio}"
ALIAS="${UPIC_KEY_ALIAS:-upic-upload}"
STORE="$DIR/upic-upload-keystore.jks"
PROPS="$DIR/keystore.properties"
KEYTOOL="${JAVA_HOME:+$JAVA_HOME/bin/}keytool"

if ! command -v "$KEYTOOL" >/dev/null 2>&1; then
    KEYTOOL=keytool
fi

if [[ -f "$STORE" || -f "$PROPS" ]]; then
    echo "Refusing to overwrite existing signing material in $DIR" >&2
    echo "Remove or move them first, or set UPIC_KEYSTORE_DIR elsewhere." >&2
    exit 1
fi

mkdir -p "$DIR"
chmod 700 "$DIR"

PASSWORD="$(openssl rand -base64 48 | tr -dc 'A-Za-z0-9' | head -c 32)"

"$KEYTOOL" -genkeypair \
    -keystore "$STORE" \
    -storetype PKCS12 \
    -alias "$ALIAS" \
    -keyalg RSA \
    -keysize 4096 \
    -validity 10000 \
    -storepass "$PASSWORD" \
    -keypass "$PASSWORD" \
    -dname "CN=UPIC Studio, OU=Android, O=UPIC Studio, C=GR"

cat > "$PROPS" <<PROPERTIES
storeFile=$STORE
storePassword=$PASSWORD
keyAlias=$ALIAS
keyPassword=$PASSWORD
PROPERTIES

chmod 600 "$STORE" "$PROPS"

echo "Created $STORE"
echo "Created $PROPS"
echo
echo "Back up both files and keep them out of the repository."
"$KEYTOOL" -list -v -keystore "$STORE" -storepass "$PASSWORD" -alias "$ALIAS" \
    | grep -E "Alias name|Certificate fingerprints|SHA256:" || true
