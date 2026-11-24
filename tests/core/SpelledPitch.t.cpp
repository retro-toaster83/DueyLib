
#include "duey/core/SpelledPitch.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

namespace duey::test {
namespace {

// -----------------------------------------------------------------------------
// Compile-time & Value-Type Checks
// -----------------------------------------------------------------------------
static_assert(std::is_trivially_copyable_v<SpelledPitch>);

constexpr SpelledPitch kMiddleC {};
static_assert(kMiddleC.letter() == Letter::C);
static_assert(kMiddleC.accidental() == Accidental::Natural);
static_assert(kMiddleC.octave() == 4);
static_assert(kMiddleC.midiNote() == 60);
static_assert(kMiddleC.pitchClass() == 0);
static_assert(kMiddleC.diatonicStep() == 28);
static_assert(kMiddleC.toMidiNoteNumber() == std::uint8_t { 60 });

constexpr SpelledPitch kFSharp4 { Letter::F, Accidental::Sharp, 4 };
constexpr SpelledPitch kGFlat4 { Letter::G, Accidental::Flat, 4 };
static_assert(kFSharp4 != kGFlat4);
static_assert(kFSharp4.isEnharmonicWith(kGFlat4));
static_assert(kFSharp4 < kGFlat4);
static_assert(kFSharp4.respell(Letter::G) == kGFlat4);
static_assert(SpelledPitch::fromMidiNote(66, true) == kGFlat4);
static_assert(SpelledPitch::fromMidiNote(66, false) == kFSharp4);

// -----------------------------------------------------------------------------
// Letter & Accidental Helpers
// -----------------------------------------------------------------------------
TEST(SpelledPitchTest, LetterAndAccidentalHelpers) {
	EXPECT_EQ(diatonicSemitone(Letter::C), 0);
	EXPECT_EQ(diatonicSemitone(Letter::D), 2);
	EXPECT_EQ(diatonicSemitone(Letter::E), 4);
	EXPECT_EQ(diatonicSemitone(Letter::F), 5);
	EXPECT_EQ(diatonicSemitone(Letter::G), 7);
	EXPECT_EQ(diatonicSemitone(Letter::A), 9);
	EXPECT_EQ(diatonicSemitone(Letter::B), 11);

	EXPECT_EQ(toChar(Letter::A), 'A');
	EXPECT_EQ(letterFromChar('g'), Letter::G);
	EXPECT_EQ(letterFromChar('F'), Letter::F);
	EXPECT_EQ(letterFromChar('H'), std::nullopt);

	const auto invalidLetter = static_cast<Letter>(7);
	EXPECT_FALSE(isValidLetter(invalidLetter));
	EXPECT_THROW((void) diatonicSemitone(invalidLetter), std::invalid_argument);
	EXPECT_THROW((void) toChar(invalidLetter), std::invalid_argument);

	EXPECT_EQ(accidentalSemitones(Accidental::DoubleFlat), -2);
	EXPECT_EQ(accidentalSemitones(Accidental::DoubleSharp), 2);
	EXPECT_EQ(accidentalFromSemitones(3), Accidental { 3 });
	EXPECT_THROW((void) accidentalFromSemitones(128), std::out_of_range);
	EXPECT_THROW((void) accidentalFromSemitones(-129), std::out_of_range);
}

// -----------------------------------------------------------------------------
// Construction & Observers
// -----------------------------------------------------------------------------
TEST(SpelledPitchTest, DefaultAndExplicitConstruction) {
	const SpelledPitch def;
	EXPECT_EQ(def.letter(), Letter::C);
	EXPECT_EQ(def.accidental(), Accidental::Natural);
	EXPECT_EQ(def.octave(), 4);
	EXPECT_EQ(def.midiNote(), 60);

	const SpelledPitch a4 { Letter::A, Accidental::Natural, 4 };
	EXPECT_EQ(a4.midiNote(), 69);
	EXPECT_EQ(a4.pitchClass(), 9);
	EXPECT_EQ(a4.diatonicStep(), 33);

	const SpelledPitch tripleSharp { Letter::C, accidentalFromSemitones(3), 4 };
	EXPECT_EQ(tripleSharp.midiNote(), 63);
	EXPECT_EQ(tripleSharp.pitchClass(), 3);

	EXPECT_THROW(
		(SpelledPitch { static_cast<Letter>(42), Accidental::Natural, 4 }),
		std::invalid_argument);
}

TEST(SpelledPitchTest, MidiNoteAndSevenBitRange) {
	EXPECT_EQ((SpelledPitch { Letter::C, Accidental::Natural, -1 }.midiNote()), 0);
	EXPECT_EQ((SpelledPitch { Letter::C, Accidental::Natural, -1 }.toMidiNoteNumber()), 0);

	EXPECT_EQ((SpelledPitch { Letter::G, Accidental::Natural, 9 }.midiNote()), 127);
	EXPECT_EQ((SpelledPitch { Letter::G, Accidental::Natural, 9 }.toMidiNoteNumber()), 127);

	// Out of 7-bit MIDI range
	const SpelledPitch tooLow { Letter::C, Accidental::Flat, -1 };
	EXPECT_EQ(tooLow.midiNote(), -1);
	EXPECT_EQ(tooLow.toMidiNoteNumber(), std::nullopt);
	EXPECT_EQ(tooLow.pitchClass(), 11);

	const SpelledPitch tooHigh { Letter::G, Accidental::Sharp, 9 };
	EXPECT_EQ(tooHigh.midiNote(), 128);
	EXPECT_EQ(tooHigh.toMidiNoteNumber(), std::nullopt);
	EXPECT_EQ(tooHigh.pitchClass(), 8);
}

// -----------------------------------------------------------------------------
// Enharmonicity, Respelling, and Simplification
// -----------------------------------------------------------------------------
TEST(SpelledPitchTest, EnharmonicComparisonIsExplicit) {
	const SpelledPitch fSharp { Letter::F, Accidental::Sharp, 4 };
	const SpelledPitch gFlat { Letter::G, Accidental::Flat, 4 };
	const SpelledPitch bSharp3 { Letter::B, Accidental::Sharp, 3 };
	const SpelledPitch c4 { Letter::C, Accidental::Natural, 4 };
	const SpelledPitch cFlat4 { Letter::C, Accidental::Flat, 4 };
	const SpelledPitch b3 { Letter::B, Accidental::Natural, 3 };
	const SpelledPitch c5 { Letter::C, Accidental::Natural, 5 };

	EXPECT_NE(fSharp, gFlat);
	EXPECT_TRUE(fSharp.isEnharmonicWith(gFlat));

	EXPECT_NE(bSharp3, c4);
	EXPECT_TRUE(bSharp3.isEnharmonicWith(c4));

	EXPECT_NE(cFlat4, b3);
	EXPECT_TRUE(cFlat4.isEnharmonicWith(b3));

	EXPECT_FALSE(c4.isEnharmonicWith(c5));
}

TEST(SpelledPitchTest, RespellAcrossOctaveBoundaries) {
	const SpelledPitch c4 { Letter::C, Accidental::Natural, 4 };
	EXPECT_EQ(c4.respell(Letter::B), (SpelledPitch { Letter::B, Accidental::Sharp, 3 }));
	EXPECT_EQ(c4.respell(Letter::D), (SpelledPitch { Letter::D, Accidental::DoubleFlat, 4 }));

	const SpelledPitch b3 { Letter::B, Accidental::Natural, 3 };
	EXPECT_EQ(b3.respell(Letter::C), (SpelledPitch { Letter::C, Accidental::Flat, 4 }));

	const SpelledPitch cMinus1 { Letter::C, Accidental::Natural, -1 };
	EXPECT_EQ(cMinus1.respell(Letter::B), (SpelledPitch { Letter::B, Accidental::Sharp, -2 }));
	EXPECT_EQ(cMinus1.respell(Letter::B).midiNote(), 0);

	EXPECT_THROW((void) c4.respell(static_cast<Letter>(99)), std::invalid_argument);
}

TEST(SpelledPitchTest, FromMidiNoteAndSimplify) {
	EXPECT_EQ(SpelledPitch::fromMidiNote(60, false), (SpelledPitch { Letter::C, Accidental::Natural, 4 }));
	EXPECT_EQ(SpelledPitch::fromMidiNote(61, false), (SpelledPitch { Letter::C, Accidental::Sharp, 4 }));
	EXPECT_EQ(SpelledPitch::fromMidiNote(61, true), (SpelledPitch { Letter::D, Accidental::Flat, 4 }));
	EXPECT_EQ(SpelledPitch::fromMidiNote(0, false), (SpelledPitch { Letter::C, Accidental::Natural, -1 }));
	EXPECT_EQ(SpelledPitch::fromMidiNote(-1, false), (SpelledPitch { Letter::B, Accidental::Natural, -2 }));
	EXPECT_EQ(SpelledPitch::fromMidiNote(127, false), (SpelledPitch { Letter::G, Accidental::Natural, 9 }));

	const SpelledPitch eSharp4 { Letter::E, Accidental::Sharp, 4 };
	EXPECT_EQ(eSharp4.simplify(), (SpelledPitch { Letter::F, Accidental::Natural, 4 }));

	const SpelledPitch cDoubleFlat4 { Letter::C, Accidental::DoubleFlat, 4 };
	EXPECT_EQ(cDoubleFlat4.simplify(true), (SpelledPitch { Letter::B, Accidental::Flat, 3 }));
	EXPECT_EQ(cDoubleFlat4.simplify(false), (SpelledPitch { Letter::A, Accidental::Sharp, 3 }));
}

TEST(SpelledPitchTest, WithMutatorsAndShiftOctaves) {
	const SpelledPitch c4;
	EXPECT_EQ(c4.withLetter(Letter::E), (SpelledPitch { Letter::E, Accidental::Natural, 4 }));
	EXPECT_EQ(c4.withAccidental(Accidental::Sharp), (SpelledPitch { Letter::C, Accidental::Sharp, 4 }));
	EXPECT_EQ(c4.withOctave(6), (SpelledPitch { Letter::C, Accidental::Natural, 6 }));
	EXPECT_EQ(c4.shiftOctaves(-2), (SpelledPitch { Letter::C, Accidental::Natural, 2 }));
	EXPECT_THROW((void) c4.withLetter(static_cast<Letter>(10)), std::invalid_argument);
}

// -----------------------------------------------------------------------------
// Ordering & Hashing
// -----------------------------------------------------------------------------
TEST(SpelledPitchTest, OrderingPreservesSoundingPitchAndTieBreaksDeterministically) {
	const SpelledPitch b3 { Letter::B, Accidental::Natural, 3 };
	const SpelledPitch cFlat4 { Letter::C, Accidental::Flat, 4 };
	const SpelledPitch bSharp3 { Letter::B, Accidental::Sharp, 3 };
	const SpelledPitch c4 { Letter::C, Accidental::Natural, 4 };
	const SpelledPitch fSharp4 { Letter::F, Accidental::Sharp, 4 };
	const SpelledPitch gFlat4 { Letter::G, Accidental::Flat, 4 };
	const SpelledPitch fDoubleSharp4 { Letter::F, Accidental::DoubleSharp, 4 };

	EXPECT_LT(b3, cFlat4);
	EXPECT_LT(cFlat4, bSharp3);
	EXPECT_LT(bSharp3, c4);
	EXPECT_LT(fSharp4, gFlat4);
	EXPECT_LT(gFlat4, fDoubleSharp4);

	// Extreme octaves should not cause signed overflow UB in operator<=>
	const SpelledPitch minOct { Letter::C, Accidental::Natural, std::numeric_limits<int>::min() };
	const SpelledPitch maxOct { Letter::B, Accidental::Natural, std::numeric_limits<int>::max() };
	EXPECT_LT(minOct, maxOct);
}

TEST(SpelledPitchTest, HashDistinguishesSpellings) {
	std::unordered_set<SpelledPitch> pitches;
	pitches.insert(SpelledPitch { Letter::F, Accidental::Sharp, 4 });
	EXPECT_TRUE(pitches.contains(SpelledPitch { Letter::F, Accidental::Sharp, 4 }));
	EXPECT_FALSE(pitches.contains(SpelledPitch { Letter::G, Accidental::Flat, 4 }));
}

// -----------------------------------------------------------------------------
// Parsing & Formatting
// -----------------------------------------------------------------------------
TEST(SpelledPitchTest, ToStringAndStreamFormatting) {
	const SpelledPitch bb3 { Letter::B, Accidental::Flat, 3 };
	EXPECT_EQ(bb3.toString(), "Bb3");

	const SpelledPitch fDoubleSharp4 { Letter::F, Accidental::DoubleSharp, 4 };
	EXPECT_EQ(fDoubleSharp4.toString(), "F##4");

	const SpelledPitch cMinus1 { Letter::C, Accidental::Natural, -1 };
	EXPECT_EQ(cMinus1.toString(), "C-1");

	std::ostringstream oss;
	oss << fDoubleSharp4 << ' ' << bb3;
	EXPECT_EQ(oss.str(), "F##4 Bb3");
}

TEST(SpelledPitchTest, FromStringValidInputsAndRoundTrip) {
	EXPECT_EQ(SpelledPitch::fromString("C4"), (SpelledPitch { Letter::C, Accidental::Natural, 4 }));
	EXPECT_EQ(SpelledPitch::fromString("  f#4 \t"), (SpelledPitch { Letter::F, Accidental::Sharp, 4 }));
	EXPECT_EQ(SpelledPitch::fromString("bb3"), (SpelledPitch { Letter::B, Accidental::Flat, 3 }));
	EXPECT_EQ(SpelledPitch::fromString("Fx4"), (SpelledPitch { Letter::F, Accidental::DoubleSharp, 4 }));
	EXPECT_EQ(SpelledPitch::fromString("Bbb3"), (SpelledPitch { Letter::B, Accidental::DoubleFlat, 3 }));
	EXPECT_EQ(SpelledPitch::fromString("C###5"), (SpelledPitch { Letter::C, accidentalFromSemitones(3), 5 }));
	EXPECT_EQ(SpelledPitch::fromString("C-1"), (SpelledPitch { Letter::C, Accidental::Natural, -1 }));
	EXPECT_EQ(SpelledPitch::fromString("A+4"), (SpelledPitch { Letter::A, Accidental::Natural, 4 }));

	for (int offset = -4; offset <= 4; ++offset) {
		const SpelledPitch p { Letter::D, accidentalFromSemitones(offset), -2 };
		EXPECT_EQ(SpelledPitch::fromString(p.toString()), p);
	}
}

TEST(SpelledPitchTest, FromStringRejectsMalformedInputs) {
	EXPECT_EQ(SpelledPitch::fromString(""), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("   "), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("H4"), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("C"), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("C#"), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("C-"), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("C+"), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("C+-1"), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("C#b4"), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("Cx#4"), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("C4abc"), std::nullopt);
	EXPECT_EQ(SpelledPitch::fromString("C9999999999999999999"), std::nullopt);

	const std::string tooManySharps = "C" + std::string(128, '#') + "4";
	EXPECT_EQ(SpelledPitch::fromString(tooManySharps), std::nullopt);
}

} // namespace
} // namespace duey::test
