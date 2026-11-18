# Patches

## `juce-7.0.12-macos15-sdk.patch`

JUCE 7.0.12 calls `CGWindowListCreateImage` for window snapshots. The macOS 15+
SDK marks that API unavailable, so the JUCE build fails on current Xcode/SDK
images. XenAudioTools does not use window snapshots, so the patch returns an
empty image on the new SDK while keeping the original code for older SDKs.

Apply it to a JUCE 7.0.12 checkout before configuring on macOS:

```bash
git -C juce-src apply --ignore-whitespace patches/juce-7.0.12-macos15-sdk.patch
```

The macOS CI jobs apply it automatically.
