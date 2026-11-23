
#include <duey/core/rational.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <ostream>
#include <string>

namespace duey {

namespace {

constexpr std::string_view trimAsciiWhitespace(std::string_view sv) noexcept {
    constexpr std::string_view whitespace = " \t\n\r\f\v";
    const std::size_t first = sv.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = sv.find_last_not_of(whitespace);
    return sv.substr(first, last - first + 1);
}

std::optional<std::int64_t> parseSignedInt64(std::string_view sv) noexcept {
    if (sv.empty()) {
        return std::nullopt;
    }
    if (sv.front() == '+') {
        sv.remove_prefix(1);
        if (sv.empty() || sv.front() == '-' || sv.front() == '+') {
            return std::nullopt;
        }
    }

    std::int64_t value = 0;
    const char* begin = sv.data();
    const char* end = sv.data() + sv.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc {} || ptr != end) {
        return std::nullopt;
    }
    return value;
}

} // namespace

Rational Rational::fromDouble(double value, std::int64_t maxDenominator) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Rational::fromDouble requires a finite value");
    }
    if (maxDenominator <= 0) {
        throw std::invalid_argument("Rational::fromDouble requires maxDenominator > 0");
    }
    if (value == 0.0) {
        return Rational { 0, 1 };
    }

    const bool negative = value < 0.0;
    const double x = std::abs(value);

    constexpr double maxInt64AsDouble =
        static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if (x >= maxInt64AsDouble) {
        throw std::overflow_error("Value out of Rational range in fromDouble");
    }

    std::int64_t h0 = 0;
    std::int64_t k0 = 1;
    std::int64_t h1 = 1;
    std::int64_t k1 = 0;

    double rem = x;
    constexpr int maxIterations = 64;
    constexpr std::int64_t maxNum = std::numeric_limits<std::int64_t>::max();

    for (int iter = 0; iter < maxIterations; ++iter) {
        const double floorVal = std::floor(rem);
        if (floorVal >= maxInt64AsDouble) {
            break;
        }

        const auto a = static_cast<std::int64_t>(floorVal);
        const bool numOverflow = (a > 0) && (h1 > (maxNum - h0) / a);
        const bool denOverflow = (a > 0) && (k1 > (maxDenominator - k0) / a);

        if (numOverflow || denOverflow) {
            if (k1 == 0) {
                throw std::overflow_error("Value out of Rational range in fromDouble");
            }
            // Check the best semiconvergent within maxDenominator
            const std::int64_t n = (maxDenominator - k0) / k1;
            if (n > 0 && h1 <= (maxNum - h0) / n) {
                const std::int64_t hSemi = h0 + n * h1;
                const std::int64_t kSemi = k0 + n * k1;
                const double errCurr = std::abs(x - static_cast<double>(h1) / static_cast<double>(k1));
                const double errSemi = std::abs(x - static_cast<double>(hSemi) / static_cast<double>(kSemi));
                if (errSemi < errCurr) {
                    h1 = hSemi;
                    k1 = kSemi;
                }
            }
            break;
        }

        const std::int64_t h2 = a * h1 + h0;
        const std::int64_t k2 = a * k1 + k0;
        h0 = h1;
        k0 = k1;
        h1 = h2;
        k1 = k2;

        const double approx = static_cast<double>(h1) / static_cast<double>(k1);
        const double frac = rem - floorVal;
        if (frac == 0.0 || std::abs(x - approx) <= 1e-15 * std::max(1.0, x)) {
            break;
        }
        rem = 1.0 / frac;
    }

    return negative ? Rational { -h1, k1 } : Rational { h1, k1 };
}

std::optional<Rational> Rational::fromString(std::string_view text) {
    const std::string_view trimmed = trimAsciiWhitespace(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    const std::size_t slashPos = trimmed.find('/');
    if (slashPos == std::string_view::npos) {
        const auto whole = parseSignedInt64(trimmed);
        if (!whole) {
            return std::nullopt;
        }
        return Rational { *whole };
    }

    // Reject multiple slashes
    if (trimmed.find('/', slashPos + 1) != std::string_view::npos) {
        return std::nullopt;
    }

    const std::string_view numPart = trimAsciiWhitespace(trimmed.substr(0, slashPos));
    const std::string_view denPart = trimAsciiWhitespace(trimmed.substr(slashPos + 1));

    const auto num = parseSignedInt64(numPart);
    const auto den = parseSignedInt64(denPart);
    if (!num || !den || *den == 0) {
        return std::nullopt;
    }

    try {
        return Rational { *num, *den };
    } catch (const std::overflow_error&) {
        return std::nullopt;
    }
}

std::string Rational::toString() const {
    if (den_ == 1) {
        return std::to_string(num_);
    }
    return std::to_string(num_) + '/' + std::to_string(den_);
}

std::ostream& operator<<(std::ostream& os, const Rational& r) {
    os << r.numerator();
    if (r.denominator() != 1) {
        os << '/' << r.denominator();
    }
    return os;
}

} // namespace duey
