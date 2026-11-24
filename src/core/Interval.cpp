
#include "duey/core/Interval.hpp"

#include <array>
#include <cstdint>
#include <ostream>
#include <string>

namespace duey {

namespace {

constexpr std::array<int, 7> kDiatonicSemitones { 0, 2, 4, 5, 7, 9, 11 };

constexpr bool isPerfectClass(int simpleStep) noexcept {
	return simpleStep == 0 || simpleStep == 3 || simpleStep == 4;
}

} // namespace

std::string Interval::toString() const {
	if (diatonicSteps_ < 0) {
		return "-" + negate().toString();
	}

	const int simpleStep = diatonicSteps_ % 7;
	const int octaves = diatonicSteps_ / 7;
	const std::int64_t expectedSemitones =
		static_cast<std::int64_t>(octaves) * 12 + kDiatonicSemitones[static_cast<std::size_t>(simpleStep)];
	const std::int64_t alteration = static_cast<std::int64_t>(semitones_) - expectedSemitones;
	const std::int64_t degree = static_cast<std::int64_t>(diatonicSteps_) + 1;

	std::string quality;
	if (isPerfectClass(simpleStep)) {
		if (alteration == 0) {
			quality = "P";
		} else if (alteration > 0 && alteration <= 4) {
			quality.assign(static_cast<std::size_t>(alteration), 'A');
		} else if (alteration < 0 && alteration >= -4) {
			quality.assign(static_cast<std::size_t>(-alteration), 'd');
		}
	} else {
		if (alteration == 0) {
			quality = "M";
		} else if (alteration == -1) {
			quality = "m";
		} else if (alteration > 0 && alteration <= 4) {
			quality.assign(static_cast<std::size_t>(alteration), 'A');
		} else if (alteration < -1 && alteration >= -5) {
			quality.assign(static_cast<std::size_t>(-alteration - 1), 'd');
		}
	}

	if (quality.empty()) {
		return "(" + std::to_string(diatonicSteps_) + ", " + std::to_string(semitones_) + ")";
	}

	return quality + std::to_string(degree);
}

std::ostream& operator<<(std::ostream& os, const Interval& interval) {
	return os << interval.toString();
}

} // namespace duey
