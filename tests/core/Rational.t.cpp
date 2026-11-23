
#include <duey/core/rational.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace duey {
namespace {

// -----------------------------------------------------------------------------
// Compile-time (constexpr) verification
// -----------------------------------------------------------------------------
constexpr Rational kZero {};
constexpr Rational kWhole { 1 };
constexpr Rational kQuarter { 1, 4 };
constexpr Rational kTripletEighth { 1, 12 };
constexpr Rational kQuintupletSixteenth { 1, 20 };

static_assert(kZero.numerator() == 0 && kZero.denominator() == 1);
static_assert(kZero.isZero() && kZero.isInteger() && !kZero);
static_assert(kWhole.numerator() == 1 && kWhole.denominator() == 1 && kWhole.isInteger());
static_assert(kQuarter + kQuarter == Rational { 1, 2 });
static_assert(kTripletEighth * 3 == kQuarter);
static_assert(kQuintupletSixteenth * 5 == kQuarter);
static_assert(kQuarter > kTripletEighth);
static_assert(kTripletEighth > kQuintupletSixteenth);
static_assert(Rational { 1, 4 }.toPpqTicks(480) == 480);
static_assert(Rational::fromPpqTicks(160, 480) == kTripletEighth);
static_assert(Rational { 7, 4 }.floor() == 1);
static_assert(Rational { 7, 4 }.ceil() == 2);
static_assert(Rational { 7, 4 }.round() == 2);
static_assert(Rational { -3, 2 }.round() == -2);

// -----------------------------------------------------------------------------
// Construction & Normalization
// -----------------------------------------------------------------------------
TEST(RationalTest, DefaultAndIntegerConstruction) {
    const Rational r0;
    EXPECT_EQ(r0.numerator(), 0);
    EXPECT_EQ(r0.denominator(), 1);
    EXPECT_TRUE(r0.isZero());
    EXPECT_FALSE(r0.isPositive());
    EXPECT_FALSE(r0.isNegative());
    EXPECT_TRUE(r0.isInteger());
    EXPECT_FALSE(static_cast<bool>(r0));

    const Rational r5 { 5 };
    EXPECT_EQ(r5.numerator(), 5);
    EXPECT_EQ(r5.denominator(), 1);
    EXPECT_TRUE(r5.isPositive());
    EXPECT_TRUE(r5.isInteger());
    EXPECT_TRUE(static_cast<bool>(r5));
}

TEST(RationalTest, NormalizationReducesAndFixesSign) {
    const Rational reduced { 12, 16 };
    EXPECT_EQ(reduced.numerator(), 3);
    EXPECT_EQ(reduced.denominator(), 4);

    const Rational negDen { 3, -4 };
    EXPECT_EQ(negDen.numerator(), -3);
    EXPECT_EQ(negDen.denominator(), 4);
    EXPECT_TRUE(negDen.isNegative());

    const Rational bothNeg { -6, -8 };
    EXPECT_EQ(bothNeg.numerator(), 3);
    EXPECT_EQ(bothNeg.denominator(), 4);

    const Rational zeroNegDen { 0, -99 };
    EXPECT_EQ(zeroNegDen.numerator(), 0);
    EXPECT_EQ(zeroNegDen.denominator(), 1);
}

TEST(RationalTest, ZeroDenominatorThrows) {
    EXPECT_THROW((Rational { 1, 0 }), std::invalid_argument);
    EXPECT_THROW((Rational { 0, 0 }), std::invalid_argument);
}

TEST(RationalTest, Int64MinEdgeCases) {
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

    const Rational validMin { kMin, 1 };
    EXPECT_EQ(validMin.numerator(), kMin);
    EXPECT_EQ(validMin.denominator(), 1);

    const Rational reducedMin { kMin, -2 };
    EXPECT_EQ(reducedMin.numerator(), 1LL << 62);
    EXPECT_EQ(reducedMin.denominator(), 1);

    EXPECT_THROW((Rational { 1, kMin }), std::overflow_error);
    EXPECT_THROW((Rational { kMin, -1 }), std::overflow_error);
    EXPECT_THROW((-validMin), std::overflow_error);
    EXPECT_THROW((validMin.abs()), std::overflow_error);
}

// -----------------------------------------------------------------------------
// Arithmetic
// -----------------------------------------------------------------------------
TEST(RationalTest, BasicArithmetic) {
    const Rational a { 1, 4 };
    const Rational b { 1, 12 };

    EXPECT_EQ(a + b, (Rational { 1, 3 }));
    EXPECT_EQ(a - b, (Rational { 1, 6 }));
    EXPECT_EQ(a * b, (Rational { 1, 48 }));
    EXPECT_EQ(a / b, (Rational { 3, 1 }));

    EXPECT_EQ(+a, a);
    EXPECT_EQ(-a, (Rational { -1, 4 }));
}

TEST(RationalTest, MixedWithIntegers) {
    const Rational quarter { 1, 4 };
    EXPECT_EQ(quarter + 1, (Rational { 5, 4 }));
    EXPECT_EQ(1 + quarter, (Rational { 5, 4 }));
    EXPECT_EQ(quarter * 3, (Rational { 3, 4 }));
    EXPECT_EQ(3 * quarter, (Rational { 3, 4 }));
    EXPECT_EQ(1 - quarter, (Rational { 3, 4 }));
    EXPECT_EQ(1 / quarter, (Rational { 4 }));
}

TEST(RationalTest, CompoundAssignmentAndZeroHandling) {
    Rational r { 1, 3 };
    r += Rational { 1, 6 };
    EXPECT_EQ(r, (Rational { 1, 2 }));
    r -= Rational { 1, 2 };
    EXPECT_EQ(r, (Rational { 0 }));

    r = Rational { 3, 5 };
    r *= Rational { 10, 9 };
    EXPECT_EQ(r, (Rational { 2, 3 }));
    r /= Rational { 4, 3 };
    EXPECT_EQ(r, (Rational { 1, 2 }));

    r *= 0;
    EXPECT_EQ(r, (Rational { 0 }));
}

TEST(RationalTest, CrossReductionPreventsUnnecessaryOverflow) {
    constexpr std::int64_t p1 = 1'000'000'007LL;
    constexpr std::int64_t p2 = 1'000'000'009LL;
    constexpr std::int64_t large = 4'000'000'000'000LL; // 4e12, fits in int64_t

    const Rational a { large, p1 };
    const Rational b { p1, large };
    EXPECT_EQ(a * b, (Rational { 1, 1 }));

    const Rational c { p2, large };
    EXPECT_EQ(a * c, (Rational { p2, p1 }));

    const Rational d { large, p2 };
    EXPECT_EQ(a / d, (Rational { p2, p1 }));
}

TEST(RationalTest, DivisionByZeroAndOverflowThrow) {
    const Rational a { 1, 4 };
    const Rational zero { 0, 1 };

    EXPECT_THROW((void) (a / zero), std::domain_error);
    EXPECT_THROW((void) (zero.reciprocal()), std::domain_error);

    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    const Rational big { kMax, 1 };
    EXPECT_THROW((void) (big * Rational { 2, 1 }), std::overflow_error);
    EXPECT_THROW((void) (big + Rational { 1, 1 }), std::overflow_error);
}

// -----------------------------------------------------------------------------
// Comparisons
// -----------------------------------------------------------------------------
TEST(RationalTest, ComparisonsAndLargeCrossProducts) {
    EXPECT_LT((Rational { 1, 12 }), (Rational { 1, 8 }));
    EXPECT_LT((Rational { -1, 4 }), (Rational { -1, 8 }));
    EXPECT_LT((Rational { -1, 100 }), (Rational { 0 }));
    EXPECT_GT((Rational { 1, 100 }), (Rational { 0 }));
    EXPECT_LE((Rational { 2, 4 }), (Rational { 1, 2 }));
    EXPECT_GE((Rational { 2, 4 }), (Rational { 1, 2 }));

    // Cross products (kMax - 1) * (kMax - 2) would overflow 64-bit if multiplied naively
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    const Rational r1 { kMax - 2, kMax };
    const Rational r2 { kMax - 1, kMax };
    const Rational r3 { kMax - 2, kMax - 1 };

    EXPECT_LT(r1, r2);
    EXPECT_LT(r1, r3);
    EXPECT_LT(r3, r2);
    EXPECT_GT(-r1, -r2);
}

// -----------------------------------------------------------------------------
// Rounding, Parts, Abs, Reciprocal
// -----------------------------------------------------------------------------
TEST(RationalTest, RoundingAndFractionalPart) {
    EXPECT_EQ((Rational { 7, 4 }).floor(), 1);
    EXPECT_EQ((Rational { 7, 4 }).ceil(), 2);
    EXPECT_EQ((Rational { 7, 4 }).round(), 2);
    EXPECT_EQ((Rational { 7, 4 }).fractionalPart(), (Rational { 3, 4 }));

    EXPECT_EQ((Rational { 5, 4 }).round(), 1);
    EXPECT_EQ((Rational { 3, 2 }).round(), 2);
    EXPECT_EQ((Rational { -3, 2 }).round(), -2);
    EXPECT_EQ((Rational { -5, 4 }).round(), -1);

    EXPECT_EQ((Rational { -1, 4 }).floor(), -1);
    EXPECT_EQ((Rational { -1, 4 }).ceil(), 0);
    EXPECT_EQ((Rational { -1, 4 }).fractionalPart(), (Rational { 3, 4 }));

    EXPECT_EQ((Rational { 2, 1 }).floor(), 2);
    EXPECT_EQ((Rational { 2, 1 }).ceil(), 2);
    EXPECT_EQ((Rational { 2, 1 }).fractionalPart(), (Rational { 0 }));

    EXPECT_EQ((Rational { -3, 4 }).abs(), (Rational { 3, 4 }));
    EXPECT_EQ((Rational { -3, 4 }).reciprocal(), (Rational { -4, 3 }));
}

// -----------------------------------------------------------------------------
// Musical Time: PPQ Ticks & Float Conversions
// -----------------------------------------------------------------------------
TEST(RationalTest, PpqConversions) {
    constexpr std::int64_t ppq = 480;

    EXPECT_EQ((Rational { 1, 4 }).toPpqTicks(ppq), 480);
    EXPECT_EQ((Rational { 1, 8 }).toPpqTicks(ppq), 240);
    EXPECT_EQ((Rational { 1, 12 }).toPpqTicks(ppq), 160);
    EXPECT_EQ((Rational { 1, 20 }).toPpqTicks(ppq), 96);
    EXPECT_EQ((Rational { 1, 1 }).toPpqTicks(ppq), 1920);

    EXPECT_EQ(Rational::fromPpqTicks(480, ppq), (Rational { 1, 4 }));
    EXPECT_EQ(Rational::fromPpqTicks(160, ppq), (Rational { 1, 12 }));
    EXPECT_EQ(Rational::fromPpqTicks(-240, ppq), (Rational { -1, 8 }));

    EXPECT_EQ((Rational { 1, 12 }).toExactPpqTicks(ppq), 160);
    // 1/28 of a whole note at 480 PPQ = 1920/28 = 480/7 (non-integer ticks)
    EXPECT_EQ((Rational { 1, 28 }).toExactPpqTicks(ppq), std::nullopt);
    EXPECT_EQ((Rational { 1, 28 }).toPpqTicks(ppq), 69); // 68.57 -> 69

    EXPECT_THROW((void) Rational::fromPpqTicks(100, 0), std::invalid_argument);
    EXPECT_THROW((void) Rational::fromPpqTicks(100, -480), std::invalid_argument);
    EXPECT_THROW((void) (Rational { 1, 4 }).toPpqTicks(0), std::invalid_argument);
    EXPECT_THROW((void) (Rational { 1, 4 }).toExactPpqTicks(-1), std::invalid_argument);
}

TEST(RationalTest, DoubleConversions) {
    EXPECT_DOUBLE_EQ((Rational { 1, 4 }).toDouble(), 0.25);
    EXPECT_FLOAT_EQ((Rational { 3, 4 }).toFloat(), 0.75f);

    EXPECT_EQ(Rational::fromDouble(0.0), (Rational { 0, 1 }));
    EXPECT_EQ(Rational::fromDouble(0.25), (Rational { 1, 4 }));
    EXPECT_EQ(Rational::fromDouble(-0.375), (Rational { -3, 8 }));
    EXPECT_EQ(Rational::fromDouble(1.0 / 3.0), (Rational { 1, 3 }));
    EXPECT_EQ(Rational::fromDouble(1.0 / 12.0), (Rational { 1, 12 }));
    EXPECT_EQ(Rational::fromDouble(0.314159265, 100), (Rational { 22, 70 })); // 11/35

    EXPECT_THROW((void) Rational::fromDouble(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    EXPECT_THROW((void) Rational::fromDouble(std::numeric_limits<double>::infinity()), std::invalid_argument);
    EXPECT_THROW((void) Rational::fromDouble(0.5, 0), std::invalid_argument);
}

// -----------------------------------------------------------------------------
// Formatting, Parsing & Hashing
// -----------------------------------------------------------------------------
TEST(RationalTest, StringFormattingAndStreamOutput) {
    EXPECT_EQ((Rational { 0 }).toString(), "0");
    EXPECT_EQ((Rational { 4 }).toString(), "4");
    EXPECT_EQ((Rational { -3, 8 }).toString(), "-3/8");

    std::ostringstream oss;
    oss << Rational { 1, 4 } << ", " << Rational { 2 };
    EXPECT_EQ(oss.str(), "1/4, 2");
}

TEST(RationalTest, StringParsing) {
    EXPECT_EQ(Rational::fromString("0"), (Rational { 0 }));
    EXPECT_EQ(Rational::fromString("42"), (Rational { 42 }));
    EXPECT_EQ(Rational::fromString("+7"), (Rational { 7 }));
    EXPECT_EQ(Rational::fromString(" -3 / 12 "), (Rational { -1, 4 }));
    EXPECT_EQ(Rational::fromString("6/-8"), (Rational { -3, 4 }));

    EXPECT_EQ(Rational::fromString(""), std::nullopt);
    EXPECT_EQ(Rational::fromString("   "), std::nullopt);
    EXPECT_EQ(Rational::fromString("/"), std::nullopt);
    EXPECT_EQ(Rational::fromString("1/"), std::nullopt);
    EXPECT_EQ(Rational::fromString("/4"), std::nullopt);
    EXPECT_EQ(Rational::fromString("1/0"), std::nullopt);
    EXPECT_EQ(Rational::fromString("1/2/3"), std::nullopt);
    EXPECT_EQ(Rational::fromString("1.25"), std::nullopt);
    EXPECT_EQ(Rational::fromString("abc"), std::nullopt);
}

TEST(RationalTest, HashSupport) {
    std::unordered_set<Rational> set;
    set.insert(Rational { 1, 4 });
    set.insert(Rational { 2, 8 }); // Same normalized value
    set.insert(Rational { 1, 12 });

    EXPECT_EQ(set.size(), 2U);
    EXPECT_TRUE(set.contains(Rational { 3, 12 }));
}

} // namespace
} // namespace duey
