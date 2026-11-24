
#include "duey/core/SpelledPitch.hpp"

#include <charconv>
#include <limits>
#include <ostream>
#include <system_error>

namespace duey {

namespace {

[[nodiscard]] constexpr bool isAsciiSpace(char ch) noexcept {
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f';
}

[[nodiscard]] constexpr bool isAsciiDigit(char ch) noexcept {
	return ch >= '0' && ch <= '9';
}

} // namespace

std::optional<SpelledPitch> SpelledPitch::fromString(std::string_view text) {
	while (!text.empty() && isAsciiSpace(text.front())) {
		text.remove_prefix(1);
	}
	while (!text.empty() && isAsciiSpace(text.back())) {
		text.remove_suffix(1);
	}

	if (text.empty()) {
		return std::nullopt;
	}

	const auto letter = letterFromChar(text.front());
	if (!letter) {
		return std::nullopt;
	}
	text.remove_prefix(1);

	int accidentalOffset = 0;
	if (!text.empty()) {
		if (text.front() == '#') {
			while (!text.empty() && text.front() == '#') {
				if (accidentalOffset == std::numeric_limits<std::int8_t>::max()) {
					return std::nullopt;
				}
				++accidentalOffset;
				text.remove_prefix(1);
			}
		} else if (text.front() == 'b') {
			while (!text.empty() && text.front() == 'b') {
				if (accidentalOffset == std::numeric_limits<std::int8_t>::min()) {
					return std::nullopt;
				}
				--accidentalOffset;
				text.remove_prefix(1);
			}
		} else if (text.front() == 'x') {
			accidentalOffset = 2;
			text.remove_prefix(1);
		}
	}

	if (text.empty()) {
		return std::nullopt;
	}

	// std::from_chars does not accept a leading '+' sign.
	if (text.front() == '+') {
		text.remove_prefix(1);
		if (text.empty() || !isAsciiDigit(text.front())) {
			return std::nullopt;
		}
	}

	int octave = 0;
	const char* const begin = text.data();
	const char* const end = begin + text.size();
	const auto [ptr, ec] = std::from_chars(begin, end, octave);
	if (ec != std::errc {} || ptr != end) {
		return std::nullopt;
	}

	return SpelledPitch { *letter, static_cast<Accidental>(accidentalOffset), octave };
}

std::string SpelledPitch::toString() const {
	std::string out;
	out.push_back(toChar(letter_));

	const int offset = accidentalSemitones(accidental_);
	if (offset > 0) {
		out.append(static_cast<std::size_t>(offset), '#');
	} else if (offset < 0) {
		out.append(static_cast<std::size_t>(-offset), 'b');
	}

	out.append(std::to_string(octave_));
	return out;
}

std::ostream& operator<<(std::ostream& os, const SpelledPitch& pitch) {
	os << toChar(pitch.letter());
	const int offset = accidentalSemitones(pitch.accidental());
	for (int i = 0; i < offset; ++i) {
		os << '#';
	}
	for (int i = 0; i < -offset; ++i) {
		os << 'b';
	}
	return os << pitch.octave();
}

} // namespace duey
