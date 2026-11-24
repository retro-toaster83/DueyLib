
#include "duey/core/TimeRange.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace duey::test {
namespace {

// -----------------------------------------------------------------------------
// Compile-time / constexpr verification
// -----------------------------------------------------------------------------
constexpr TimeRange kDefaultRange {};
static_assert(kDefaultRange.start() == Rational { 0 });
static_assert(kDefaultRange.duration() == Rational { 0 });
static_assert(kDefaultRange.end() == Rational { 0 });
static_assert(kDefaultRange.isEmpty());

constexpr TimeRange kQuarterNote { Rational { 1, 4 }, Rational { 1, 4 } };
static_assert(kQuarterNote.end() == Rational { 1, 2 });
static_assert(!kQuarterNote.isEmpty());
static_assert(kQuarterNote.contains(Rational { 1, 4 }));
static_assert(kQuarterNote.contains(Rational { 3, 8 }));
static_assert(!kQuarterNote.contains(Rational { 1, 2 }));

constexpr TimeRange kFromEnd = TimeRange::fromEndpoints(Rational { 1, 4 }, Rational { 3, 4 });
static_assert(kFromEnd.duration() == Rational { 1, 2 });
static_assert(kFromEnd.contains(kQuarterNote));
static_assert(kFromEnd.overlaps(kQuarterNote));
static_assert(kFromEnd.intersect(kQuarterNote) == kQuarterNote);
static_assert(kQuarterNote.shift(Rational { 1, 4 }) == TimeRange { Rational { 1, 2 }, Rational { 1, 4 } });
static_assert(kQuarterNote < kFromEnd);

// -----------------------------------------------------------------------------
// Construction & Invariants
// -----------------------------------------------------------------------------
TEST(TimeRangeTest, DefaultConstructsToZeroEmptyRange) {
	const TimeRange r;
	EXPECT_EQ(r.start(), Rational { 0 });
	EXPECT_EQ(r.duration(), Rational { 0 });
	EXPECT_EQ(r.end(), Rational { 0 });
	EXPECT_TRUE(r.isEmpty());
}

TEST(TimeRangeTest, ConstructsValidRanges) {
	const TimeRange pos { Rational { 3, 4 }, Rational { 1, 2 } };
	EXPECT_EQ(pos.start(), (Rational { 3, 4 }));
	EXPECT_EQ(pos.duration(), (Rational { 1, 2 }));
	EXPECT_EQ(pos.end(), (Rational { 5, 4 }));
	EXPECT_FALSE(pos.isEmpty());

	const TimeRange negStart { Rational { -1, 4 }, Rational { 3, 4 } };
	EXPECT_EQ(negStart.start(), (Rational { -1, 4 }));
	EXPECT_EQ(negStart.end(), (Rational { 1, 2 }));

	const TimeRange zeroDur { Rational { 2 }, Rational { 0 } };
	EXPECT_TRUE(zeroDur.isEmpty());
	EXPECT_EQ(zeroDur.end(), Rational { 2 });
}

TEST(TimeRangeTest, RejectsNegativeDuration) {
	EXPECT_THROW((TimeRange { Rational { 0 }, Rational { -1, 4 } }), std::invalid_argument);
	EXPECT_THROW((TimeRange { Rational { 5 }, Rational { -1 } }), std::invalid_argument);
}

TEST(TimeRangeTest, FromEndpointsValidAndInvalid) {
	const TimeRange r = TimeRange::fromEndpoints(Rational { 1, 3 }, Rational { 5, 6 });
	EXPECT_EQ(r.start(), (Rational { 1, 3 }));
	EXPECT_EQ(r.duration(), (Rational { 1, 2 }));
	EXPECT_EQ(r.end(), (Rational { 5, 6 }));

	const TimeRange empty = TimeRange::fromEndpoints(Rational { 2 }, Rational { 2 });
	EXPECT_TRUE(empty.isEmpty());
	EXPECT_EQ(empty.start(), Rational { 2 });

	EXPECT_THROW(
		(void) TimeRange::fromEndpoints(Rational { 1 }, Rational { 1, 2 }),
		std::invalid_argument);
}

TEST(TimeRangeTest, OverflowHandling) {
	constexpr auto kMax = std::numeric_limits<std::int64_t>::max();

	const TimeRange nearMax { Rational { kMax }, Rational { 1 } };
	EXPECT_THROW((void) nearMax.end(), std::overflow_error);
	EXPECT_THROW((void) nearMax.shift(Rational { 1 }), std::overflow_error);

	EXPECT_THROW(
		(void) TimeRange::fromEndpoints(Rational { -1 }, Rational { kMax }),
		std::overflow_error);
}

// -----------------------------------------------------------------------------
// Contains (Point & Range)
// -----------------------------------------------------------------------------
TEST(TimeRangeTest, ContainsPointHalfOpenSemantics) {
	const TimeRange r { Rational { 1, 4 }, Rational { 1, 2 } }; // [1/4, 3/4)

	EXPECT_FALSE(r.contains(Rational { 0 }));
	EXPECT_FALSE(r.contains(Rational { 1, 4 } - Rational { 1, 1000 }));
	EXPECT_TRUE(r.contains(Rational { 1, 4 }));
	EXPECT_TRUE(r.contains(Rational { 1, 2 }));
	EXPECT_TRUE(r.contains(Rational { 3, 4 } - Rational { 1, 1000 }));
	EXPECT_FALSE(r.contains(Rational { 3, 4 }));
	EXPECT_FALSE(r.contains(Rational { 1 }));
}

TEST(TimeRangeTest, EmptyRangeContainsNoPoints) {
	const TimeRange emptyAtZero {};
	EXPECT_FALSE(emptyAtZero.contains(Rational { 0 }));

	const TimeRange emptyAtOne { Rational { 1 }, Rational { 0 } };
	EXPECT_FALSE(emptyAtOne.contains(Rational { 1 }));
}

TEST(TimeRangeTest, ContainsRange) {
	const TimeRange outer { Rational { 1 }, Rational { 3 } }; // [1, 4)

	EXPECT_TRUE(outer.contains(outer));
	EXPECT_TRUE(outer.contains(TimeRange { Rational { 1 }, Rational { 1 } }));
	EXPECT_TRUE(outer.contains(TimeRange { Rational { 2 }, Rational { 1 } }));
	EXPECT_TRUE(outer.contains(TimeRange { Rational { 3 }, Rational { 1 } }));

	// Partial or outside
	EXPECT_FALSE(outer.contains(TimeRange { Rational { 0 }, Rational { 2 } }));
	EXPECT_FALSE(outer.contains(TimeRange { Rational { 3 }, Rational { 2 } }));
	EXPECT_FALSE(outer.contains(TimeRange { Rational { 4 }, Rational { 1 } }));
	EXPECT_FALSE(outer.contains(TimeRange { Rational { 0 }, Rational { 5 } }));

	// Empty ranges
	EXPECT_TRUE(outer.contains(TimeRange { Rational { 2 }, Rational { 0 } }));
	EXPECT_FALSE(outer.contains(TimeRange { Rational { 0 }, Rational { 0 } }));

	const TimeRange empty { Rational { 2 }, Rational { 0 } };
	EXPECT_TRUE(empty.contains(empty));
	EXPECT_FALSE(empty.contains(TimeRange { Rational { 2 }, Rational { 1 } }));
}

// -----------------------------------------------------------------------------
// Overlaps & Intersect
// -----------------------------------------------------------------------------
TEST(TimeRangeTest, OverlapsAndIntersect) {
	const TimeRange a { Rational { 0 }, Rational { 2 } }; // [0, 2)
	const TimeRange b { Rational { 1 }, Rational { 2 } }; // [1, 3)

	EXPECT_TRUE(a.overlaps(b));
	EXPECT_TRUE(b.overlaps(a));
	EXPECT_EQ(a.intersect(b), (TimeRange { Rational { 1 }, Rational { 1 } }));
	EXPECT_EQ(b.intersect(a), (TimeRange { Rational { 1 }, Rational { 1 } }));

	// Nested
	const TimeRange inner { Rational { 1, 2 }, Rational { 1 } }; // [1/2, 3/2)
	EXPECT_TRUE(a.overlaps(inner));
	EXPECT_EQ(a.intersect(inner), inner);
	EXPECT_EQ(inner.intersect(a), inner);

	// Abutting intervals do not overlap
	const TimeRange c { Rational { 2 }, Rational { 1 } }; // [2, 3)
	EXPECT_FALSE(a.overlaps(c));
	EXPECT_FALSE(c.overlaps(a));
	EXPECT_EQ(a.intersect(c), std::nullopt);
	EXPECT_EQ(c.intersect(a), std::nullopt);

	// Disjoint
	const TimeRange d { Rational { 5 }, Rational { 1 } }; // [5, 6)
	EXPECT_FALSE(a.overlaps(d));
	EXPECT_EQ(a.intersect(d), std::nullopt);
}

TEST(TimeRangeTest, EmptyRangesNeverOverlapOrIntersect) {
	const TimeRange nonEmpty { Rational { 0 }, Rational { 2 } };
	const TimeRange emptyInside { Rational { 1 }, Rational { 0 } };
	const TimeRange emptyAtStart { Rational { 0 }, Rational { 0 } };

	EXPECT_FALSE(nonEmpty.overlaps(emptyInside));
	EXPECT_FALSE(emptyInside.overlaps(nonEmpty));
	EXPECT_FALSE(emptyInside.overlaps(emptyInside));
	EXPECT_FALSE(nonEmpty.overlaps(emptyAtStart));

	EXPECT_EQ(nonEmpty.intersect(emptyInside), std::nullopt);
	EXPECT_EQ(emptyInside.intersect(nonEmpty), std::nullopt);
	EXPECT_EQ(emptyInside.intersect(emptyInside), std::nullopt);
	EXPECT_EQ(nonEmpty.intersect(emptyAtStart), std::nullopt);
}

// -----------------------------------------------------------------------------
// Shift
// -----------------------------------------------------------------------------
TEST(TimeRangeTest, ShiftTranslatesStartPreservingDuration) {
	const TimeRange r { Rational { 1, 3 }, Rational { 2, 3 } };

	const TimeRange forward = r.shift(Rational { 1, 2 });
	EXPECT_EQ(forward.start(), (Rational { 5, 6 }));
	EXPECT_EQ(forward.duration(), (Rational { 2, 3 }));
	EXPECT_EQ(forward.end(), (Rational { 3, 2 }));

	const TimeRange backward = forward.shift(Rational { -1, 2 });
	EXPECT_EQ(backward, r);
}

// -----------------------------------------------------------------------------
// Ordering, Hashing, Formatting
// -----------------------------------------------------------------------------
TEST(TimeRangeTest, LexicographicalOrdering) {
	const TimeRange r1 { Rational { 0 }, Rational { 1 } };
	const TimeRange r2 { Rational { 0 }, Rational { 2 } };
	const TimeRange r3 { Rational { 1 }, Rational { 1, 2 } };

	EXPECT_EQ(r1, r1);
	EXPECT_NE(r1, r2);
	EXPECT_LT(r1, r2);
	EXPECT_LT(r2, r3);
	EXPECT_GT(r3, r1);
}

TEST(TimeRangeTest, HashWorksInUnorderedSet) {
	std::unordered_set<TimeRange> set;
	const TimeRange r1 { Rational { 1, 4 }, Rational { 3, 8 } };
	const TimeRange r2 { Rational { 2, 8 }, Rational { 6, 16 } };
	const TimeRange r3 { Rational { 1, 4 }, Rational { 1, 2 } };

	set.insert(r1);
	EXPECT_TRUE(set.contains(r2));
	EXPECT_FALSE(set.contains(r3));
}

TEST(TimeRangeTest, FormattingToStringAndStream) {
	const TimeRange r { Rational { -1, 4 }, Rational { 3, 4 } };
	EXPECT_EQ(r.toString(), "[-1/4, 1/2)");

	std::ostringstream oss;
	oss << r;
	EXPECT_EQ(oss.str(), "[-1/4, 1/2)");

	const TimeRange empty { Rational { 2 }, Rational { 0 } };
	EXPECT_EQ(empty.toString(), "[2, 2)");
}

// -----------------------------------------------------------------------------
// Deterministic Property Tests
// -----------------------------------------------------------------------------
TEST(TimeRangeTest, AlgebraicPropertiesHoldAcrossSamples) {
	// Fixed deterministic LCG so test behavior is identical across toolchains.
	std::uint64_t state = 0xDEADBEEF12345678ULL;
	auto nextInt = [&state](std::int64_t lo, std::int64_t hi) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		const auto span = static_cast<std::uint64_t>(hi - lo + 1);
		return lo + static_cast<std::int64_t>((state >> 32) % span);
	};

	std::vector<TimeRange> samples;
	samples.reserve(100);
	for (int i = 0; i < 100; ++i) {
		const Rational s { nextInt(-16, 16), nextInt(1, 12) };
		const Rational d { nextInt(0, 16), nextInt(1, 12) };
		samples.emplace_back(s, d);
	}

	for (std::size_t i = 0; i < samples.size(); ++i) {
		const auto& a = samples[i];
		EXPECT_TRUE(a.contains(a));
		EXPECT_EQ(a.shift(Rational { 3, 7 }).shift(Rational { -3, 7 }), a);

		for (std::size_t j = 0; j < samples.size(); ++j) {
			const auto& b = samples[j];

			// Symmetry
			EXPECT_EQ(a.overlaps(b), b.overlaps(a));
			EXPECT_EQ(a.intersect(b), b.intersect(a));

			// Overlap / intersection equivalence
			const auto inter = a.intersect(b);
			EXPECT_EQ(a.overlaps(b), inter.has_value());

			if (inter) {
				EXPECT_FALSE(inter->isEmpty());
				EXPECT_TRUE(a.contains(*inter));
				EXPECT_TRUE(b.contains(*inter));
			}
		}
	}
}

} // namespace
} // namespace duey::test
