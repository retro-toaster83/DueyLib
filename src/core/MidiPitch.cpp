
#include "duey/core/MidiPitch.hpp"

#include <locale>
#include <ostream>
#include <sstream>

namespace duey {

std::string MidiPitch::toString() const {
	std::ostringstream oss;
	oss.imbue(std::locale::classic());
	oss << static_cast<int>(noteNumber_);
	if (centOffset_ > 0.0f) {
		oss << '+' << centOffset_ << 'c';
	} else if (centOffset_ < 0.0f) {
		oss << centOffset_ << 'c';
	}
	return oss.str();
}

std::ostream& operator<<(std::ostream& os, const MidiPitch& pitch) {
	return os << pitch.toString();
}

} // namespace duey
