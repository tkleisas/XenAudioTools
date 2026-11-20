# Releasing UPIC Studio on Google Play

The Android package name is **`gr.tkleisas.upicstudio`**. It cannot change after
the first Play upload.

## One-time Play Console setup

1. Create a Google Play developer account and register the app with package
   `gr.tkleisas.upicstudio`.
2. Enable **Play App Signing**. Google holds the app-signing key; this repository
   signs uploads with an upload key.
3. Create an **upload key**:

   ```bash
   Java_HOME=/path/to/jdk17 android/scripts/create_upload_keystore.sh
   ```

   This writes `~/.upicstudio/upic-upload-keystore.jks` and
   `~/.upicstudio/keystore.properties`. **Back both files up.** If you lose the
   upload key, you must request an upload-key reset from Google.

4. Configure the GitHub signing secrets:

   ```bash
   android/scripts/configure_github_secrets.sh
   ```

   The workflow uses:
   - `ANDROID_KEYSTORE_BASE64`
   - `ANDROID_KEYSTORE_PASSWORD`
   - `ANDROID_KEY_ALIAS`
   - `ANDROID_KEY_PASSWORD`

5. For automated Play uploads, create a Google Cloud service account with Play
   Console API access, grant it access in **Play Console → Users and
   permissions**, download the JSON key, and add it as
   `GOOGLE_PLAY_SERVICE_ACCOUNT_JSON`.
6. Create a protected GitHub environment named `play-store` if you want manual
   approval before publishing.

## Store listing

Fastlane-ready metadata is under `android/fastlane/metadata/android/en-US/`:

- `title.txt`
- `short_description.txt`
- `full_description.txt`
- `changelogs/1.txt`
- `images/icon.png` — 512×512
- `images/featureGraphic.png` — 1024×500
- `images/phoneScreenshots/phoneScreenshot1.png`
- `images/phoneScreenshots/phoneScreenshot2.png`

See [`store/play-console.md`](store/play-console.md) for suggested Play Console
answers, including Data safety and content rating.

## Privacy policy

The policy is in [`PRIVACY.md`](PRIVACY.md) and
[`../docs/privacy.html`](../docs/privacy.html). Enable GitHub Pages for the
`docs/` directory to publish:

`https://tkleisas.github.io/XenAudioTools/privacy.html`

Use that URL in **Play Console → App content → Privacy policy**.

The app collects no data and has no network permission, so the Data safety form
should report **no data collected or shared**.

## Versioning

Play requires a monotonically increasing `versionCode`. Edit
`android/version.properties` before every upload:

```bash
android/scripts/set_version.sh 2 0.1.1
```

Do not reuse a `versionCode`, even if an upload was rejected. `versionName` is
user-facing and may follow the suite version.

## Local release build

```bash
export JAVA_HOME=/path/to/jdk17
export ANDROID_HOME=$HOME/Android/Sdk
android/scripts/build_release.sh
```

Outputs:

- `android/app/build/outputs/bundle/release/app-release.aab` — upload this
- `android/app/build/outputs/apk/release/app-release.apk` — phone QA
- `android/app/build/outputs/mapping/release/mapping.txt` — keep for crash
  deobfuscation

The script verifies the AAB signature, APK signature, and 16 KB zip alignment.

## CI builds and publishing

`.github/workflows/android-release.yml` builds a signed AAB on every `v*` tag
and on manual dispatch.

- Tag build: **Actions → Android Release**, download the signed AAB artifact.
- Manual publish: run the workflow with `publish`, `track`, and
  `release_status`. Publishing uses the `play-store` environment if configured.
- Play Console manual upload is also supported; it is the safest first release.

## First release checklist

1. Bump `version.properties`.
2. Build and install the release APK on a physical phone.
3. Test offline operation, arc drawing, all four generators, live GENDYN notes,
   transport, and opening/saving a `.xenpage` file.
4. Upload the AAB to **Internal testing** and install from the Play link.
5. Complete App content:
   - Privacy policy URL
   - Ads: no
   - App access: all functionality available without login
   - Content rating questionnaire
   - Target audience
   - Data safety: no collection/sharing
   - Government/news/health declarations as applicable
6. Complete the store listing with the assets above.
7. Promote to closed testing if required for a new personal developer account.
8. Roll out to production in stages and monitor Android vitals.

## Signing notes

- The upload keystore is local at `~/.upicstudio/` and is not in git.
- The certificate SHA-256 for the generated upload key is shown by
  `keytool -list -v -keystore ~/.upicstudio/upic-upload-keystore.jks -alias upic-upload`.
- Keep `mapping.txt` from every release. R8 is enabled for release builds.
- The native library is linked with 16 KB page alignment for Android 15+
  devices.
