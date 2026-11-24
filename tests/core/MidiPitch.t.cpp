
#include "duey/core/MidiPitch.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace duey::test {
namespace {

// -----------------------------------------------------------------------------
// Compile-time / constexpr verification
// -----------------------------------------------------------------------------
constexpr MidiPitch kDefaultPitch {};
static_assert(kDefaultPitch.noteNumber() == 0);
static_assert(kDefaultPitch.centOffset() == 0.0f);
static_assert(kDefaultPitch.isExactNote());

constexpr MidiPitch kNormalized { 60, 130.0f };
static_assert(kNormalized.noteNumber() == 61);
static_assert(kNormalized.centOffset() == 30.0f);
static_assert(!kNormalized.isExactNote());
static_assert(kNormalized.nearestNote() == MidiPitch { 61 });

constexpr MidiPitch kFromSemi = MidiPitch::fromSemitones(61.3);
static_assert(kFromSemi.noteNumber() == 61);
static_assert(kFromSemi.approxEqual(kNormalized));

static_assert(MidiPitch { 60 }.transpose(12) == MidiPitch { 72 });
static_assert(MidiPitch { 60 }.transposeCents(130.0f) == MidiPitch { 61, 30.0f });
static_assert(MidiPitch { 60 }.centsTo(MidiPitch { 61, 30.0f }) == 130.0);
static_assert(MidiPitch { 59, 40.0f } < MidiPitch { 60, -40.0f });

// -----------------------------------------------------------------------------
// Construction & Normalization
// -----------------------------------------------------------------------------
TEST(MidiPitchTest, DefaultConstructsToMidiZero) {
	const MidiPitch p;
	EXPECT_EQ(p.noteNumber(), 0);
	EXPECT_FLOAT_EQ(p.centOffset(), 0.0f);
	EXPECT_TRUE(p.isExactNote());
}

TEST(MidiPitchTest, NormalizesPositiveAndNegativeCents) {
	// Example from TDD: 60 + 130c -> 61 + 30c
	const MidiPitch a { 60, 130.0f };
	EXPECT_EQ(a.noteNumber(), 61);
	EXPECT_FLOAT_EQ(a.centOffset(), 30.0f);

	// Half-open [-50, 50) boundary
	const MidiPitch halfUp { 60, 50.0f };
	EXPECT_EQ(halfUp.noteNumber(), 61);
	EXPECT_FLOAT_EQ(halfUp.centOffset(), -50.0f);

	const MidiPitch halfDown { 60, -50.0f };
	EXPECT_EQ(halfDown.noteNumber(), 60);
	EXPECT_FLOAT_EQ(halfDown.centOffset(), -50.0f);

	const MidiPitch wrapDown { 60, -50.1f };
	EXPECT_EQ(wrapDown.noteNumber(), 59);
	EXPECT_NEAR(wrapDown.centOffset(), 49.9f, 1e-4f);

	const MidiPitch multiOctaveUp { 60, 1200.0f };
	EXPECT_EQ(multiOctaveUp, MidiPitch { 72 });

	const MidiPitch multiOctaveDown { 60, -1200.0f };
	EXPECT_EQ(multiOctaveDown, MidiPitch { 48 });

	// Input note outside [0, 127] that normalizes back inside [0, 127]
	const MidiPitch backInLow { -1, 100.0f };
	EXPECT_EQ(backInLow, MidiPitch { 0 });

	const MidiPitch backInHigh { 128, -100.0f };
	EXPECT_EQ(backInHigh, MidiPitch { 127 });
}

TEST(MidiPitchTest, CanonicalizesNegativeZeroCents) {
	const MidiPitch p { 60, -0.0f };
	EXPECT_EQ(p.centOffset(), 0.0f);
	EXPECT_FALSE(std::signbit(p.centOffset()));
}

TEST(MidiPitchTest, BoundaryNotesAreAllowed) {
	const MidiPitch low { 0, -50.0f };
	EXPECT_EQ(low.noteNumber(), 0);
	EXPECT_FLOAT_EQ(low.centOffset(), -50.0f);

	const MidiPitch high { 127, 49.5f };
	EXPECT_EQ(high.noteNumber(), 127);
	EXPECT_FLOAT_EQ(high.centOffset(), 49.5f);
}

TEST(MidiPitchTest, RejectsOutOfRangeNotes) {
	EXPECT_THROW((void) MidiPitch { -1 }, std::out_of_range);
	EXPECT_THROW((void) MidiPitch { 128 }, std::out_of_range);
	EXPECT_THROW((void) (MidiPitch { 0, -50.1f }), std::out_of_range);
	EXPECT_THROW((void) (MidiPitch { 127, 50.0f }), std::out_of_range);
	EXPECT_THROW((void) (MidiPitch { 60, 1e20f }), std::out_of_range);
	EXPECT_THROW((void) (MidiPitch { 60, -1e20f }), std::out_of_range);
}

TEST(MidiPitchTest, RejectsNonFiniteCents) {
	constexpr float kNan = std::numeric_limits<float>::quiet_NaN();
	constexpr float kInf = std::numeric_limits<float>::infinity();

	EXPECT_THROW((void) (MidiPitch { 60, kNan }), std::invalid_argument);
	EXPECT_THROW((void) (MidiPitch { 60, kInf }), std::invalid_argument);
	EXPECT_THROW((void) (MidiPitch { 60, -kInf }), std::invalid_argument);
}

// -----------------------------------------------------------------------------
// Semitone Conversions
// -----------------------------------------------------------------------------
TEST(MidiPitchTest, FromAndToSemitones) {
	const MidiPitch c4 = MidiPitch::fromSemitones(60.0);
	EXPECT_EQ(c4, MidiPitch { 60 });
	EXPECT_DOUBLE_EQ(c4.toSemitones(), 60.0);

	const MidiPitch quarterTone = MidiPitch::fromSemitones(60.5);
	EXPECT_EQ(quarterTone.noteNumber(), 61);
	EXPECT_FLOAT_EQ(quarterTone.centOffset(), -50.0f);
	EXPECT_DOUBLE_EQ(quarterTone.toSemitones(), 60.5);

	const MidiPitch nearZero = MidiPitch::fromSemitones(-0.4);
	EXPECT_EQ(nearZero.noteNumber(), 0);
	EXPECT_NEAR(nearZero.centOffset(), -40.0f, 1e-4f);

	EXPECT_THROW((void) MidiPitch::fromSemitones(-0.6), std::out_of_range);
	EXPECT_THROW((void) MidiPitch::fromSemitones(127.5), std::out_of_range);
	EXPECT_THROW((void) MidiPitch::fromSemitones(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
	EXPECT_THROW((void) MidiPitch::fromSemitones(std::numeric_limits<double>::infinity()), std::invalid_argument);
}

// -----------------------------------------------------------------------------
// Transposition & Distances
// -----------------------------------------------------------------------------
TEST(MidiPitchTest, TransposeAndCentsDelta) {
	const MidiPitch a4 { 69, -15.0f };

	EXPECT_EQ(a4.transpose(5), (MidiPitch { 74, -15.0f }));
	EXPECT_EQ(a4.transpose(-9), (MidiPitch { 60, -15.0f }));
	EXPECT_THROW((void) a4.transpose(60), std::out_of_range);
	EXPECT_THROW((void) a4.transpose(-70), std::out_of_range);
	EXPECT_THROW((void) a4.transpose(std::numeric_limits<int>::max()), std::out_of_range);

	const MidiPitch shifted = a4.transposeCents(80.0f); // 69 + 65c -> 70 - 35c
	EXPECT_EQ(shifted.noteNumber(), 70);
	EXPECT_FLOAT_EQ(shifted.centOffset(), -35.0f);

	EXPECT_THROW((void) a4.transposeCents(std::numeric_limits<float>::max()), std::out_of_range);
	EXPECT_THROW((void) a4.transposeCents(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
}

TEST(MidiPitchTest, CentsToSemitonesToAndApproxEqual) {
	const MidiPitch p1 { 60, 20.0f };
	const MidiPitch p2 { 61, -30.0f };

	EXPECT_NEAR(p1.centsTo(p2), 50.0, 1e-5);
	EXPECT_NEAR(p2.centsTo(p1), -50.0, 1e-5);
	EXPECT_NEAR(p1.semitonesTo(p2), 0.5, 1e-7);

	EXPECT_TRUE(p1.approxEqual(MidiPitch { 60, 20.0005f }, 1e-3f));
	EXPECT_FALSE(p1.approxEqual(MidiPitch { 60, 20.1f }, 1e-3f));
}

// -----------------------------------------------------------------------------
// Ordering, Hashing, Formatting
// -----------------------------------------------------------------------------
TEST(MidiPitchTest, TotalOrderingMatchesPitchHeight) {
	const MidiPitch p0 { 59, 40.0f };
	const MidiPitch p1 { 60, -40.0f };
	const MidiPitch p2 { 60, 0.0f };
	const MidiPitch p3 { 60, 40.0f };
	const MidiPitch p4 { 61, -40.0f };

	EXPECT_LT(p0, p1);
	EXPECT_LT(p1, p2);
	EXPECT_LT(p2, p3);
	EXPECT_LT(p3, p4);
	EXPECT_GT(p4, p0);
	EXPECT_EQ(p2, MidiPitch { 60 });
}

TEST(MidiPitchTest, HashWorksInUnorderedSet) {
	std::unordered_set<MidiPitch> set;
	set.insert(MidiPitch { 60, 130.0f });

	EXPECT_TRUE(set.contains(MidiPitch { 61, 30.0f }));
	EXPECT_FALSE(set.contains(MidiPitch { 61, -30.0f }));
}

TEST(MidiPitchTest, FormattingToStringAndStream) {
	EXPECT_EQ((MidiPitch { 60 }).toString(), "60");
	EXPECT_EQ((MidiPitch { 60, 130.0f }).toString(), "61+30c");
	EXPECT_EQ((MidiPitch { 64, -14.5f }).toString(), "64-14.5c");

	std::ostringstream oss;
	oss << MidiPitch { 61, 30.0f };
	EXPECT_EQ(oss.str(), "61+30c");
}

} // namespace
} // namespace duey::test
