
#include "duey/core/TimeRange.hpp"

#include <ostream>

namespace duey {

std::string TimeRange::toString() const {
	return "[" + start_.toString() + ", " + end().toString() + ")";
}

std::ostream& operator<<(std::ostream& os, const TimeRange& range) {
	return os << '[' << range.start() << ", " << range.end() << ')';
}

} // namespace duey
