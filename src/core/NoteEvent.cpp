
#include "duey/core/NoteEvent.hpp"

#include <locale>
#include <ostream>
#include <sstream>

namespace duey {

std::string NoteEvent::toString() const {
	std::ostringstream oss;
	oss.imbue(std::locale::classic());

	const std::string pitchStr = std::visit(
		[](const auto& p) { return p.toString(); },
		pitch);
	const float canonicalVelocity = (velocity == 0.0f) ? 0.0f : velocity;

	oss << "NoteEvent(span=[" << span.start().toString() << ", " << span.end().toString() << ")"
		<< ", pitch=" << pitchStr
		<< ", velocity=" << canonicalVelocity
		<< ", voice=" << static_cast<unsigned>(voice);
	if (id.has_value()) {
		oss << ", id=" << *id;
	}
	oss << ')';
	return oss.str();
}

std::ostream& operator<<(std::ostream& os, const NoteEvent& event) {
	return os << event.toString();
}

} // namespace duey
