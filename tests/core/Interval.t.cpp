
#include "duey/core/Interval.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace duey::test {
namespace {

// -----------------------------------------------------------------------------
// Compile-time / constexpr verification
// -----------------------------------------------------------------------------
constexpr Interval kUnison {};
static_assert(kUnison.diatonicSteps() == 0);
static_assert(kUnison.semitones() == 0);
static_assert(kUnison.isUnison());
static_assert(!kUnison.isAscending());
static_assert(!kUnison.isDescending());

constexpr Interval kMajorThird = Interval::majorThird();
constexpr Interval kDimFourth = Interval::diminishedFourth();
static_assert(kMajorThird == Interval { 2, 4 });
static_assert(kDimFourth == Interval { 3, 4 });
static_assert(kMajorThird != kDimFourth);
static_assert(kMajorThird.isEnharmonic(kDimFourth));
static_assert(kMajorThird < kDimFourth);

static_assert(Interval::perfectFifth() + Interval::perfectFourth() == Interval::perfectOctave());
static_assert(Interval::perfectOctave() - Interval::majorThird() == Interval::minorSixth());
static_assert(Interval::majorThird().invert() == Interval::minorSixth());
static_assert(Interval::diminishedFourth().invert() == Interval { 4, 8 }); // A5
static_assert(-Interval::perfectFifth() == Interval { -4, -7 });
static_assert(Interval::perfectFifth() * 2 == Interval { 8, 14 }); // M9

// -----------------------------------------------------------------------------
// Construction & Observers
// -----------------------------------------------------------------------------
TEST(IntervalTest, DefaultConstructsToPerfectUnison) {
	const Interval u;
	EXPECT_EQ(u.diatonicSteps(), 0);
	EXPECT_EQ(u.semitones(), 0);
	EXPECT_TRUE(u.isUnison());
	EXPECT_FALSE(u.isAscending());
	EXPECT_FALSE(u.isDescending());
	EXPECT_FALSE(u.isCompound());
}

TEST(IntervalTest, DirectionAndCompoundClassification) {
	const Interval asc { 2, 4 };
	EXPECT_TRUE(asc.isAscending());
	EXPECT_FALSE(asc.isDescending());
	EXPECT_FALSE(asc.isCompound());

	const Interval augUnison { 0, 1 };
	EXPECT_TRUE(augUnison.isAscending());
	EXPECT_FALSE(augUnison.isDescending());

	const Interval dimUnison { 0, -1 };
	EXPECT_FALSE(dimUnison.isAscending());
	EXPECT_TRUE(dimUnison.isDescending());

	const Interval desc { -3, -5 };
	EXPECT_FALSE(desc.isAscending());
	EXPECT_TRUE(desc.isDescending());
	EXPECT_EQ(desc.abs(), Interval::perfectFourth());

	EXPECT_FALSE(Interval::perfectOctave().isCompound());
	EXPECT_TRUE((Interval { 8, 14 }).isCompound());
	EXPECT_TRUE((Interval { -8, -14 }).isCompound());
}

TEST(IntervalTest, EnharmonicPreservesSpelling) {
	const Interval m3Plus { 2, 4 }; // M3
	const Interval d4 { 3, 4 };     // d4

	EXPECT_NE(m3Plus, d4);
	EXPECT_TRUE(m3Plus.isEnharmonic(d4));
	EXPECT_TRUE(d4.isEnharmonic(m3Plus));
	EXPECT_FALSE(m3Plus.isEnharmonic(Interval::minorThird()));
}

// -----------------------------------------------------------------------------
// Arithmetic: Add, Subtract, Scale, Negate
// -----------------------------------------------------------------------------
TEST(IntervalTest, AddSubtractAndScale) {
	// M3 + m3 = P5
	EXPECT_EQ(Interval::majorThird() + Interval::minorThird(), Interval::perfectFifth());
	// P5 + P4 = P8
	EXPECT_EQ(Interval::perfectFifth() + Interval::perfectFourth(), Interval::perfectOctave());
	// P5 - M3 = m3
	EXPECT_EQ(Interval::perfectFifth() - Interval::majorThird(), Interval::minorThird());
	// d4 + P5 = d8 {7, 11}
	EXPECT_EQ(Interval::diminishedFourth() + Interval::perfectFifth(), (Interval { 7, 11 }));

	// Compound assignment
	Interval acc = Interval::majorSecond();
	acc += Interval::majorSecond();
	EXPECT_EQ(acc, Interval::majorThird());
	acc -= Interval::minorSecond();
	EXPECT_EQ(acc, (Interval { 1, 3 })); // A2

	// Scalar multiplication
	EXPECT_EQ(Interval::perfectFifth() * 2, (Interval { 8, 14 }));
	EXPECT_EQ(2 * Interval::perfectFifth(), (Interval { 8, 14 }));
	EXPECT_EQ(Interval::majorThird() * -2, (Interval { -4, -8 }));
	EXPECT_EQ(Interval::majorThird() * 0, Interval::perfectUnison());
}

TEST(IntervalTest, NegateAndAbs) {
	const Interval p5 = Interval::perfectFifth();
	EXPECT_EQ(p5.negate(), (Interval { -4, -7 }));
	EXPECT_EQ(-p5, (Interval { -4, -7 }));
	EXPECT_EQ(+p5, p5);
	EXPECT_EQ((-p5).negate(), p5);
	EXPECT_EQ((-p5).abs(), p5);
	EXPECT_EQ(p5.abs(), p5);
}

// -----------------------------------------------------------------------------
// Inversion
// -----------------------------------------------------------------------------
TEST(IntervalTest, InvertsSimpleIntervalsAndPreservesSpelling) {
	EXPECT_EQ(Interval::perfectUnison().invert(), Interval::perfectOctave());
	EXPECT_EQ(Interval::perfectOctave().invert(), Interval::perfectUnison());
	EXPECT_EQ(Interval::minorSecond().invert(), Interval::majorSeventh());
	EXPECT_EQ(Interval::majorSecond().invert(), Interval::minorSeventh());
	EXPECT_EQ(Interval::minorThird().invert(), Interval::majorSixth());
	EXPECT_EQ(Interval::majorThird().invert(), Interval::minorSixth());
	EXPECT_EQ(Interval::perfectFourth().invert(), Interval::perfectFifth());
	EXPECT_EQ(Interval::augmentedFourth().invert(), Interval::diminishedFifth());
	EXPECT_EQ(Interval::diminishedFourth().invert(), (Interval { 4, 8 })); // A5

	// Augmented / diminished unisons and octaves
	EXPECT_EQ((Interval { 0, 1 }).invert(), (Interval { 7, 11 }));  // A1 -> d8
	EXPECT_EQ((Interval { 7, 11 }).invert(), (Interval { 0, 1 }));  // d8 -> A1
}

TEST(IntervalTest, InvertsCompoundAndDescendingIntervals) {
	// M9 {8, 14} -> m7 {6, 10}
	EXPECT_EQ((Interval { 8, 14 }).invert(), Interval::minorSeventh());
	// M10 {9, 16} -> m6 {5, 8}
	EXPECT_EQ((Interval { 9, 16 }).invert(), Interval::minorSixth());
	// Two octaves {14, 24} -> P1 {0, 0}
	EXPECT_EQ((Interval { 14, 24 }).invert(), Interval::perfectUnison());

	// Descending intervals keep descending direction
	EXPECT_EQ((-Interval::majorThird()).invert(), -Interval::minorSixth());
	EXPECT_EQ((-Interval::perfectFifth()).invert(), -Interval::perfectFourth());
	EXPECT_EQ((-Interval::perfectOctave()).invert(), Interval::perfectUnison());
}

TEST(IntervalTest, InvertIsInvolutionForSimpleIntervals) {
	for (int steps = -6; steps <= 7; ++steps) {
		for (int semis = -14; semis <= 14; ++semis) {
			const Interval interval { steps, semis };
			EXPECT_EQ(interval.invert().invert(), interval);
		}
	}

	// Descending octave collapses sign at unison {0, 0}, so double-invert yields +P8.
	const Interval descOctave = -Interval::perfectOctave();
	EXPECT_EQ(descOctave.invert(), Interval::perfectUnison());
	EXPECT_EQ(descOctave.invert().invert(), Interval::perfectOctave());
}

// -----------------------------------------------------------------------------
// Overflow Checks
// -----------------------------------------------------------------------------
TEST(IntervalTest, OverflowThrowsOverflowError) {
	constexpr int kMax = std::numeric_limits<int>::max();
	constexpr int kMin = std::numeric_limits<int>::min();

	EXPECT_THROW((void) (Interval { kMax, 0 } + Interval { 1, 0 }), std::overflow_error);
	EXPECT_THROW((void) (Interval { 0, kMax } + Interval { 0, 1 }), std::overflow_error);
	EXPECT_THROW((void) (Interval { kMin, 0 } - Interval { 1, 0 }), std::overflow_error);
	EXPECT_THROW((void) (Interval { 0, kMin } - Interval { 0, 1 }), std::overflow_error);
	EXPECT_THROW((void) (Interval { kMin, 0 }).negate(), std::overflow_error);
	EXPECT_THROW((void) (Interval { 0, kMin }).negate(), std::overflow_error);
	EXPECT_THROW((void) (Interval { kMax, 0 } *2), std::overflow_error);
	EXPECT_THROW((void) (Interval { kMin, 0 } *-1), std::overflow_error);
	EXPECT_THROW((void) (Interval { 0, kMin }).invert(), std::overflow_error);
}

// -----------------------------------------------------------------------------
// Ordering, Hashing, Formatting
// -----------------------------------------------------------------------------
TEST(IntervalTest, OrderingAndHashing) {
	EXPECT_LT(Interval::minorThird(), Interval::majorThird());
	EXPECT_LT(Interval::majorThird(), Interval::diminishedFourth());
	EXPECT_LT(Interval::diminishedFourth(), Interval::perfectFourth());
	EXPECT_LT(-Interval::majorThird(), Interval::perfectUnison());

	std::unordered_set<Interval> set;
	set.insert(Interval::majorThird());
	EXPECT_TRUE(set.contains(Interval { 2, 4 }));
	EXPECT_FALSE(set.contains(Interval::diminishedFourth()));
}

TEST(IntervalTest, FormattingToStringAndStream) {
	EXPECT_EQ(Interval::perfectUnison().toString(), "P1");
	EXPECT_EQ(Interval::minorSecond().toString(), "m2");
	EXPECT_EQ(Interval::majorThird().toString(), "M3");
	EXPECT_EQ(Interval::diminishedFourth().toString(), "d4");
	EXPECT_EQ(Interval::augmentedFourth().toString(), "A4");
	EXPECT_EQ(Interval::diminishedFifth().toString(), "d5");
	EXPECT_EQ(Interval::perfectFifth().toString(), "P5");
	EXPECT_EQ(Interval::perfectOctave().toString(), "P8");
	EXPECT_EQ((Interval { 8, 14 }).toString(), "M9");
	EXPECT_EQ((Interval { 0, 1 }).toString(), "A1");
	EXPECT_EQ((Interval { 0, -1 }).toString(), "d1");
	EXPECT_EQ((-Interval::perfectFifth()).toString(), "-P5");
	EXPECT_EQ((Interval { 3, 12 }).toString(), "(3, 12)");

	std::ostringstream oss;
	oss << Interval::majorThird();
	EXPECT_EQ(oss.str(), "M3");
}

} // namespace
} // namespace duey::test
