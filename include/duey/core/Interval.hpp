
#pragma once

#include <compare>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <limits>
#include <stdexcept>
#include <string>

namespace duey {

namespace detail {

[[nodiscard]] constexpr int checkedNegateInt(int a) {
	if (a == std::numeric_limits<int>::min()) {
		throw std::overflow_error("Cannot negate INT_MIN in Interval");
	}
	return -a;
}

[[nodiscard]] constexpr int checkedAddInt(int a, int b) {
	if (b > 0 && a > std::numeric_limits<int>::max() - b) {
		throw std::overflow_error("Integer addition overflow in Interval");
	}
	if (b < 0 && a < std::numeric_limits<int>::min() - b) {
		throw std::overflow_error("Integer addition overflow in Interval");
	}
	return a + b;
}

[[nodiscard]] constexpr int checkedSubInt(int a, int b) {
	if (b < 0 && a > std::numeric_limits<int>::max() + b) {
		throw std::overflow_error("Integer subtraction overflow in Interval");
	}
	if (b > 0 && a < std::numeric_limits<int>::min() + b) {
		throw std::overflow_error("Integer subtraction overflow in Interval");
	}
	return a - b;
}

[[nodiscard]] constexpr int checkedMulInt(int a, int b) {
	if (a == 0 || b == 0) {
		return 0;
	}
	if (a > 0) {
		if (b > 0) {
			if (a > std::numeric_limits<int>::max() / b) {
				throw std::overflow_error("Integer multiplication overflow in Interval");
			}
		} else {
			if (b < std::numeric_limits<int>::min() / a) {
				throw std::overflow_error("Integer multiplication overflow in Interval");
			}
		}
	} else {
		if (b > 0) {
			if (a < std::numeric_limits<int>::min() / b) {
				throw std::overflow_error("Integer multiplication overflow in Interval");
			}
		} else {
			if (a < std::numeric_limits<int>::max() / b) {
				throw std::overflow_error("Integer multiplication overflow in Interval");
			}
		}
	}
	return a * b;
}

} // namespace detail

/**
 * @brief Signed symbolic musical interval preserving enharmonic spelling.
 *
 * Represented as (diatonicSteps, semitones), so a major third is {2, 4} and a
 * diminished fourth is {3, 4}. Negative values denote descending intervals.
 */
class Interval {
public:
	constexpr Interval() noexcept = default;

	constexpr Interval(int diatonicSteps, int semitones) noexcept
		: diatonicSteps_(diatonicSteps), semitones_(semitones) {
	}

	// -------------------------------------------------------------------------
	// Common Named Constructors
	// -------------------------------------------------------------------------
	[[nodiscard]] static constexpr Interval perfectUnison() noexcept { return { 0, 0 }; }
	[[nodiscard]] static constexpr Interval minorSecond() noexcept { return { 1, 1 }; }
	[[nodiscard]] static constexpr Interval majorSecond() noexcept { return { 1, 2 }; }
	[[nodiscard]] static constexpr Interval minorThird() noexcept { return { 2, 3 }; }
	[[nodiscard]] static constexpr Interval majorThird() noexcept { return { 2, 4 }; }
	[[nodiscard]] static constexpr Interval diminishedFourth() noexcept { return { 3, 4 }; }
	[[nodiscard]] static constexpr Interval perfectFourth() noexcept { return { 3, 5 }; }
	[[nodiscard]] static constexpr Interval augmentedFourth() noexcept { return { 3, 6 }; }
	[[nodiscard]] static constexpr Interval diminishedFifth() noexcept { return { 4, 6 }; }
	[[nodiscard]] static constexpr Interval perfectFifth() noexcept { return { 4, 7 }; }
	[[nodiscard]] static constexpr Interval minorSixth() noexcept { return { 5, 8 }; }
	[[nodiscard]] static constexpr Interval majorSixth() noexcept { return { 5, 9 }; }
	[[nodiscard]] static constexpr Interval minorSeventh() noexcept { return { 6, 10 }; }
	[[nodiscard]] static constexpr Interval majorSeventh() noexcept { return { 6, 11 }; }
	[[nodiscard]] static constexpr Interval perfectOctave() noexcept { return { 7, 12 }; }

	// -------------------------------------------------------------------------
	// Observers
	// -------------------------------------------------------------------------
	[[nodiscard]] constexpr int diatonicSteps() const noexcept { return diatonicSteps_; }
	[[nodiscard]] constexpr int semitones() const noexcept { return semitones_; }

	[[nodiscard]] constexpr bool isUnison() const noexcept {
		return diatonicSteps_ == 0 && semitones_ == 0;
	}

	[[nodiscard]] constexpr bool isAscending() const noexcept {
		return diatonicSteps_ > 0 || (diatonicSteps_ == 0 && semitones_ > 0);
	}

	[[nodiscard]] constexpr bool isDescending() const noexcept {
		return diatonicSteps_ < 0 || (diatonicSteps_ == 0 && semitones_ < 0);
	}

	[[nodiscard]] constexpr bool isCompound() const noexcept {
		return diatonicSteps_ > 7 || diatonicSteps_ < -7;
	}

	/// Explicit enharmonic equivalence: true iff semitone spans match (e.g. M3 and d4).
	[[nodiscard]] constexpr bool isEnharmonic(const Interval& other) const noexcept {
		return semitones_ == other.semitones_;
	}

	// -------------------------------------------------------------------------
	// Operations
	// -------------------------------------------------------------------------

	/// Returns the negated (opposite-direction) interval: { -diatonicSteps, -semitones }.
	[[nodiscard]] constexpr Interval negate() const {
		return Interval {
			detail::checkedNegateInt(diatonicSteps_),
			detail::checkedNegateInt(semitones_)
		};
	}

	/// Returns the non-descending magnitude of the interval.
	[[nodiscard]] constexpr Interval abs() const {
		return isDescending() ? negate() : *this;
	}

	/**
	 * @brief Returns the octave-complement inversion of this interval.
	 *
	 * For simple non-negative intervals (0 <= diatonicSteps <= 7), returns
	 * P8 - *this (e.g. M3 {2, 4} -> m6 {5, 8}, d4 {3, 4} -> A5 {4, 8},
	 * P1 {0, 0} -> P8 {7, 12}, P8 {7, 12} -> P1 {0, 0}).
	 * Compound intervals invert against the smallest enclosing octave multiple,
	 * and descending intervals preserve their downward direction.
	 */
	[[nodiscard]] constexpr Interval invert() const {
		if (diatonicSteps_ < 0) {
			return negate().invert().negate();
		}
		if (diatonicSteps_ == 0) {
			return Interval { 7, detail::checkedSubInt(12, semitones_) };
		}
		const int octaves = (diatonicSteps_ - 1) / 7 + 1;
		const int steps = 7 * octaves - diatonicSteps_;
		const int semis = detail::checkedSubInt(detail::checkedMulInt(12, octaves), semitones_);
		return Interval { steps, semis };
	}

	// -------------------------------------------------------------------------
	// Operators
	// -------------------------------------------------------------------------
	[[nodiscard]] constexpr Interval operator+() const noexcept { return *this; }
	[[nodiscard]] constexpr Interval operator-() const { return negate(); }

	constexpr Interval& operator+=(const Interval& rhs) {
		diatonicSteps_ = detail::checkedAddInt(diatonicSteps_, rhs.diatonicSteps_);
		semitones_ = detail::checkedAddInt(semitones_, rhs.semitones_);
		return *this;
	}

	constexpr Interval& operator-=(const Interval& rhs) {
		diatonicSteps_ = detail::checkedSubInt(diatonicSteps_, rhs.diatonicSteps_);
		semitones_ = detail::checkedSubInt(semitones_, rhs.semitones_);
		return *this;
	}

	constexpr Interval& operator*=(int factor) {
		diatonicSteps_ = detail::checkedMulInt(diatonicSteps_, factor);
		semitones_ = detail::checkedMulInt(semitones_, factor);
		return *this;
	}

	[[nodiscard]] friend constexpr Interval operator+(Interval lhs, const Interval& rhs) {
		lhs += rhs;
		return lhs;
	}

	[[nodiscard]] friend constexpr Interval operator-(Interval lhs, const Interval& rhs) {
		lhs -= rhs;
		return lhs;
	}

	[[nodiscard]] friend constexpr Interval operator*(Interval lhs, int factor) {
		lhs *= factor;
		return lhs;
	}

	[[nodiscard]] friend constexpr Interval operator*(int factor, Interval rhs) {
		rhs *= factor;
		return rhs;
	}

	// -------------------------------------------------------------------------
	// Formatting & Comparisons
	// -------------------------------------------------------------------------
	[[nodiscard]] std::string toString() const;

	[[nodiscard]] constexpr bool operator==(const Interval& rhs) const noexcept = default;

	/// Orders primarily by chromatic semitone span, breaking enharmonic ties by diatonic steps.
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const Interval& rhs) const noexcept {
		if (const auto cmp = semitones_ <=> rhs.semitones_; cmp != 0) {
			return cmp;
		}
		return diatonicSteps_ <=> rhs.diatonicSteps_;
	}

private:
	int diatonicSteps_ { 0 };
	int semitones_ { 0 };
};

std::ostream& operator<<(std::ostream& os, const Interval& interval);

} // namespace duey

template <>
struct std::hash<duey::Interval> {
	[[nodiscard]] std::size_t operator()(const duey::Interval& interval) const noexcept {
		const std::size_t h1 = std::hash<int> {}(interval.diatonicSteps());
		const std::size_t h2 = std::hash<int> {}(interval.semitones());
		return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
	}
};
