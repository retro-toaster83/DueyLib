# DueyLib Technical Design Document

Nov 23, 2026 · @Jeremy Bond

## 1. Overview

DueyLib is a headless C++20 static library that gives music software one exact, deterministic core for time, pitch, theory, transformation, scheduling and MIDI file I/O. It never touches a UI, an audio device or an OS thread, so a piano roll, a notation editor, a tracker and a generative engine can all sit on the same foundation.

### Goals

- **Exactness:** musical time is a normalized rational; no floating-point drift anywhere in symbolic time.
- **Determinism:** the same inputs and the same seed give byte-identical outputs on every platform and compiler.
- **Independence:** zero runtime dependencies beyond the C++ standard library; GoogleTest only for tests.
- **Value semantics:** all public types are copyable values; no hidden global state, no singletons.
- **Composability:** every transformation is a pure function from `Sequence` to `Sequence`.
- **Testability:** every public function has unit tests, including edge cases in theory, timing and I/O.

### Non-goals

- Audio synthesis, plugin hosting or device I/O.
- UI, rendering or engraving (notation layout is out of scope; only symbolic data is provided).
- Real-time thread management. The transport cursor is pull-based so the host owns its own threads.
- Full MusicXML / MEI import and export in v1.

### Design principles

| Principle | Consequence in the code |
| --- | --- |
| Symbolic time is exact | `Rational` with `int64_t` numerator/denominator, always reduced, overflow-checked |
| Wall-clock time is derived | Seconds and samples only appear at the `TempoMap` and `TransportCursor` boundary |
| Spelling is preserved | `SpelledPitch` is the source of truth; `MidiPitch` is a projection |
| No hidden state | Randomness takes an explicit seed or `Rng&`; no `static` mutable data |
| Errors are values | Fallible parsing returns `std::expected`-style results; exceptions only for programmer errors |
| Stable surface | Public headers under `include/duey/`; implementation details in `src/` and `detail/` namespaces |

## 2. Architecture

DueyLib is five public modules in a strict layering: `core` at the bottom, then `theory`, `timing`, `transform` and `io` above it. Each module is a header directory under `include/duey/` and a matching source directory under `src/`; they compile into one static library.

&#91;embedded content: module layers · 4 modules on 1 core\]

Arrows point from a module to what it uses: `transform` uses `theory` for scale-aware transposition, `io` uses `timing` for tempo and meter maps, and every module rests on `core`. The host application sits outside and calls only the public headers.

### Dependency rules

- Dependencies point downward only; the CI build runs a header-include check that fails on any upward or sideways include other than `transform` to `theory` and `io` to `timing`.
- `core` includes nothing outside the standard library.
- Public headers expose no implementation types. Anything in `src/` or `duey::detail` is private and may change freely.
- The umbrella header `duey/duey.hpp` includes every public header; consumers who care about compile time include individual headers.

### Public API conventions

- **Namespaces:** `duey::` for core types, with `duey::theory`, `duey::transform`, `duey::timing`, `duey::io` for the rest.
- **Value semantics:** types are regular (default-constructible where it makes sense, copyable, comparable). No owning raw pointers appear in any signature.
- **Immutability:** free functions take `const&` and return new values. Classes with state (`TempoMap`, `TransportCursor`, `Rng`) expose explicit mutators.
- **Strong types:** `Letter`, `Accidental` and `Curve` are `enum class`; distinct units such as ticks and samples use thin wrapper types so they cannot be swapped by accident.
- **Designated initializers:** aggregates such as `NoteEvent` stay plain structs so the README's `.span = ..., .pitch = ...` style works.

### Error model

- Programmer errors (violated preconditions such as a zero denominator) throw `std::invalid_argument` or `std::domain_error`, and are guarded by `DUEY_ASSERT` in debug builds.
- Expected failures (malformed MIDI, unparseable `.scl`, a failed constraint search) return a result type carrying a typed error and a location. No exceptions cross the `io` boundary.
- The library is exception-safe and builds with exceptions on; a `DUEY_NO_EXCEPTIONS` mode is a later goal, not a v1 promise.

## 3. Core Primitives (`duey/core`)

Everything above `core` is built from seven value types. Each is trivially copyable or cheap to copy, has no virtual functions, and defines `operator==` and `operator<=>` where an ordering is meaningful.

| Type | Representation | Key invariants and operations |
| --- | --- | --- |
| `Rational` | `int64_t num`, `int64_t den` | Always reduced, `den > 0`, zero is `0/1`. Checked arithmetic via `__builtin_*_overflow` or a 128-bit intermediate; overflow throws `std::overflow_error`. `floor`, `ceil`, `mod`, `to_double` (display only) |
| `TimeRange` | `Rational start`, `Rational duration` | `duration >= 0`. `end()`, `contains`, `overlaps`, `intersect`, `shift`. Half-open `[start, end)` |
| `SpelledPitch` | `Letter`, `Accidental`, `int octave` | Accidental holds a signed semitone offset (double sharp = +2) to allow any spelling. Converts to `MidiPitch` under a given tuning |
| `MidiPitch` | `uint8_t noteNumber`, `float centOffset` | Cents are bounded to a single semitone; normalized on construction so `60 + 130c` becomes `61 + 30c` |
| `Interval` | `int diatonicSteps`, `int semitones` | Preserves spelling: a major third is `{2, 4}`, a diminished fourth is `{3, 4}`. Supports add, subtract, invert, negate |
| `NoteEvent` | span, pitch variant, `float velocity`, `uint8_t voice` | Velocity normalized to `[0, 1]`; pitch is `std::variant<SpelledPitch, MidiPitch>` |
| `Sequence` | sorted container of events | Ordered by `(start, voice, pitch)` for stable iteration; see below |

### Key decisions

- **`Rational` width.** `int64_t` covers any practical tuplet nesting. Operations reduce by GCD after each step and detect overflow rather than wrapping. A `Rational::from_ticks(ticks, ppq)` helper converts MIDI ticks exactly.
- **Pitch representation.** `SpelledPitch` is canonical. `F#4` and `Gb4` compare unequal as spelled pitches but equal as `MidiPitch`. Enharmonic comparison is an explicit function, never an implicit conversion.
- **Sequence storage.** A `std::vector<NoteEvent>` kept sorted, with an auxiliary start-time index. Range queries (`events_in(TimeRange)`) are `O(log n + k)`. Bulk insertion appends then sorts once; single insertion uses binary search. This beats a node-based tree for the read-heavy playback path.
- **Stable identity.** Each event gets an optional `EventId` (monotonic `uint64_t` per `Sequence`) so editors can refer to a note across transforms. Transforms preserve ids unless an event is created or split.
- **Non-destructive by default.** `Sequence` is cheap to copy via copy-on-write of its internal buffer, so `transform::*` functions take and return values without deep copies on the no-op path.

## 4. Timing and Scheduling (`duey/timing`)

Symbolic time stays rational until the last moment. `TempoMap` is the only place beats become seconds, and `TransportCursor` is the only place seconds become samples.

### TempoMap

- Stores a sorted list of `TempoPoint { Rational beat; double bpm; Curve curve; }` where `Curve` is `Step` or `Linear`.
- `beats_to_seconds` and `seconds_to_beats` are exact inverses within floating-point tolerance. For a linear ramp, seconds are the integral of `60 / bpm(b)` over beats, which has a closed-form logarithmic solution; Step segments reduce to a multiply.
- Cumulative seconds at each tempo point are precomputed on insertion, so a lookup is a binary search plus one segment evaluation: `O(log n)`.
- Ticks: `to_ticks(Rational, ppq)` and `from_ticks` round only at the tick boundary and report the residual so callers can detect non-representable tuplets.

### Meter

- `TimeSignature` holds a list of beat groups, so `3+2+3/8` and `6/8` differ in grouping but share a total length of `Rational{6, 8}`.
- `MeterMap` maps bars to beats, handling mid-piece meter changes and pickup bars. Provides `bar_beat_at(Rational)` and the inverse.

### TransportCursor

A pull-based scheduler. The host calls `step(sequence, tempoMap, bufferSizeSamples)` once per audio block and gets back the events that fall inside that window, each tagged with a sample offset.

1. **Window math.** Convert the cursor's current sample position to seconds, then to a beat range `[b0, b1)` via `TempoMap`. Query `Sequence::events_in` for note starts in the range.
2. **Sample offset.** `offset = round((seconds(event) - seconds(window start)) * sampleRate)`. Rounding is done once per event from the exact beat, so errors never accumulate across buffers.
3. **Note-off pairing.** The cursor keeps a small set of currently sounding notes. A note that starts in this window and ends in a later one is held in the set; its `NoteOff` is emitted in whichever window contains its end.
4. **Seeks and loops.** On a seek or loop wrap the cursor first emits `NoteOff` for every held note at the discontinuity, then resumes. This guarantees no stuck notes.
5. **State.** The only mutable state is the sample position, the loop range and the held-note set. `TransportCursor` is copyable, so a host can snapshot it for undo or for a look-ahead pass.

### Edge cases that need tests

- Notes that start exactly on a buffer boundary appear in exactly one window.
- Zero-length notes emit `NoteOn` and `NoteOff` at the same offset, in that order.
- A tempo change inside a buffer shifts the offsets of only the later events.
- A loop end that falls inside a sounding note's duration.

## 5. Theory and Analysis (`duey/theory`)

Theory code is pure functions over small value types. Nothing here depends on `Sequence`, so it can be used on its own.

### PitchClassSet

- A `uint16_t` with 12 used bits. Transposition is a 12-bit rotate; inversion is a bit reversal; union, intersection and complement are single instructions.
- Derived data is computed on demand: interval-class vector, prime form (via minimal rotation of the set and its inversion), `popcount` cardinality.
- A 4096-entry lookup table, built at compile time with `constexpr`, maps every set to its prime form.

### Scales and modes

- `Scale` pairs a `PitchClassSet` with a root and a spelling policy. Mode rotation is a re-rooting of the same set.
- Degree mapping converts a `(degree, octave)` pair to a spelled pitch and back, with chromatic alteration (`b3`, `#4`) handled explicitly.
- A registry of named scales (major, the minor family, modes, pentatonics, symmetric scales) lives in a `constexpr` table. Custom scales are built from interval lists.

### Tuning

- `Tuning` is an interface with `frequency(MidiPitch)` and `from_frequency`. Implementations: `EqualTemperament`, `JustIntonation` (ratio table relative to a root) and `TableTuning` loaded from Scala `.scl` / `.kbm` text.
- Tuning only affects the conversion to Hz and cents. It never changes symbolic pitch.

### Chords

- **Builder.** A `ChordSpec` holds root, quality, extensions, alterations and optional slash bass. It resolves to a `PitchClassSet` plus a spelled pitch list.
- **Recognizer.** `analyze(PitchClassSet, optional bass)` tests candidate roots against a template library, scores each match, and returns a ranked list of `ChordSymbol`. Score combines template coverage, missing and extra tones, root-position bonus and bass agreement. Ties break deterministically on a fixed template order.

### Validation and linting

- `InstrumentProfile` holds written and comfortable ranges, transposition and polyphony limits.
- Rule checkers return `std::vector<Diagnostic>` rather than throwing. A diagnostic carries a severity, a `TimeRange`, a rule id and a message. Counterpoint rules (parallel fifths and octaves, voice crossing, large leaps) are individual functions registered in a `RuleSet`, so apps can enable the ones they want.
- An overlap sanitizer trims or merges same-pitch overlaps on one voice according to a chosen policy.

## 6. Transformations and Generative Tools (`duey/transform`)

Every transformation has the shape `Sequence f(const Sequence&, Params)` and never mutates its input. That makes them composable, testable in isolation and safe to run on a background thread.

### Transformation catalog

| Group | Functions | Notes |
| --- | --- | --- |
| Pitch | `transpose` (chromatic and diatonic), `invert`, `retrograde_pitch` | Diatonic transposition needs a `Scale`; chromatic uses `Interval` and keeps spelling |
| Time | `retrograde`, `augment`, `diminish`, `shift`, `stretch` | Scale factors are `Rational`, so a 3:2 stretch is exact |
| Quantize | `quantize(grid, strength, swing)` | Grid is a `Rational`; strength blends between original and snapped start |
| Humanize | `humanize(seed, timingJitter, velocityJitter)` | Uses the seeded PRNG; same seed, same output |
| Voicing | `voice_lead`, `drop2`, `drop3`, `spread`, `close` | Voice leading minimizes total semitone motion between successive chords, with a deterministic tie-break |
| Arrangement | `arpeggiate(pattern)`, `strum(offsetPerNote)` | Offsets are `Rational` |
| Algorithmic | `euclid(k, n, rotation)`, `markov_walk`, `constraint_generate` | Detailed below |

### Determinism and randomness

- A single `Rng` type wraps a fixed algorithm (PCG32 or xoshiro128++) with its state exposed. The standard library's `std::mt19937` is avoided because `std::uniform_*_distribution` output differs between implementations.
- Distributions are hand-written on top of raw 32-bit output, so results are identical on GCC, Clang and MSVC.
- Functions that need randomness take either a `uint64_t seed` or an `Rng&`. There is no global generator.

### Algorithmic utilities

- **Euclidean rhythms.** Bjorklund's algorithm over a bit vector, returning a pattern that can be mapped onto a `Sequence` with a chosen step size and rotation.
- **Markov helpers.** A `MarkovModel<State>` with training from sequences, normalized transition weights and a `walk(Rng&, length)`. Order-n states use a hashed tuple key with a deterministic iteration order.
- **Constraint generation.** A bounded depth-first search over candidate notes with pluggable constraints (scale membership, range, leap limits, rule checkers from `theory`). Candidate ordering is seeded so the search is reproducible; a node limit prevents runaway search and returns a clear failure result.

## 7. MIDI and File I/O (`duey/io`)

I/O is a thin layer at the edge. It converts between bytes and `Sequence` / `TempoMap` / `MeterMap` and holds no other logic.

### Byte stream codec

- `MidiMessage` is a small value type: status, channel, up to two data bytes, or a `SysEx` byte vector.
- `encode(MidiMessage, ByteSink&)` and `decode(ByteSpan) -> DecodeResult` handle running status, 14-bit values and note-on with velocity 0 as note-off.
- Decoding is bounds-checked and never reads past the span. Malformed input returns an error value with a byte offset.

### Standard MIDI File

- **Reader.** Parses `MThd` and `MTrk` chunks, variable-length quantities and meta events (tempo, time signature, key signature, track name, end of track). Type 0 and Type 1 are supported; Type 2 is rejected with a clear error.
- **Tick conversion.** Delta ticks accumulate as integers, then convert to `Rational` beats by `ticks / ppq`. No rounding happens on import.
- **Writer.** Quantizes each event time to ticks once, sorts events within each track deterministically (note-offs before note-ons at the same tick), emits running status only when requested, and writes tempo and meter maps to the conductor track.
- **Round trip guarantee.** `read(write(seq))` equals `seq` whenever all event times are representable at the chosen PPQ. When they are not, the writer reports which events were rounded and by how much.

### Design constraints

- Input and output use `std::span<const std::byte>` and a sink interface, so file, memory and network sources all work. The library itself performs no file system calls beyond optional convenience wrappers behind a compile-time switch.
- Fuzz tests feed random and truncated byte streams to the decoder and the SMF reader; the only acceptable outcomes are a valid result or a clean error.

## 8. Implementation Strategy

Build bottom-up along the dependency graph: each phase delivers a module that is fully tested before anything depends on it, and each ends at a gate with measurable criteria. Phases are ordered by dependency, not by calendar; durations are left for the team to set.

&#91;embedded content: build roadmap · 6 phases, 5 gates\]

Phases 4 and 5 can run in parallel with phase 3 once phase 2 is done, because `theory` and `transform` depend only on `core`. Phase 3 is the earliest point at which the library is useful end to end.

### Build system

- One CMake target `duey` (a static library) with the alias `Duey::DueyLib`, `cxx_std_20` as a public compile feature and `include/` as a public include directory.
- `DUEY_BUILD_TESTS` (default `ON` only for a top-level build) adds the test executables. GoogleTest comes from `find_package` first, with a `FetchContent` fallback pinned to a release tag.
- Optional switches: `DUEY_WARNINGS_AS_ERRORS`, `DUEY_SANITIZE` (address and undefined behaviour), `DUEY_BUILD_FUZZERS`, `DUEY_ENABLE_FILE_IO`.
- Install and export rules produce a `DueyLibConfig.cmake` so consumers can use `find_package(DueyLib)` as well as `add_subdirectory`.
- C++20 only: `std::expected` is C++23, so the library ships a small `duey::Result<T, E>` with the same shape and can alias it later.

### Testing strategy

- **Layout:** `tests/` mirrors `include/duey/`, one test file per header, so every public function has an obvious home.
- **Property tests without dependencies:** loops over a seeded `Rng` check algebraic laws, for example `a + b - b == a` for `Rational`, `transpose(up) then transpose(down)` is the identity, and `read(write(x)) == x` for SMF.
- **Golden files:** fixed `.mid` inputs and expected event lists, plus hashes of generative output, so a determinism regression on any compiler shows up as a diff.
- **Oracles:** slow, obviously correct brute-force versions (range query by linear scan, chord analysis by exhaustive search) validate the fast implementations.
- **Fuzzing:** libFuzzer targets for the MIDI decoder and SMF reader, built only when `DUEY_BUILD_FUZZERS` is on.

### Continuous integration

- Matrix: GCC, Clang and MSVC on Linux, macOS and Windows, in Debug and Release.
- Every push runs the warning flags from the README (`-Wall -Wextra -Wpedantic`, `/W4`), the include-direction check, and a sanitizer job.
- A nightly job runs the fuzzers for a fixed time budget and a larger randomized property run.

### Performance targets

The library has no hard real-time promise, but `TransportCursor::step` is meant to be called from an audio thread, so it must not allocate or lock in steady state. Its output buffer is caller-provided or reserved at construction. Benchmarks (Google Benchmark, optional) track `step`, `events_in` and `TempoMap` lookups so regressions are caught in review.

### Risks and mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| `Rational` overflow with deep tuplets or long tempo maps | Wrong timing | 128-bit intermediates, checked operations, property tests against a big-integer oracle |
| Floating-point differences between compilers in `TempoMap` | Non-reproducible sample offsets | Exact math until the final conversion, no fast-math, tolerance-based golden tests |
| Enharmonic spelling mistakes in transposition | Wrong notation | Table-driven tests over all interval and spelling combinations, round-trip properties |
| Stuck notes on seek or loop | Audible defects in hosts | Held-note invariant tests and randomized seek sequences |
| Real-world SMF files that break the reader | Import failures | Corpus of real files, fuzzing, strict-but-clear error reporting |
| Scope creep into notation or MusicXML | Schedule slip | Non-goals stay in force; add extension points instead of features |
| API churn before 1.0 | Breakage for early adopters | Small public headers; unstable additions go in `duey::experimental` |

### Open questions

- Is `int64_t` enough for `Rational`, or should an optional wider type be selectable at build time?
- Should `NoteEvent` hold a `std::variant` of pitch types, or two optional fields with a conversion helper?
- How should microtonal pitch be written to SMF: pitch bend per channel, MPE, or a documented limitation?
- When can the library move to C++23 and use `std::expected` and `std::mdspan` directly?
- Which license and versioning policy (semantic versioning with an ABI promise, or source compatibility only) will the project adopt?
