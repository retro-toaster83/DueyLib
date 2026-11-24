
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace duey {

enum class Letter : std::uint8_t {
	C = 0,
	D = 1,
	E = 2,
	F = 3,
	G = 4,
	A = 5,
	B = 6,
};

/// Signed semitone offset from natural. Any int8_t offset is valid (e.g. +3 = triple sharp).
enum class Accidental : std::int8_t {
	DoubleFlat = -2,
	Flat = -1,
	Natural = 0,
	Sharp = 1,
	DoubleSharp = 2,
};

[[nodiscard]] constexpr bool isValidLetter(Letter letter) noexcept {
	const auto v = static_cast<std::uint8_t>(letter);
	return v <= static_cast<std::uint8_t>(Letter::B);
}

[[nodiscard]] constexpr int diatonicSemitone(Letter letter) {
	switch (letter) {
	case Letter::C: return 0;
	case Letter::D: return 2;
	case Letter::E: return 4;
	case Letter::F: return 5;
	case Letter::G: return 7;
	case Letter::A: return 9;
	case Letter::B: return 11;
	}
	throw std::invalid_argument("Invalid Letter enumerator");
}

[[nodiscard]] constexpr char toChar(Letter letter) {
	switch (letter) {
	case Letter::C: return 'C';
	case Letter::D: return 'D';
	case Letter::E: return 'E';
	case Letter::F: return 'F';
	case Letter::G: return 'G';
	case Letter::A: return 'A';
	case Letter::B: return 'B';
	}
	throw std::invalid_argument("Invalid Letter enumerator");
}

[[nodiscard]] constexpr std::optional<Letter> letterFromChar(char ch) noexcept {
	switch (ch) {
	case 'C': case 'c': return Letter::C;
	case 'D': case 'd': return Letter::D;
	case 'E': case 'e': return Letter::E;
	case 'F': case 'f': return Letter::F;
	case 'G': case 'g': return Letter::G;
	case 'A': case 'a': return Letter::A;
	case 'B': case 'b': return Letter::B;
	default: return std::nullopt;
	}
}

[[nodiscard]] constexpr int accidentalSemitones(Accidental accidental) noexcept {
	return static_cast<int>(accidental);
}

[[nodiscard]] constexpr Accidental accidentalFromSemitones(int semitones) {
	if (semitones < std::numeric_limits<std::int8_t>::min() ||
		semitones > std::numeric_limits<std::int8_t>::max()) {
		throw std::out_of_range("Accidental semitone offset out of int8_t range");
	}
	return static_cast<Accidental>(semitones);
}

namespace detail {

[[nodiscard]] constexpr std::int64_t floorDiv64(std::int64_t a, std::int64_t b) noexcept {
	const std::int64_t q = a / b;
	const std::int64_t r = a % b;
	return (r < 0) ? (q - 1) : q;
}

[[nodiscard]] constexpr int floorMod12(int v) noexcept {
	const int r = v % 12;
	return (r < 0) ? (r + 12) : r;
}

} // namespace detail

/**
 * @brief Canonical spelled pitch (Letter, Accidental, octave) in scientific pitch notation.
 *
 * By convention, C4 is middle C (MIDI note 60) and C-1 is MIDI note 0.
 * Equality compares spelling strictly (F#4 != Gb4); enharmonic equivalence is
 * tested explicitly via isEnharmonicWith().
 */
class SpelledPitch {
public:
	constexpr SpelledPitch() noexcept = default;

	constexpr SpelledPitch(
		Letter letter,
		Accidental accidental = Accidental::Natural,
		int octave = 4)
		: letter_(letter), accidental_(accidental), octave_(octave) {
		if (!isValidLetter(letter_)) {
			throw std::invalid_argument("Invalid Letter in SpelledPitch");
		}
	}

	// -------------------------------------------------------------------------
	// Factories & Parsing
	// -------------------------------------------------------------------------

	/// Projects an integer MIDI note number onto a standard spelled pitch.
	[[nodiscard]] static constexpr SpelledPitch fromMidiNote(
		int midiNote,
		bool preferFlats = false) noexcept {
		constexpr Letter kSharpLetters[12] = {
			Letter::C, Letter::C, Letter::D, Letter::D, Letter::E, Letter::F,
			Letter::F, Letter::G, Letter::G, Letter::A, Letter::A, Letter::B
		};
		constexpr Accidental kSharpAcc[12] = {
			Accidental::Natural, Accidental::Sharp, Accidental::Natural, Accidental::Sharp,
			Accidental::Natural, Accidental::Natural, Accidental::Sharp, Accidental::Natural,
			Accidental::Sharp, Accidental::Natural, Accidental::Sharp, Accidental::Natural
		};
		constexpr Letter kFlatLetters[12] = {
			Letter::C, Letter::D, Letter::D, Letter::E, Letter::E, Letter::F,
			Letter::G, Letter::G, Letter::A, Letter::A, Letter::B, Letter::B
		};
		constexpr Accidental kFlatAcc[12] = {
			Accidental::Natural, Accidental::Flat, Accidental::Natural, Accidental::Flat,
			Accidental::Natural, Accidental::Natural, Accidental::Flat, Accidental::Natural,
			Accidental::Flat, Accidental::Natural, Accidental::Flat, Accidental::Natural
		};

		const std::int64_t m = midiNote;
		const int octave = static_cast<int>(detail::floorDiv64(m, 12) - 1);
		const int pc = detail::floorMod12(midiNote);
		if (preferFlats) {
			return SpelledPitch { kFlatLetters[pc], kFlatAcc[pc], octave };
		}
		return SpelledPitch { kSharpLetters[pc], kSharpAcc[pc], octave };
	}

	/// Parses scientific pitch notation such as "C4", "F#4", "Bb3", "F##4", "Fx4", "C-1".
	[[nodiscard]] static std::optional<SpelledPitch> fromString(std::string_view text);

	// -------------------------------------------------------------------------
	// Observers
	// -------------------------------------------------------------------------
	[[nodiscard]] constexpr Letter letter() const noexcept { return letter_; }
	[[nodiscard]] constexpr Accidental accidental() const noexcept { return accidental_; }
	[[nodiscard]] constexpr int octave() const noexcept { return octave_; }

	/// Diatonic step index relative to C0 (C0 = 0, D0 = 1, ..., C4 = 28).
	[[nodiscard]] constexpr int diatonicStep() const noexcept {
		return octave_ * 7 + static_cast<int>(letter_);
	}

	/// Pitch class in [0, 11], where C = 0.
	[[nodiscard]] constexpr int pitchClass() const noexcept {
		return detail::floorMod12(diatonicSemitoneUnchecked() + accidentalSemitones(accidental_));
	}

	/// Unclamped 12-TET MIDI note number (C-1 = 0, C4 = 60, G9 = 127).
	[[nodiscard]] constexpr int midiNote() const noexcept {
		return static_cast<int>(midiNote64());
	}

	/// Returns a standard 7-bit MIDI note number if in [0, 127], otherwise std::nullopt.
	[[nodiscard]] constexpr std::optional<std::uint8_t> toMidiNoteNumber() const noexcept {
		const std::int64_t note = midiNote64();
		if (note < 0 || note > 127) {
			return std::nullopt;
		}
		return static_cast<std::uint8_t>(note);
	}

	// -------------------------------------------------------------------------
	// Enharmonic & Spelling Operations
	// -------------------------------------------------------------------------
	[[nodiscard]] constexpr bool isEnharmonicWith(const SpelledPitch& other) const noexcept {
		return midiNote64() == other.midiNote64();
	}

	/// Respells this pitch onto targetLetter using the minimal accidental offset.
	[[nodiscard]] constexpr SpelledPitch respell(Letter targetLetter) const {
		if (!isValidLetter(targetLetter)) {
			throw std::invalid_argument("Invalid target Letter in SpelledPitch::respell");
		}
		const std::int64_t targetMidi = midiNote64();
		const std::int64_t baseAtZero = 12 + diatonicSemitone(targetLetter);
		const std::int64_t targetOctave = detail::floorDiv64((targetMidi - baseAtZero) + 5, 12);
		const std::int64_t diff = targetMidi - (targetOctave * 12 + baseAtZero);
		return SpelledPitch {
			targetLetter,
			static_cast<Accidental>(diff),
			static_cast<int>(targetOctave)
		};
	}

	/// Returns an enharmonic pitch with at most a single sharp or flat.
	[[nodiscard]] constexpr SpelledPitch simplify(bool preferFlats = false) const noexcept {
		return fromMidiNote(midiNote(), preferFlats);
	}

	[[nodiscard]] constexpr SpelledPitch withLetter(Letter newLetter) const {
		return SpelledPitch { newLetter, accidental_, octave_ };
	}

	[[nodiscard]] constexpr SpelledPitch withAccidental(Accidental newAccidental) const noexcept {
		return SpelledPitch { letter_, newAccidental, octave_ };
	}

	[[nodiscard]] constexpr SpelledPitch withOctave(int newOctave) const noexcept {
		return SpelledPitch { letter_, accidental_, newOctave };
	}

	[[nodiscard]] constexpr SpelledPitch shiftOctaves(int deltaOctaves) const noexcept {
		return SpelledPitch { letter_, accidental_, octave_ + deltaOctaves };
	}

	// -------------------------------------------------------------------------
	// Formatting & Comparisons
	// -------------------------------------------------------------------------
	[[nodiscard]] std::string toString() const;

	[[nodiscard]] constexpr bool operator==(const SpelledPitch& rhs) const noexcept = default;

	/// Orders by sounding pitch first, breaking enharmonic ties deterministically by spelling.
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const SpelledPitch& rhs) const noexcept {
		if (const auto cmp = midiNote64() <=> rhs.midiNote64(); cmp != 0) {
			return cmp;
		}
		if (const auto cmp = octave_ <=> rhs.octave_; cmp != 0) {
			return cmp;
		}
		if (const auto cmp = letter_ <=> rhs.letter_; cmp != 0) {
			return cmp;
		}
		return accidental_ <=> rhs.accidental_;
	}

private:
	[[nodiscard]] constexpr int diatonicSemitoneUnchecked() const noexcept {
		constexpr int kSemitones[7] = { 0, 2, 4, 5, 7, 9, 11 };
		return kSemitones[static_cast<std::size_t>(letter_)];
	}

	[[nodiscard]] constexpr std::int64_t midiNote64() const noexcept {
		return (static_cast<std::int64_t>(octave_) + 1) * 12
			+ static_cast<std::int64_t>(diatonicSemitoneUnchecked())
			+ static_cast<std::int64_t>(accidentalSemitones(accidental_));
	}

	Letter letter_ { Letter::C };
	Accidental accidental_ { Accidental::Natural };
	int octave_ { 4 };
};

std::ostream& operator<<(std::ostream& os, const SpelledPitch& pitch);

} // namespace duey

template <>
struct std::hash<duey::SpelledPitch> {
	[[nodiscard]] std::size_t operator()(const duey::SpelledPitch& pitch) const noexcept {
		const std::size_t h1 = std::hash<std::uint8_t> {}(static_cast<std::uint8_t>(pitch.letter()));
		const std::size_t h2 = std::hash<std::int8_t> {}(static_cast<std::int8_t>(pitch.accidental()));
		const std::size_t h3 = std::hash<int> {}(pitch.octave());
		std::size_t seed = h1;
		seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
		seed ^= h3 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
		return seed;
	}
};
