# Play Console submission notes

## App identity

| Field | Value |
| --- | --- |
| App name | UPIC Studio |
| Package | `gr.tkleisas.upicstudio` |
| Category | Music & Audio |
| Default language | English (United States) |
| Contact email | Use the Play Console developer contact |
| Privacy policy | `https://tkleisas.github.io/XenAudioTools/privacy.html` |

## Store listing

Use the Fastlane metadata in `../fastlane/metadata/android/en-US/`.

- **Title:** UPIC Studio
- **Short description:** Draw UPIC arcs, sculpt GENDYN sound, compose with
  Xenakis sieves.
- **Full description:** see `full_description.txt`.
- **App icon:** 512×512 `images/icon.png`
- **Feature graphic:** 1024×500 `images/featureGraphic.png`
- **Phone screenshots:** two 1920×1080 landscape captures in
  `images/phoneScreenshots/` showing the single-line toolbar with full-screen
  canvas and the collapsible side control panel.

## App content

| Question | Answer |
| --- | --- |
| Ads | No |
| App access | All content is available without login or special access |
| Data collection | No data collected or shared |
| Data transmission | None; no INTERNET permission |
| Account deletion | Not applicable; no accounts |
| Target audience | 13+ music/creative users (not designed for children) |
| Content rating | Complete the questionnaire; no violence, sex, language, gambling, drugs, or user-generated content |
| News app | No |
| Government app | No |
| Health app | No |
| Financial features | No |
| COVID-19 contact tracing | No |

## Data safety detail

- Does the app collect or share any required user data types? **No.**
- Is all user data encrypted in transit? **Not applicable.**
- Can users request data deletion? **Not applicable; no data is retained.**
- Files are accessed only through the Android document picker when the user
  chooses Open or Save.
- No analytics, advertising, crash-reporting, or attribution SDKs are present.

## Permissions

The release manifest requests no runtime permissions and no internet access.
The app only writes/reads `.xenpage` files through Storage Access Framework
user consent.

## Testing tracks

1. Internal testing: upload the signed AAB, add the physical test device account,
   install from the Play link, and run the first-release checklist.
2. Closed testing: required by Play for some new personal developer accounts.
   Check the current account-specific requirement in Play Console.
3. Production: staged rollout, then monitor Android vitals and reviews.

## Release build

Use `android/scripts/build_release.sh` or the `Android Release` GitHub workflow.
The upload artifact is `android/app/build/outputs/bundle/release/app-release.aab`.
