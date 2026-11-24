
#pragma once

#include "duey/core/Rational.hpp"

#include <compare>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>

namespace duey {

/**
 * @brief Exact half-open symbolic time interval [start, start + duration).
 *
 * Invariants:
 *  - duration() is always non-negative (>= 0).
 */
class TimeRange {
public:
	constexpr TimeRange() noexcept = default;

	constexpr TimeRange(Rational start, Rational duration)
		: start_(start), duration_(duration) {
		if (duration_.isNegative()) {
			throw std::invalid_argument("TimeRange duration cannot be negative");
		}
	}

	/// Constructs a TimeRange from half-open endpoints [start, end).
	[[nodiscard]] static constexpr TimeRange fromEndpoints(Rational start, Rational end) {
		if (end < start) {
			throw std::invalid_argument("TimeRange end cannot precede start");
		}
		return TimeRange { start, end - start };
	}

	// -------------------------------------------------------------------------
	// Observers
	// -------------------------------------------------------------------------
	[[nodiscard]] constexpr Rational start() const noexcept { return start_; }
	[[nodiscard]] constexpr Rational duration() const noexcept { return duration_; }
	[[nodiscard]] constexpr Rational end() const { return start_ + duration_; }

	[[nodiscard]] constexpr bool isEmpty() const noexcept { return duration_.isZero(); }

	// -------------------------------------------------------------------------
	// Interval Queries & Transformations
	// -------------------------------------------------------------------------

	/// Returns true iff point lies in [start, end). Empty ranges contain no points.
	[[nodiscard]] constexpr bool contains(const Rational& point) const {
		if (isEmpty()) {
			return false;
		}
		return point >= start_ && point < end();
	}

	/// Returns true iff [other.start, other.end) is enclosed within [start, end).
	[[nodiscard]] constexpr bool contains(const TimeRange& other) const {
		return start_ <= other.start_ && other.end() <= end();
	}

	/// Returns true iff the two intervals have a non-empty intersection.
	[[nodiscard]] constexpr bool overlaps(const TimeRange& other) const {
		if (isEmpty() || other.isEmpty()) {
			return false;
		}
		return start_ < other.end() && other.start_ < end();
	}

	/// Returns the non-empty overlap of two intervals, or std::nullopt if disjoint/empty.
	[[nodiscard]] constexpr std::optional<TimeRange> intersect(const TimeRange& other) const {
		if (isEmpty() || other.isEmpty()) {
			return std::nullopt;
		}
		const Rational s = (start_ > other.start_) ? start_ : other.start_;
		const Rational thisEnd = end();
		const Rational otherEnd = other.end();
		const Rational e = (thisEnd < otherEnd) ? thisEnd : otherEnd;
		if (s >= e) {
			return std::nullopt;
		}
		return TimeRange { s, e - s };
	}

	/// Returns a new TimeRange translated by delta, preserving duration.
	[[nodiscard]] constexpr TimeRange shift(const Rational& delta) const {
		return TimeRange { start_ + delta, duration_ };
	}

	// -------------------------------------------------------------------------
	// Formatting & Comparisons
	// -------------------------------------------------------------------------
	[[nodiscard]] std::string toString() const;

	[[nodiscard]] constexpr bool operator==(const TimeRange& rhs) const noexcept = default;
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const TimeRange& rhs) const noexcept = default;

private:
	Rational start_ { 0 };
	Rational duration_ { 0 };
};

std::ostream& operator<<(std::ostream& os, const TimeRange& range);

} // namespace duey

template <>
struct std::hash<duey::TimeRange> {
	[[nodiscard]] std::size_t operator()(const duey::TimeRange& range) const noexcept {
		const std::size_t h1 = std::hash<duey::Rational> {}(range.start());
		const std::size_t h2 = std::hash<duey::Rational> {}(range.duration());
		return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
	}
};
