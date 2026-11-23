
# DueyLib

**A lightweight, app-agnostic C++ static library and toolbox for music composition.**

DueyLib provides the foundational data structures, music theory engine, rhythmic math, and compositional transformations needed to power music software—without locking you into a specific UI paradigm, audio engine, or application architecture.

Whether you are building a DAW piano roll, a traditional notation editor, a live-coding environment, a tracker, or an algorithmic music generator, DueyLib serves as a deterministic, headless core.

---

## Design Philosophy

* **App-Agnostic Architecture:** Strictly separates symbolic music logic from visual rendering, audio synthesis, and OS threading.
* **Exact & Deterministic:** Uses rational arithmetic for musical time (zero floating-point drift) and seeded PRNGs for generative/humanization tools.
* **Minimal Dependencies:** Zero external runtime dependencies—built purely on the C++ Standard Library.
* **Static Library First:** Designed to compile cleanly into a static library (`libduey.a` / `duey.lib`) with clean value semantics and no hidden global state.
* **Fully Unit Tested:** Backed by a comprehensive GoogleTest (GTest) suite covering edge cases in theory, timing, and transformations.

---

## Features

### Core Primitives & Timing
* **Exact Rational Time:** Represent beats, durations, and nested tuplets (`1/4`, `1/12`, `1/20`) as exact fractions alongside PPQ tick conversions.
* **Dual Pitch System:** Seamless conversion between **Spelled Pitch** (`F#4` vs. `Gb4`) and **Acoustic/MIDI Pitch** (note numbers + microtonal cent offsets).
* **Microtonality & Tuning:** Built-in support for 12-TET, Just Intonation, and custom tuning tables (e.g., Scala `.scl` support).
* **Tempo & Meter Maps:** Piecewise tempo automation (step and linear curves) and support for simple, compound, and asymmetric/additive meters (`5/8`, `3+2+3/8`).

### Music Theory & Analysis
* **Scales & Modes:** Fast bitmask-based `PitchClassSet` operations, mode rotation, and scale degree mapping.
* **Chord Builder & Recognizer:** Construct chords with extensions/slash-basses and analyze arbitrary pitch collections into ranked chord symbols.
* **Interval Math:** Diatonic and chromatic interval arithmetic that preserves enharmonic spelling.
* **Validation & Linting:** Instrument range profiles, overlap sanitizers, and counterpoint/voice-leading rule checkers.

### Composition Toolbox
* **Classical & Modern Transformations:** Diatonic/chromatic transposition, inversion, retrograde, augmentation/diminution, and deterministic humanization.
* **Voicing & Arrangement:** Automatic voice leading, Drop-2/Drop-3 voicings, arpeggiation, and strumming offsets.
* **Algorithmic Utilities:** Euclidean rhythm generator (Bjorklund’s algorithm), Markov chain sequence helpers, and constraint-based generators.

### Scheduling & Interop
* **Pull-Based Transport Cursor:** Headless, sample-accurate playback scheduler that yields `NoteOn`/`NoteOff`/`CC` events for any buffer window while automatically preventing stuck notes on seeks or loops.
* **Standard MIDI File (SMF):** Zero-dependency Type 0 / Type 1 `.mid` reader and writer, plus raw MIDI byte stream encoding/decoding.

---

## Core Data Types Overview

| Type | Purpose |
| :--- | :--- |
| `Rational` | Exact fractional musical time and duration |
| `TimeRange` | Start/duration span used for notes, loops, and range queries |
| `SpelledPitch` | Symbolic pitch (`Letter`, `Accidental`, `Octave`) |
| `MidiPitch` | Acoustic pitch (`noteNumber` + microtonal `centOffset`) |
| `PitchClassSet` | 12-bit mask for fast scale and chord set operations |
| `NoteEvent` | Value-type event holding span, pitch, normalized velocity, and voice/channel |
| `Sequence` | Time-indexed container of musical events (clips, parts, or patterns) |
| `TempoMap` | Converts between symbolic `Rational` time and wall-clock/sample offsets |
| `TransportCursor` | Stateless-friendly playback state machine for audio/MIDI engines |

---

## Requirements

* **C++ Compiler:** C++20 or newer (GCC, Clang, or MSVC)
* **CMake:** 3.28 or newer
* **GoogleTest (GTest):** Required *only* when building unit tests (`DUEY_BUILD_TESTS=ON`)

---

## Building & Running Tests

DueyLib uses CMake out-of-source builds. By default, unit tests can be toggled via `DUEY_BUILD_TESTS`.

```bash
# 1. Configure the project with unit tests enabled
cmake -B build -DDUEY_BUILD_TESTS=ON

# 2. Build the static library and test executable
cmake --build build

# 3. Run the GoogleTest suite via CTest
ctest --test-dir build --output-on-failure
```

To build **only** the static library without pulling or linking GoogleTest:

```bash
cmake -B build -DDUEY_BUILD_TESTS=OFF
cmake --build build
```

---

## Integrating into Your Project

### Using CMake `add_subdirectory` or `FetchContent`

```cmake
# Disable tests when consuming DueyLib as a dependency
set(DUEY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/DueyLib)

target_link_libraries(YourMusicApp PRIVATE Duey::DueyLib)
```

---

## Quick Example

```cpp
#include <duey/duey.hpp>
#include <iostream>

int main() {
    using namespace duey;

    // 1. Define exact musical time (quarter note = 1/4, eighth-note triplet = 1/12)
    const Rational quarter{1, 4};
    const Rational triplet_eighth{1, 12};

    // 2. Build a Sequence using exact spelled pitches
    Sequence melody;
    melody.add(NoteEvent{
        .span = {Rational{0, 1}, quarter},
        .pitch = SpelledPitch{Letter::C, Accidental::Natural, 4},
        .velocity = 0.85f
    });
    melody.add(NoteEvent{
        .span = {quarter, triplet_eighth},
        .pitch = SpelledPitch{Letter::E, Accidental::Flat, 4},
        .velocity = 0.75f
    });

    // 3. Apply a non-destructive transformation (transpose up a Major 3rd)
    const Interval major_third{2, 4}; // 2 diatonic steps, 4 semitones
    Sequence transposed = transform::transpose(melody, major_third);

    // 4. Convert to sample-accurate playback events using a TempoMap
    TempoMap tempo_map(/*initialBpm=*/120.0);
    TransportCursor cursor(/*sampleRate=*/44100);

    auto events = cursor.step(transposed, tempo_map, /*bufferSizeSamples=*/512);
    for (const auto& ev : events) {
        std::cout << "Sample " << ev.sampleOffset
                  << " | MIDI Note: " << static_cast<int>(ev.pitch.noteNumber) << "\n";
    }

    return 0;
}
```

---

## Project Structure

```text
DueyLib/
├── CMakeLists.txt          # Top-level CMake configuration
├── include/
│   └── duey/               # Public headers
│       ├── duey.hpp        # Umbrella header
│       ├── core/           # Rational, Pitch, Interval, Events
│       ├── theory/         # Scales, Chords, Harmony, Validation
│       ├── transform/      # Transposition, Quantize, Generative tools
│       ├── timing/         # TempoMap, TimeSignature, TransportCursor
│       └── io/             # Standard MIDI File & byte stream codecs
├── src/                    # Private implementation files
└── tests/                  # GoogleTest unit tests (mirrors include/ structure)
    ├── core/
    ├── theory/
    ├── transform/
    ├── timing/
    └── io/
```

---

## Contributing

1. All new features and bug fixes must include corresponding **GoogleTest** unit tests.
2. Keep core logic **pure and deterministic**—avoid global mutable state, OS-specific APIs, and non-seeded randomness.
3. Ensure zero compiler warnings under `-Wall -Wextra -Wpedantic` (or `/W4` on MSVC).
