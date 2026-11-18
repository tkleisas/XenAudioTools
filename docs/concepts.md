# Concepts and implementation notes

This document maps each implemented idea to the source file that realises it and
records the choices made where the published description leaves room for
interpretation. It refers to chapter numbers of *Formalized Music* (revised
edition) but does not reproduce the text.

---

## 0. Common coordinate model

Everything in the suite is a set of points on one or more lines with a unit `u`,
as Xenakis proposes for sieves:

- **Pitch** is an integer index. By default index `0` is MIDI note 0
  (8.1758 Hz) and `u` is one semitone, so index `n` is `8.1758 * 2^(n/12)` Hz.
- **Time** is in seconds.
- A **frequency table** maps the normalised vertical position `p ∈ [0,1]` of a
  drawn arc to a pitch index, and therefore to Hz. The table can be continuous,
  discrete (snap to equal divisions), or snapped to a sieve.
- An **arc** is a piecewise-linear pitch-versus-time curve, the UPIC extension of
  a note.

Source: `lib/include/xen/tuning.h`, `lib/include/xen/upic.h`.

---

## 1. Sieves (Chapters XI–XII)

### Definition

A sieve is a union of intersections of congruence classes. A class is a pair
`(M, I)`: all integers `x` with `x ≡ I (mod M)`. In code a `Sieve` is
`vector<vector<SieveClass>>`; the outer vector is the union and each inner vector
is one intersection.

```
L = { (M11, I11) ∩ (M12, I12) ∩ ... } ∪ { (M21, I21) ∩ ... } ∪ ...
```

A point belongs to the sieve if it belongs to at least one intersection.

### Intersection of two classes

For `(M1, I1)` and `(M2, I2)`, let `D = gcd(M1, M2)`.

- If `(I1 − I2) mod D ≠ 0`, the intersection is empty.
- Otherwise the result is `(M3, I3)` with `M3 = lcm(M1, M2)` and `I3` obtained by
  solving `I1 + M1·k ≡ I2 (mod M2)`. The implementation divides by `D`, uses the
  modular inverse of `M1/D` modulo `M2/D` (Bachet–de Méziriac's theorem), and
  reduces the offset modulo `M3`.

Source: `Sieve::intersectClass`, `Sieve::reduced`.

### Period and complement

After reduction each intersection becomes one class. The sieve period is the
least common multiple of the resulting moduli. The complement of a sieve `A`
inside a universe `U` is computed exactly over `lcm(period(A), period(U))`
residues (capped at 65 536), then reduced and subsumed. Classes that are subsets
of another class are removed.

Source: `Sieve::period`, `sieveComplement`.

### Inverse: points to sieve

`Sieve::fromPoints` implements the book's inverse construction in a bounded,
deterministic form. For each still-uncovered point it searches moduli `q = 1, 2,
…` and keeps the first `q` whose class hits at least two of the given points and
no point outside the set (within the range). Points that cannot join a larger
class get a singleton class with the range modulus. The result is reduced and
subsumed. This is not guaranteed to recover the unique minimal formula, but it is
exact on the input points and demonstrates the inverse operation.

### Metabolae

`transpose(n)` shifts every offset; `invert()` reflects offsets. Union and
intersection are provided as free functions. Adding a constant to the offsets is
the book's index-change metabola; changing the unit is done by changing the
frequency table's `divisionsPerOctave`.

### Use in the app

The Sieve panel parses `modulus: offsets`, e.g. `12: 0 2 4 5 7 9 11`, and offers
three generators:

- **Field** — one sustained arc per sieve point in the index range.
- **Arpeggio** — successive notes cycling through the sieve points.
- **Rhythm** — applies the sieve to time steps; a step sounds when the sieve
  matches the step index.

The frequency table is marked `useSieve`, so the pitch snapping in the canvas and
the arc renderer both use the same sieve.

Source: `lib/include/xen/sieve.h`, `shared/include/xen/generators.h`,
`ui/src/XenEditorPanel.cpp`, `ui/src/ScoreCanvas.cpp`.

---

## 2. Dynamic stochastic synthesis (Chapter XIII)

### Model

A voice is a closed polygon of `n` breakpoints over one period `T = 1/f`:

- `d[i]` — duration in samples of segment `i`, with `Σ d[i] ≈ T`;
- `a[i]` — amplitude at the end of segment `i`;
- output between breakpoints is linear interpolation.

At the end of every period the breakpoints are perturbed:

```
d'[i] = clamp(d[i] + δ_d · mean · step_d, d[i] − band_d, d[i] + band_d)
d'[i] = clamp(d'[i], d_min, d_max)

a'[i] = clamp(a[i] + δ_a · step_a, a[i] − band_a, a[i] + band_a)
a'[i] = clamp(a'[i], a_min, a_max)
```

where `δ` is drawn from the chosen distribution (uniform, Gaussian, Cauchy,
logistic, exponential), `band_d` is a fraction of the mean segment length, and
the local barriers are followed by global barriers as in the book. `mirror`
enforces the symmetry `a[n−1−i] = −a[i]`.

The initial polygon is a sine-like shape plus a small random perturbation. When
the frequency changes, durations are rescaled by the ratio of the new and old
mean segment lengths, which makes glissandi and arc pitch changes continuous.

### Implementation details

- Breakpoint arrays are preallocated to 64 segments in `prepare()`. Changing the
  segment count never allocates on the audio thread.
- A voice is only gated on entering an arc; it is not re-triggered every period.
  Leaving an arc fades the gain to zero before the voice is gated off, avoiding
  clicks.
- The distribution scale and step sizes are exposed in the GENDYN panel.

Source: `lib/include/xen/gendyn.h`, the GENDYN panel in
`ui/src/XenEditorPanel.cpp`, and the live/arc voice handling in
`shared/src/engine.cpp`.

---

## 3. Markovian stochastic music (Chapters II–III)

A first-order chain is a row-stochastic matrix `P`, with `P[i][j]` the probability
of moving from state `i` to state `j`. The implementation provides:

- `setMatrix`, `fromCounts` (adds a smoothing constant and normalises),
- `randomMatrix(states, selfBias, seed)`,
- `periodicScale(states)` (a deterministic cycle, useful for testing),
- `step`, `jump`, `distribution`, and `stationary` (power iteration).

For musical use, states map linearly onto a pitch-index range. The Markov panel
generates a sequence of short arcs; a state can also modulate note duration.
Because the random generator is seeded, a given seed and matrix reproduce the
same sequence.

Source: `lib/include/xen/markov.h`.

---

## 4. Stochastic clouds (Chapters I, V, IX)

A cloud is a set of grains. Grain onsets follow a Poisson process with rate
`λ` (grains per second), implemented by drawing exponential inter-onset times.
Optional density spread multiplies each interval by `exp(U(−s, s))`.

For each grain:

- **pitch** = `centre · 2^(Δ/12)`, where `Δ` is drawn from the chosen
  distribution (logistic by default) scaled by the semitone spread;
- **duration** = `mean · exp(...)` or `mean · (1 + U(−s, s))`, clamped;
- **amplitude** = `mean · exp(...)`, clamped;
- **pan** = optional uniform spread.

The generator converts each grain into a short arc so that clouds are rendered by
the same UPIC page player as everything else.

Source: `lib/include/xen/clouds.h`, `generateCloudArcs` in
`shared/include/xen/generators.h`.

---

## 5. Musical strategy (Chapter IV)

`StrategyGame` is a two-player game with payoff matrices `A` (row player) and
`B` (column player). It supports:

- **best response** to a pure opponent strategy;
- **fictitious play** — each player best-responds to the empirical frequency of
  the opponent's past choices; the empirical frequencies are the mixed strategy;
- **replicator dynamics** — a strategy grows in proportion to how its fitness
  exceeds the population average:

```
x_i ← x_i · (1 + α (f_i − f̄))
```

followed by normalisation. `dt`/`α` is clamped to `[0,1]` for stability.

The Strategy panel builds a random payoff matrix of the chosen size, advances the
game per musical segment, and maps the row strategy to pitch and the column
strategy to duration, with the payoff nudging the pitch. Fictitious play gives a
deterministic exploration of the strategy space; the replicator gives a smooth
evolution of the mixture.

Source: `lib/include/xen/strategy.h`, `generateStrategy` in
`shared/include/xen/generators.h`.

---

## 6. UPIC expansion (Appendix III)

### Data model

- `ArcPoint { time, pitch }` — a breakpoint; pitch is normalised `[0,1]`.
- `Arc` — sorted points, waveform type, group, channel, envelope index,
  frequency-table index, weight, mute flag.
- `Envelope` — normalised-time, level breakpoints; `Envelope::ad` builds an
  attack/release shape.
- `FrequencyTable` — a `Tuning` plus optional sieve and index range.
- `ArcGroup` — name, mute, solo, transpose.
- `Page` — arcs, envelopes, tables, groups, duration, loop range.

The book's arc concept (a continuous frequency-versus-time curve), voice
attributes (waveform, envelope, frequency and amplitude tables), groups, and
performance model are all represented. The standalone program and the VST3 share
the same page and editor.

### Rendering

The engine scans the page at the playhead. Every arc whose time range contains
the playhead is a candidate; up to 64 are assigned to a persistent voice pool,
and voices whose arcs have left fade out and are reused. For an active arc:

1. if `playhead ∈ [start, end]`, find `u = (playhead − start) / duration`;
2. `pitch = arc.pitchAt(playhead)`;
3. `hz = table.hzForNorm(pitch)`, optionally snapped to the table's sieve, then
   transposed by the group;
4. `amp = arc.weight · page.envelope(u) · pageGain`;
5. drive the arc's persistent oscillator or `GendynVoice`, with smoothed gain.

The page transport supports run/stop, looping between `loopStart` and `loopEnd`,
reverse scanning, and a speed multiplier. Live MIDI notes are separate 16-voice
GENDYN voices and are mixed with the page.

### Differences from the original UPIC

- 64 simultaneous voices, matching the original 64-oscillator synthesizer. The
  editor can store many more arcs; they are assigned to voices as they become
  active, and at most 64 sound at the same instant.
- Waveforms are sine/saw/square/triangle plus GENDYN, rather than a bank of 64
  sampled waveforms.
- Envelopes are breakpoint curves; the original also had amplitude tables and
  modulating arcs. The data model leaves room for these (indexed tables and
  groups) but the first version exposes breakpoint envelopes.
- Drawing is mouse/trackpad based; no digitising tablet.

Source: `lib/include/xen/upic.h`, `shared/include/xen/engine.h`,
`shared/src/engine.cpp`, `ui/src/ScoreCanvas.cpp`.

---

## 7. Architecture and threading

```
UI (message thread)                 audio thread
────────────────────                ────────────
PageModel::pageRef()  (edit)
PageModel::publish()  ──copy──▶  shared_ptr<const Page>  ──atomic──▶ Engine::render
Engine::setArcGendynParams ──mutex──▶ params copied at block boundary
Engine::noteOn/noteOff  ──▶        live voice pool
```

- `PageModel` owns the editable page and a mutex. The editor reads and edits it on
  the message thread; `snapshot()`/`serialize()` lock for cross-thread state
  access.
- `publish()` creates an immutable `shared_ptr<const Page>` and stores it with
  `std::atomic_store`. The audio thread loads it once per block.
- All voices are preallocated. No allocation, locking, or I/O happens per sample.
  The only lock on the audio thread is a short parameter copy at block start.
- In the VST3, `setStateInformation` is marshalled to the message thread so it
  cannot race the editor's page access.

---

## 8. Page file format (`.xenpage`)

The format is line-oriented, human-readable text:

```
XENPAGE 1
name "..."
duration 16
loop 0 16
groups 4
group 0 0 0 "Group A"
...
envelopes 1
env 4 0 0 0.01 1 0.95 1 1 0
tables 1
table 24 96 8.1758 12 0 0 1 <sieve groups and classes>
arcs 12
arc 4 0 0 0 0 0.45 0 2 0 0.5 2 0.5
```

Numbers are parsed with `std::istringstream`; names use `std::quoted`. Unknown
tokens are skipped, which leaves room for future fields. See
`shared/src/serialize.cpp`.

---

## 9. Testing strategy

The tests are deliberately model-level and render-level:

- algebraic properties of sieves (membership, CRT, period, complement, inverse);
- tuning round-trips;
- Markov stationarity and bounded transitions;
- GENDYN output finiteness, bounds, gating, and frequency changes;
- cloud onset ordering and parameter bounds;
- strategy convergence on matching pennies and normalisation;
- arc interpolation, envelopes, and page duration;
- serialization round-trips including sieves and quoted names;
- offline engine renders of every generator and every waveform.

Run with `ctest --test-dir build --output-on-failure`.
