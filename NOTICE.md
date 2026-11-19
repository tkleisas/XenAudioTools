# Notice and attribution

XenAudioTools / Xenakis UPIC / Xenakis UPIC Studio
Copyright © 2026 bokontep.

The **core library and engine** (`lib/` and `shared/`) are dual-licensed under
**MIT OR GPL-3.0-or-later** (see `LICENSES/MIT.txt`). The VST3 instrument,
standalone app, Android app, editor UI, tools, tests, presets, and documentation
are **GPL-3.0-or-later** (see `LICENSE`).

## Conceptual sources

The algorithms in this project are implementations of concepts published by
Iannis Xenakis in *Formalized Music: Thought and Mathematics in Composition*,
especially:

- free stochastic music and clouds of sound-points (Chapters I, V, IX);
- Markovian stochastic music (Chapters II–III);
- musical strategy and payoff games (Chapter IV);
- sieves (Chapters XI–XII);
- dynamic stochastic synthesis (Chapter XIII);
- the UPIC graphical score system (Appendix III).

The implementations are original code written from the mathematical descriptions.
No source code, text, score, image, or audio recording from the book is included
in this repository's software or presets. The local PDF copy of the book is a
personal reference and is not redistributed by this project.

## Third-party software

- **JUCE 7** — © Raw Material Software Limited, GPLv3 (or commercial). The JUCE
  source is not vendored here; the build expects an existing JUCE 7 checkout,
  by default `../ans-1964/third_party/JUCE`.
- **Steinberg VST3 SDK** — © Steinberg Media Technologies GmbH, GPLv3. It is
  distributed inside JUCE and is used here under the GPLv3 option.
- **VST** is a trademark of Steinberg Media Technologies GmbH. **VST3** is used
  descriptively.
- **AndroidX / Jetpack Compose** — © The Android Open Source Project, Apache
  License 2.0. Used by the Android app.
- **Android NDK / AAudio** — © The Android Open Source Project, Apache License
  2.0. Native Android audio output uses AAudio.
- **Gradle wrapper** — © Gradle Inc., Apache License 2.0. The wrapper JAR is
  included to make the Android build reproducible.

The MPC Plugin SDK is not used by this project. The ANS 1964 and Polivoks 1982
projects are separate works with their own notices.

## Independence

This is an independent project. It is not affiliated with, endorsed by, or
sponsored by the Xenakis estate, CEMAMu, Steinberg, or any hardware or software
manufacturer. "UPIC" refers to the historical CEMAMu system and is used here to
describe the graphical-arc composition model that this project expands.

## No warranty

The software is provided "as is", without warranty of any kind, express or
implied, including but not limited to the warranties of merchantability, fitness
for a particular purpose and noninfringement.
