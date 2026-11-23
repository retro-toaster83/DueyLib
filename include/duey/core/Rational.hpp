
#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace duey {

namespace detail {

[[nodiscard]] constexpr std::uint64_t absUnsigned(std::int64_t v) noexcept {
    return v < 0
        ? (std::uint64_t { 0 } - static_cast<std::uint64_t>(v))
        : static_cast<std::uint64_t>(v);
}

struct Uint128 {
    std::uint64_t hi {};
    std::uint64_t lo {};

    constexpr auto operator<=>(const Uint128&) const noexcept = default;
};

[[nodiscard]] constexpr Uint128 mul64(std::uint64_t x, std::uint64_t y) noexcept {
    const std::uint64_t x0 = x & 0xFFFFFFFFULL;
    const std::uint64_t x1 = x >> 32;
    const std::uint64_t y0 = y & 0xFFFFFFFFULL;
    const std::uint64_t y1 = y >> 32;

    const std::uint64_t p00 = x0 * y0;
    const std::uint64_t p01 = x0 * y1;
    const std::uint64_t p10 = x1 * y0;
    const std::uint64_t p11 = x1 * y1;

    const std::uint64_t mid =
        (p00 >> 32) + (p01 & 0xFFFFFFFFULL) + (p10 & 0xFFFFFFFFULL);
    const std::uint64_t lo = (p00 & 0xFFFFFFFFULL) | (mid << 32);
    const std::uint64_t hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    return { hi, lo };
}

[[nodiscard]] constexpr std::int64_t checkedMul(std::int64_t a, std::int64_t b) {
    if (a == 0 || b == 0) {
        return 0;
    }

    const std::uint64_t ua = absUnsigned(a);
    const std::uint64_t ub = absUnsigned(b);
    const bool negative = (a < 0) != (b < 0);

    constexpr auto maxPos = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    constexpr std::uint64_t maxNeg = maxPos + 1ULL;
    const std::uint64_t limit = negative ? maxNeg : maxPos;

    if (ua > limit / ub) {
        throw std::overflow_error("Integer multiplication overflow in Rational");
    }

    const std::uint64_t prod = ua * ub;
    if (negative) {
        if (prod == maxNeg) {
            return std::numeric_limits<std::int64_t>::min();
        }
        return -static_cast<std::int64_t>(prod);
    }
    return static_cast<std::int64_t>(prod);
}

[[nodiscard]] constexpr std::int64_t checkedAdd(std::int64_t a, std::int64_t b) {
    if (b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) {
        throw std::overflow_error("Integer addition overflow in Rational");
    }
    if (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b) {
        throw std::overflow_error("Integer addition overflow in Rational");
    }
    return a + b;
}

[[nodiscard]] constexpr std::int64_t checkedSub(std::int64_t a, std::int64_t b) {
    if (b < 0 && a > std::numeric_limits<std::int64_t>::max() + b) {
        throw std::overflow_error("Integer subtraction overflow in Rational");
    }
    if (b > 0 && a < std::numeric_limits<std::int64_t>::min() + b) {
        throw std::overflow_error("Integer subtraction overflow in Rational");
    }
    return a - b;
}

} // namespace detail

/**
 * @brief Exact rational number for deterministic musical time and durations.
 *
 * Invariants:
 *  - denominator() is always strictly positive (> 0).
 *  - The fraction is always reduced to lowest terms.
 *  - Zero is always represented as 0/1.
 *
 * By musical convention in DueyLib, 1/1 represents a whole note and 1/4 a quarter note.
 */
class Rational {
public:
    constexpr Rational() noexcept = default;

    constexpr Rational(std::int64_t numerator) noexcept
        : num_(numerator), den_(1) {
    }

    constexpr Rational(std::int64_t numerator, std::int64_t denominator) {
        if (denominator == 0) {
            throw std::invalid_argument("Rational denominator cannot be zero");
        }
        if (numerator == 0) {
            num_ = 0;
            den_ = 1;
            return;
        }

        const bool negative = (numerator < 0) != (denominator < 0);
        std::uint64_t absNum = detail::absUnsigned(numerator);
        std::uint64_t absDen = detail::absUnsigned(denominator);

        const std::uint64_t divisor = std::gcd(absNum, absDen);
        absNum /= divisor;
        absDen /= divisor;

        constexpr auto maxPos = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        constexpr std::uint64_t maxNeg = maxPos + 1ULL;

        if (absDen > maxPos) {
            throw std::overflow_error("Rational denominator exceeds int64_t range");
        }
        if (negative) {
            if (absNum > maxNeg) {
                throw std::overflow_error("Rational numerator exceeds int64_t range");
            }
            num_ = (absNum == maxNeg)
                ? std::numeric_limits<std::int64_t>::min()
                : -static_cast<std::int64_t>(absNum);
        } else {
            if (absNum > maxPos) {
                throw std::overflow_error("Rational numerator exceeds int64_t range");
            }
            num_ = static_cast<std::int64_t>(absNum);
        }
        den_ = static_cast<std::int64_t>(absDen);
    }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------
    [[nodiscard]] constexpr std::int64_t numerator() const noexcept { return num_; }
    [[nodiscard]] constexpr std::int64_t denominator() const noexcept { return den_; }

    [[nodiscard]] constexpr bool isZero() const noexcept { return num_ == 0; }
    [[nodiscard]] constexpr bool isPositive() const noexcept { return num_ > 0; }
    [[nodiscard]] constexpr bool isNegative() const noexcept { return num_ < 0; }
    [[nodiscard]] constexpr bool isInteger() const noexcept { return den_ == 1; }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return num_ != 0; }

    // -------------------------------------------------------------------------
    // Conversions & PPQ Helpers
    // -------------------------------------------------------------------------
    [[nodiscard]] constexpr double toDouble() const noexcept {
        return static_cast<double>(num_) / static_cast<double>(den_);
    }

    [[nodiscard]] constexpr float toFloat() const noexcept {
        return static_cast<float>(toDouble());
    }

    /// Converts PPQ (Pulses Per Quarter note) ticks to whole-note Rational time.
    [[nodiscard]] static constexpr Rational fromPpqTicks(std::int64_t ticks, std::int64_t ppq) {
        if (ppq <= 0) {
            throw std::invalid_argument("PPQ must be strictly positive");
        }
        return Rational { ticks, ppq } / Rational { 4 };
    }

    /// Converts whole-note Rational time to PPQ ticks, rounded to nearest (half away from zero).
    [[nodiscard]] constexpr std::int64_t toPpqTicks(std::int64_t ppq) const {
        if (ppq <= 0) {
            throw std::invalid_argument("PPQ must be strictly positive");
        }
        return ((*this * Rational { 4 }) * Rational { ppq }).round();
    }

    /// Converts whole-note Rational time to PPQ ticks only if the tick count is exact.
    [[nodiscard]] constexpr std::optional<std::int64_t> toExactPpqTicks(std::int64_t ppq) const {
        if (ppq <= 0) {
            throw std::invalid_argument("PPQ must be strictly positive");
        }
        const Rational scaled = (*this * Rational { 4 }) * Rational { ppq };
        if (!scaled.isInteger()) {
            return std::nullopt;
        }
        return scaled.numerator();
    }

    /// Approximates a finite floating-point number using continued fractions.
    [[nodiscard]] static Rational fromDouble(
        double value,
        std::int64_t maxDenominator = 1'000'000);

    /// Parses strings of the form "n", "n/d", "-3/4" (with optional outer whitespace).
    [[nodiscard]] static std::optional<Rational> fromString(std::string_view text);

    /// Formats as "n" when denominator == 1, otherwise "n/d".
    [[nodiscard]] std::string toString() const;

    // -------------------------------------------------------------------------
    // Mathematical Operations
    // -------------------------------------------------------------------------
    [[nodiscard]] constexpr Rational abs() const {
        if (num_ >= 0) {
            return *this;
        }
        if (num_ == std::numeric_limits<std::int64_t>::min()) {
            throw std::overflow_error("Cannot negate INT64_MIN in Rational::abs");
        }
        return Rational { -num_, den_, NormalizedTag{} };
    }

    [[nodiscard]] constexpr Rational reciprocal() const {
        if (num_ == 0) {
            throw std::domain_error("Reciprocal of zero is undefined");
        }
        return Rational { den_, num_ };
    }

    [[nodiscard]] constexpr std::int64_t floor() const noexcept {
        const std::int64_t q = num_ / den_;
        const std::int64_t r = num_ % den_;
        return (r < 0) ? (q - 1) : q;
    }

    [[nodiscard]] constexpr std::int64_t ceil() const noexcept {
        const std::int64_t q = num_ / den_;
        const std::int64_t r = num_ % den_;
        return (r > 0) ? (q + 1) : q;
    }

    /// Rounds to nearest integer, with half-way values rounded away from zero.
    [[nodiscard]] constexpr std::int64_t round() const noexcept {
        const std::int64_t q = num_ / den_;
        const std::int64_t r = num_ % den_;
        if (r == 0) {
            return q;
        }
        const std::uint64_t ur = detail::absUnsigned(r);
        const auto uden = static_cast<std::uint64_t>(den_);
        if (ur >= uden - ur) {
            return (num_ >= 0) ? (q + 1) : (q - 1);
        }
        return q;
    }

    /// Returns (*this - floor()), always in the half-open interval [0, 1).
    [[nodiscard]] constexpr Rational fractionalPart() const {
        return *this - Rational { floor() };
    }

    // -------------------------------------------------------------------------
    // Unary & Compound Assignment Operators
    // -------------------------------------------------------------------------
    [[nodiscard]] constexpr Rational operator+() const noexcept { return *this; }

    [[nodiscard]] constexpr Rational operator-() const {
        if (num_ == std::numeric_limits<std::int64_t>::min()) {
            throw std::overflow_error("Cannot negate INT64_MIN in Rational");
        }
        return Rational { -num_, den_, NormalizedTag{} };
    }

    constexpr Rational& operator+=(const Rational& rhs) {
        if (rhs.num_ == 0) {
            return *this;
        }
        if (num_ == 0) {
            *this = rhs;
            return *this;
        }

        const std::int64_t g = std::gcd(den_, rhs.den_);
        const std::int64_t d1 = den_ / g;
        const std::int64_t d2 = rhs.den_ / g;

        const std::int64_t lhsScaled = detail::checkedMul(num_, d2);
        const std::int64_t rhsScaled = detail::checkedMul(rhs.num_, d1);
        const std::int64_t sumNum = detail::checkedAdd(lhsScaled, rhsScaled);

        if (sumNum == 0) {
            num_ = 0;
            den_ = 1;
            return *this;
        }

        const std::uint64_t g2 = std::gcd(detail::absUnsigned(sumNum), static_cast<std::uint64_t>(g));
        const std::int64_t finalNum = sumNum / static_cast<std::int64_t>(g2);
        const std::int64_t finalDen = detail::checkedMul(d1, rhs.den_ / static_cast<std::int64_t>(g2));

        num_ = finalNum;
        den_ = finalDen;
        return *this;
    }

    constexpr Rational& operator-=(const Rational& rhs) {
        if (rhs.num_ == 0) {
            return *this;
        }
        if (num_ == 0) {
            *this = -rhs;
            return *this;
        }

        const std::int64_t g = std::gcd(den_, rhs.den_);
        const std::int64_t d1 = den_ / g;
        const std::int64_t d2 = rhs.den_ / g;

        const std::int64_t lhsScaled = detail::checkedMul(num_, d2);
        const std::int64_t rhsScaled = detail::checkedMul(rhs.num_, d1);
        const std::int64_t diffNum = detail::checkedSub(lhsScaled, rhsScaled);

        if (diffNum == 0) {
            num_ = 0;
            den_ = 1;
            return *this;
        }

        const std::uint64_t g2 = std::gcd(detail::absUnsigned(diffNum), static_cast<std::uint64_t>(g));
        const std::int64_t finalNum = diffNum / static_cast<std::int64_t>(g2);
        const std::int64_t finalDen = detail::checkedMul(d1, rhs.den_ / static_cast<std::int64_t>(g2));

        num_ = finalNum;
        den_ = finalDen;
        return *this;
    }

    constexpr Rational& operator*=(const Rational& rhs) {
        if (num_ == 0 || rhs.num_ == 0) {
            num_ = 0;
            den_ = 1;
            return *this;
        }

        const std::uint64_t g1 = std::gcd(detail::absUnsigned(num_), static_cast<std::uint64_t>(rhs.den_));
        const std::uint64_t g2 = std::gcd(detail::absUnsigned(rhs.num_), static_cast<std::uint64_t>(den_));

        const std::int64_t n1 = num_ / static_cast<std::int64_t>(g1);
        const std::int64_t n2 = rhs.num_ / static_cast<std::int64_t>(g2);
        const std::int64_t d1 = den_ / static_cast<std::int64_t>(g2);
        const std::int64_t d2 = rhs.den_ / static_cast<std::int64_t>(g1);

        num_ = detail::checkedMul(n1, n2);
        den_ = detail::checkedMul(d1, d2);
        return *this;
    }

    constexpr Rational& operator/=(const Rational& rhs) {
        if (rhs.num_ == 0) {
            throw std::domain_error("Division by zero in Rational");
        }
        if (num_ == 0) {
            return *this;
        }

        const std::uint64_t g1 = std::gcd(detail::absUnsigned(num_), detail::absUnsigned(rhs.num_));
        const std::uint64_t g2 = std::gcd(static_cast<std::uint64_t>(den_), static_cast<std::uint64_t>(rhs.den_));

        const bool negative = (num_ < 0) != (rhs.num_ < 0);
        const std::uint64_t an = detail::absUnsigned(num_) / g1;
        const std::uint64_t bn = detail::absUnsigned(rhs.num_) / g1;
        const std::uint64_t ad = static_cast<std::uint64_t>(den_) / g2;
        const std::uint64_t bd = static_cast<std::uint64_t>(rhs.den_) / g2;

        // Result magnitude is (an * bd) / (ad * bn)
        constexpr auto maxPos = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        constexpr std::uint64_t maxNeg = maxPos + 1ULL;

        if (ad > maxPos / bn) {
            throw std::overflow_error("Denominator overflow in Rational division");
        }
        const std::uint64_t newDen = ad * bn;

        const std::uint64_t numLimit = negative ? maxNeg : maxPos;
        if (an > numLimit / bd) {
            throw std::overflow_error("Numerator overflow in Rational division");
        }
        const std::uint64_t newNum = an * bd;

        if (negative) {
            num_ = (newNum == maxNeg)
                ? std::numeric_limits<std::int64_t>::min()
                : -static_cast<std::int64_t>(newNum);
        } else {
            num_ = static_cast<std::int64_t>(newNum);
        }
        den_ = static_cast<std::int64_t>(newDen);
        return *this;
    }

    // -------------------------------------------------------------------------
    // Binary Arithmetic Operators (Hidden Friends)
    // -------------------------------------------------------------------------
    [[nodiscard]] friend constexpr Rational operator+(Rational lhs, const Rational& rhs) {
        lhs += rhs;
        return lhs;
    }

    [[nodiscard]] friend constexpr Rational operator-(Rational lhs, const Rational& rhs) {
        lhs -= rhs;
        return lhs;
    }

    [[nodiscard]] friend constexpr Rational operator*(Rational lhs, const Rational& rhs) {
        lhs *= rhs;
        return lhs;
    }

    [[nodiscard]] friend constexpr Rational operator/(Rational lhs, const Rational& rhs) {
        lhs /= rhs;
        return lhs;
    }

    // -------------------------------------------------------------------------
    // Comparisons
    // -------------------------------------------------------------------------
    [[nodiscard]] constexpr bool operator==(const Rational& rhs) const noexcept = default;

    [[nodiscard]] constexpr std::strong_ordering operator<=>(const Rational& rhs) const noexcept {
        if (num_ == rhs.num_ && den_ == rhs.den_) {
            return std::strong_ordering::equal;
        }
        if ((num_ < 0) != (rhs.num_ < 0)) {
            return num_ < rhs.num_ ? std::strong_ordering::less : std::strong_ordering::greater;
        }
        if (num_ == 0 || rhs.num_ == 0) {
            return num_ <=> rhs.num_;
        }

        const detail::Uint128 lhsCross = detail::mul64(
            detail::absUnsigned(num_),
            static_cast<std::uint64_t>(rhs.den_));
        const detail::Uint128 rhsCross = detail::mul64(
            detail::absUnsigned(rhs.num_),
            static_cast<std::uint64_t>(den_));

        if (num_ > 0) {
            return lhsCross <=> rhsCross;
        }
        return rhsCross <=> lhsCross;
    }

private:
    struct NormalizedTag {};

    constexpr Rational(std::int64_t num, std::int64_t den, NormalizedTag) noexcept
        : num_(num), den_(den) {
    }

    std::int64_t num_ { 0 };
    std::int64_t den_ { 1 };
};

std::ostream& operator<<(std::ostream& os, const Rational& r);

} // namespace duey

template <>
struct std::hash<duey::Rational> {
    [[nodiscard]] std::size_t operator()(const duey::Rational& r) const noexcept {
        const std::size_t h1 = std::hash<std::int64_t> {}(r.numerator());
        const std::size_t h2 = std::hash<std::int64_t> {}(r.denominator());
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};
