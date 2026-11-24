
#include "duey/core/Sequence.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace duey {

namespace {

struct EventLess {
	[[nodiscard]] constexpr bool operator()(const NoteEvent& lhs, const NoteEvent& rhs) const noexcept {
		return std::is_lt(lhs <=> rhs);
	}
};

[[nodiscard]] Rational maxRational(const Rational& a, const Rational& b) noexcept {
	return (a > b) ? a : b;
}

} // namespace

struct Sequence::Storage {
	std::vector<NoteEvent> events;
	std::vector<Rational> startTimes;
	std::vector<Rational> prefixMaxEnd;
	EventId nextId { 1 };
	bool nextIdExhausted { false };
};

namespace {

void rebuildIndices(std::vector<NoteEvent>& events,
	std::vector<Rational>& startTimes,
	std::vector<Rational>& prefixMaxEnd) {
	const std::size_t n = events.size();
	startTimes.resize(n);
	prefixMaxEnd.resize(n);
	if (n == 0) {
		return;
	}

	Rational runningMax = events[0].end();
	for (std::size_t i = 0; i < n; ++i) {
		startTimes[i] = events[i].start();
		const Rational endVal = events[i].end();
		if (endVal > runningMax) {
			runningMax = endVal;
		}
		prefixMaxEnd[i] = runningMax;
	}
}

void repairPrefixMaxEndFrom(const std::vector<NoteEvent>& events,
	std::vector<Rational>& prefixMaxEnd,
	std::size_t fromIdx) {
	const std::size_t n = events.size();
	for (std::size_t i = fromIdx; i < n; ++i) {
		const Rational curEnd = events[i].end();
		const Rational newMax = (i == 0)
			? curEnd
			: maxRational(prefixMaxEnd[i - 1], curEnd);
		if (prefixMaxEnd[i] == newMax) {
			break;
		}
		prefixMaxEnd[i] = newMax;
	}
}

[[nodiscard]] std::optional<std::size_t> findIndexById(
	std::span<const NoteEvent> events,
	EventId id) noexcept {
	for (std::size_t i = 0; i < events.size(); ++i) {
		if (events[i].id == id) {
			return i;
		}
	}
	return std::nullopt;
}

} // namespace

// -----------------------------------------------------------------------------
// Construction & Assignment
// -----------------------------------------------------------------------------
Sequence::Sequence() noexcept = default;
Sequence::Sequence(const Sequence&) noexcept = default;
Sequence::Sequence(Sequence&&) noexcept = default;
Sequence& Sequence::operator=(const Sequence&) noexcept = default;
Sequence& Sequence::operator=(Sequence&&) noexcept = default;
Sequence::~Sequence() = default;

Sequence::Sequence(std::span<const NoteEvent> events) {
	insertBulk(events);
}

Sequence::Sequence(std::vector<NoteEvent>&& events) {
	if (events.empty()) {
		return;
	}

	std::size_t nulloptCount = 0;
	EventId maxExplicitId = 0;
	bool hasExplicitId = false;
	std::vector<EventId> explicitIds;

	for (const auto& ev : events) {
		ev.validate();
		if (ev.id.has_value()) {
			explicitIds.push_back(*ev.id);
			if (!hasExplicitId || *ev.id > maxExplicitId) {
				maxExplicitId = *ev.id;
				hasExplicitId = true;
			}
		} else {
			++nulloptCount;
		}
	}

	if (explicitIds.size() > 1) {
		std::sort(explicitIds.begin(), explicitIds.end());
		if (std::adjacent_find(explicitIds.begin(), explicitIds.end()) != explicitIds.end()) {
			throw std::invalid_argument("Duplicate EventId in Sequence");
		}
	}

	EventId nextId = 1;
	bool exhausted = false;
	if (hasExplicitId) {
		if (maxExplicitId == std::numeric_limits<EventId>::max()) {
			exhausted = true;
		} else if (maxExplicitId >= nextId) {
			nextId = maxExplicitId + 1;
		}
	}

	if (nulloptCount > 0) {
		if (exhausted || (nulloptCount - 1) > (std::numeric_limits<EventId>::max() - nextId)) {
			throw std::overflow_error("EventId space exhausted in Sequence");
		}
	}

	for (auto& ev : events) {
		if (ev.velocity == 0.0f) {
			ev.velocity = 0.0f;
		}
		if (!ev.id.has_value()) {
			ev.id = nextId;
			if (nextId == std::numeric_limits<EventId>::max()) {
				exhausted = true;
			} else {
				++nextId;
			}
		}
	}

	std::stable_sort(events.begin(), events.end(), EventLess {});

	auto storage = std::make_shared<Storage>();
	storage->events = std::move(events);
	storage->nextId = nextId;
	storage->nextIdExhausted = exhausted;
	rebuildIndices(storage->events, storage->startTimes, storage->prefixMaxEnd);
	data_ = std::move(storage);
}

Sequence::Sequence(std::initializer_list<NoteEvent> events)
	: Sequence(std::span<const NoteEvent> { events.begin(), events.size() }) {
}

// -----------------------------------------------------------------------------
// Capacity & Element Access
// -----------------------------------------------------------------------------
bool Sequence::empty() const noexcept {
	return !data_ || data_->events.empty();
}

Sequence::size_type Sequence::size() const noexcept {
	return data_ ? data_->events.size() : 0;
}

const NoteEvent& Sequence::operator[](size_type index) const noexcept {
	assert(data_ && index < data_->events.size());
	return data_->events[index];
}

const NoteEvent& Sequence::at(size_type index) const {
	if (!data_ || index >= data_->events.size()) {
		throw std::out_of_range("Sequence index out of range");
	}
	return data_->events[index];
}

const NoteEvent& Sequence::front() const {
	if (!data_ || data_->events.empty()) {
		throw std::out_of_range("Sequence::front() called on empty Sequence");
	}
	return data_->events.front();
}

const NoteEvent& Sequence::back() const {
	if (!data_ || data_->events.empty()) {
		throw std::out_of_range("Sequence::back() called on empty Sequence");
	}
	return data_->events.back();
}

std::span<const NoteEvent> Sequence::events() const noexcept {
	if (!data_) {
		return {};
	}
	return data_->events;
}

Sequence::const_iterator Sequence::begin() const noexcept {
	return data_ ? data_->events.data() : nullptr;
}

Sequence::const_iterator Sequence::end() const noexcept {
	return data_ ? (data_->events.data() + data_->events.size()) : nullptr;
}

Sequence::const_iterator Sequence::cbegin() const noexcept {
	return begin();
}

Sequence::const_iterator Sequence::cend() const noexcept {
	return end();
}

Sequence::const_reverse_iterator Sequence::rbegin() const noexcept {
	return const_reverse_iterator { end() };
}

Sequence::const_reverse_iterator Sequence::rend() const noexcept {
	return const_reverse_iterator { begin() };
}

Sequence::const_reverse_iterator Sequence::crbegin() const noexcept {
	return rbegin();
}

Sequence::const_reverse_iterator Sequence::crend() const noexcept {
	return rend();
}

// -----------------------------------------------------------------------------
// Timing & Summary Observers
// -----------------------------------------------------------------------------
Rational Sequence::startTime() const noexcept {
	if (!data_ || data_->events.empty()) {
		return Rational { 0 };
	}
	return data_->events.front().start();
}

Rational Sequence::endTime() const noexcept {
	if (!data_ || data_->prefixMaxEnd.empty()) {
		return Rational { 0 };
	}
	return data_->prefixMaxEnd.back();
}

Rational Sequence::duration() const noexcept {
	if (!data_ || data_->events.empty()) {
		return Rational { 0 };
	}
	return endTime() - startTime();
}

std::optional<TimeRange> Sequence::bounds() const noexcept {
	if (!data_ || data_->events.empty()) {
		return std::nullopt;
	}
	return TimeRange::fromEndpoints(startTime(), endTime());
}

std::vector<std::uint8_t> Sequence::voices() const {
	std::vector<std::uint8_t> result;
	if (!data_ || data_->events.empty()) {
		return result;
	}

	bool seen[256] {};
	for (const auto& ev : data_->events) {
		seen[ev.voice] = true;
	}
	for (std::size_t v = 0; v < 256; ++v) {
		if (seen[v]) {
			result.push_back(static_cast<std::uint8_t>(v));
		}
	}
	return result;
}

// -----------------------------------------------------------------------------
// Range & Identity Queries
// -----------------------------------------------------------------------------
std::span<const NoteEvent> Sequence::eventsIn(const TimeRange& range) const noexcept {
	if (!data_ || data_->events.empty() || range.isEmpty()) {
		return {};
	}

	const auto& starts = data_->startTimes;
	const auto firstIt = std::lower_bound(starts.begin(), starts.end(), range.start());
	const auto lastIt = std::lower_bound(firstIt, starts.end(), range.end());

	const auto offset = static_cast<std::size_t>(firstIt - starts.begin());
	const auto count = static_cast<std::size_t>(lastIt - firstIt);
	return std::span<const NoteEvent> { data_->events.data() + offset, count };
}

std::span<const NoteEvent> Sequence::eventsStartingAt(const Rational& time) const noexcept {
	if (!data_ || data_->events.empty()) {
		return {};
	}

	const auto& starts = data_->startTimes;
	const auto [firstIt, lastIt] = std::equal_range(starts.begin(), starts.end(), time);

	const auto offset = static_cast<std::size_t>(firstIt - starts.begin());
	const auto count = static_cast<std::size_t>(lastIt - firstIt);
	return std::span<const NoteEvent> { data_->events.data() + offset, count };
}

std::vector<NoteEvent> Sequence::eventsOverlapping(const TimeRange& range) const {
	std::vector<NoteEvent> result;
	if (!data_ || data_->events.empty() || range.isEmpty()) {
		return result;
	}

	const auto& starts = data_->startTimes;
	const auto& maxEnd = data_->prefixMaxEnd;

	const auto upperIt = std::lower_bound(starts.begin(), starts.end(), range.end());
	const auto upperIdx = static_cast<std::size_t>(upperIt - starts.begin());
	if (upperIdx == 0) {
		return result;
	}

	const auto firstIt = std::upper_bound(
		maxEnd.begin(),
		maxEnd.begin() + static_cast<std::ptrdiff_t>(upperIdx),
		range.start());
	const auto firstIdx = static_cast<std::size_t>(firstIt - maxEnd.begin());

	for (std::size_t i = firstIdx; i < upperIdx; ++i) {
		if (data_->events[i].span.overlaps(range)) {
			result.push_back(data_->events[i]);
		}
	}
	return result;
}

std::vector<NoteEvent> Sequence::eventsAt(const Rational& point) const {
	std::vector<NoteEvent> result;
	if (!data_ || data_->events.empty()) {
		return result;
	}

	const auto& starts = data_->startTimes;
	const auto& maxEnd = data_->prefixMaxEnd;

	const auto upperIt = std::upper_bound(starts.begin(), starts.end(), point);
	const auto upperIdx = static_cast<std::size_t>(upperIt - starts.begin());
	if (upperIdx == 0) {
		return result;
	}

	const auto firstIt = std::upper_bound(
		maxEnd.begin(),
		maxEnd.begin() + static_cast<std::ptrdiff_t>(upperIdx),
		point);
	const auto firstIdx = static_cast<std::size_t>(firstIt - maxEnd.begin());

	for (std::size_t i = firstIdx; i < upperIdx; ++i) {
		if (data_->events[i].span.contains(point)) {
			result.push_back(data_->events[i]);
		}
	}
	return result;
}

std::vector<NoteEvent> Sequence::eventsInVoice(std::uint8_t voice) const {
	std::vector<NoteEvent> result;
	if (!data_) {
		return result;
	}
	for (const auto& ev : data_->events) {
		if (ev.voice == voice) {
			result.push_back(ev);
		}
	}
	return result;
}

const NoteEvent* Sequence::findById(EventId id) const noexcept {
	if (!data_) {
		return nullptr;
	}
	const auto idx = findIndexById(data_->events, id);
	return idx.has_value() ? &data_->events[*idx] : nullptr;
}

bool Sequence::containsId(EventId id) const noexcept {
	return findById(id) != nullptr;
}

EventId Sequence::nextEventId() const noexcept {
	return data_ ? data_->nextId : EventId { 1 };
}

// -----------------------------------------------------------------------------
// Mutators
// -----------------------------------------------------------------------------
EventId Sequence::insert(const NoteEvent& event) {
	event.validate();

	if (event.id.has_value()) {
		if (containsId(*event.id)) {
			throw std::invalid_argument("Duplicate EventId in Sequence");
		}
	} else if (data_ && data_->nextIdExhausted) {
		throw std::overflow_error("EventId space exhausted in Sequence");
	}

	ensureUnique();

	NoteEvent toInsert = event;
	if (toInsert.velocity == 0.0f) {
		toInsert.velocity = 0.0f;
	}

	if (toInsert.id.has_value()) {
		const EventId explicitId = *toInsert.id;
		if (!data_->nextIdExhausted) {
			if (explicitId == std::numeric_limits<EventId>::max()) {
				data_->nextIdExhausted = true;
			} else if (explicitId >= data_->nextId) {
				data_->nextId = explicitId + 1;
			}
		}
	} else {
		toInsert.id = data_->nextId;
		if (data_->nextId == std::numeric_limits<EventId>::max()) {
			data_->nextIdExhausted = true;
		} else {
			++data_->nextId;
		}
	}

	const EventId assignedId = *toInsert.id;

	// Reserve all three buffers up front for strong exception safety.
	const std::size_t newCap = data_->events.size() + 1;
	data_->events.reserve(newCap);
	data_->startTimes.reserve(newCap);
	data_->prefixMaxEnd.reserve(newCap);

	const auto [loIt, hiIt] = std::equal_range(
		data_->startTimes.begin(),
		data_->startTimes.end(),
		toInsert.start());
	const auto loIdx = static_cast<std::ptrdiff_t>(loIt - data_->startTimes.begin());
	const auto hiIdx = static_cast<std::ptrdiff_t>(hiIt - data_->startTimes.begin());

	const auto posIt = std::lower_bound(
		data_->events.begin() + loIdx,
		data_->events.begin() + hiIdx,
		toInsert,
		EventLess {});
	const auto insertIdx = static_cast<std::size_t>(posIt - data_->events.begin());
	const auto insertOffset = static_cast<std::ptrdiff_t>(insertIdx);

	const Rational insertedStart = toInsert.start();
	const Rational insertedEnd = toInsert.end();
	const Rational newPrefixMax = (insertIdx == 0)
		? insertedEnd
		: maxRational(data_->prefixMaxEnd[insertIdx - 1], insertedEnd);

	data_->events.insert(posIt, std::move(toInsert));
	data_->startTimes.insert(data_->startTimes.begin() + insertOffset, insertedStart);
	data_->prefixMaxEnd.insert(data_->prefixMaxEnd.begin() + insertOffset, newPrefixMax);
	repairPrefixMaxEndFrom(data_->events, data_->prefixMaxEnd, insertIdx + 1);

	return assignedId;
}

std::vector<EventId> Sequence::insertBulk(std::span<const NoteEvent> incoming) {
	std::vector<EventId> assignedIds;
	if (incoming.empty()) {
		return assignedIds;
	}

	std::size_t nulloptCount = 0;
	EventId maxExplicitId = 0;
	bool hasExplicitId = false;
	std::vector<EventId> explicitIds;

	for (const auto& ev : incoming) {
		ev.validate();
		if (ev.id.has_value()) {
			explicitIds.push_back(*ev.id);
			if (!hasExplicitId || *ev.id > maxExplicitId) {
				maxExplicitId = *ev.id;
				hasExplicitId = true;
			}
		} else {
			++nulloptCount;
		}
	}

	if (explicitIds.size() > 1) {
		std::sort(explicitIds.begin(), explicitIds.end());
		if (std::adjacent_find(explicitIds.begin(), explicitIds.end()) != explicitIds.end()) {
			throw std::invalid_argument("Duplicate EventId in bulk insert batch");
		}
	}

	if (!explicitIds.empty() && data_ && !data_->events.empty()) {
		if (explicitIds.size() == 1) {
			if (containsId(explicitIds.front())) {
				throw std::invalid_argument("Duplicate EventId in Sequence");
			}
		} else {
			std::unordered_set<EventId> existingIds;
			existingIds.reserve(data_->events.size());
			for (const auto& ev : data_->events) {
				if (ev.id.has_value()) {
					existingIds.insert(*ev.id);
				}
			}
			for (const EventId id : explicitIds) {
				if (existingIds.contains(id)) {
					throw std::invalid_argument("Duplicate EventId in Sequence");
				}
			}
		}
	}

	EventId candidateNextId = data_ ? data_->nextId : EventId { 1 };
	bool candidateExhausted = data_ ? data_->nextIdExhausted : false;

	if (hasExplicitId && !candidateExhausted) {
		if (maxExplicitId == std::numeric_limits<EventId>::max()) {
			candidateExhausted = true;
		} else if (maxExplicitId >= candidateNextId) {
			candidateNextId = maxExplicitId + 1;
		}
	}

	if (nulloptCount > 0) {
		if (candidateExhausted ||
			(nulloptCount - 1) > (std::numeric_limits<EventId>::max() - candidateNextId)) {
			throw std::overflow_error("EventId space exhausted in Sequence");
		}
	}

	assignedIds.reserve(incoming.size());
	std::vector<NoteEvent> prepared;
	prepared.reserve(incoming.size());

	for (const auto& ev : incoming) {
		NoteEvent copy = ev;
		if (copy.velocity == 0.0f) {
			copy.velocity = 0.0f;
		}
		if (copy.id.has_value()) {
			assignedIds.push_back(*copy.id);
		} else {
			const EventId id = candidateNextId;
			if (candidateNextId == std::numeric_limits<EventId>::max()) {
				candidateExhausted = true;
			} else {
				++candidateNextId;
			}
			copy.id = id;
			assignedIds.push_back(id);
		}
		prepared.push_back(std::move(copy));
	}

	ensureUnique();

	const std::size_t totalSize = data_->events.size() + prepared.size();
	data_->events.reserve(totalSize);
	data_->startTimes.reserve(totalSize);
	data_->prefixMaxEnd.reserve(totalSize);

	data_->events.insert(
		data_->events.end(),
		std::make_move_iterator(prepared.begin()),
		std::make_move_iterator(prepared.end()));
	data_->nextId = candidateNextId;
	data_->nextIdExhausted = candidateExhausted;

	std::stable_sort(data_->events.begin(), data_->events.end(), EventLess {});
	rebuildIndices(data_->events, data_->startTimes, data_->prefixMaxEnd);

	return assignedIds;
}

std::vector<EventId> Sequence::insertBulk(std::initializer_list<NoteEvent> events) {
	return insertBulk(std::span<const NoteEvent> { events.begin(), events.size() });
}

bool Sequence::removeById(EventId id) {
	if (!data_) {
		return false;
	}
	const auto idx = findIndexById(data_->events, id);
	if (!idx.has_value()) {
		return false;
	}
	eraseAt(*idx);
	return true;
}

void Sequence::eraseAt(size_type index) {
	if (!data_ || index >= data_->events.size()) {
		throw std::out_of_range("Sequence::eraseAt index out of range");
	}

	ensureUnique();

	const auto offset = static_cast<std::ptrdiff_t>(index);
	data_->events.erase(data_->events.begin() + offset);
	data_->startTimes.erase(data_->startTimes.begin() + offset);
	data_->prefixMaxEnd.erase(data_->prefixMaxEnd.begin() + offset);
	repairPrefixMaxEndFrom(data_->events, data_->prefixMaxEnd, index);
}

Sequence::size_type Sequence::removeIn(const TimeRange& range) {
	if (!data_ || data_->events.empty() || range.isEmpty()) {
		return 0;
	}

	const auto& starts = data_->startTimes;
	const auto firstIt = std::lower_bound(starts.begin(), starts.end(), range.start());
	const auto lastIt = std::lower_bound(firstIt, starts.end(), range.end());
	const auto firstIdx = static_cast<std::size_t>(firstIt - starts.begin());
	const auto lastIdx = static_cast<std::size_t>(lastIt - starts.begin());

	if (firstIdx == lastIdx) {
		return 0;
	}

	ensureUnique();

	const auto firstOff = static_cast<std::ptrdiff_t>(firstIdx);
	const auto lastOff = static_cast<std::ptrdiff_t>(lastIdx);

	data_->events.erase(data_->events.begin() + firstOff, data_->events.begin() + lastOff);
	data_->startTimes.erase(data_->startTimes.begin() + firstOff, data_->startTimes.begin() + lastOff);
	data_->prefixMaxEnd.erase(data_->prefixMaxEnd.begin() + firstOff, data_->prefixMaxEnd.begin() + lastOff);
	repairPrefixMaxEndFrom(data_->events, data_->prefixMaxEnd, firstIdx);

	return lastIdx - firstIdx;
}

bool Sequence::updateById(EventId id, const NoteEvent& replacement) {
	replacement.validate();

	if (!data_) {
		return false;
	}

	const auto idxOpt = findIndexById(data_->events, id);
	if (!idxOpt.has_value()) {
		return false;
	}
	const std::size_t oldIdx = *idxOpt;

	const EventId targetId = replacement.id.value_or(id);
	if (targetId != id && containsId(targetId)) {
		throw std::invalid_argument("Duplicate EventId in Sequence::updateById");
	}

	ensureUnique();

	NoteEvent updated = replacement;
	if (updated.velocity == 0.0f) {
		updated.velocity = 0.0f;
	}
	updated.id = targetId;

	if (!data_->nextIdExhausted) {
		if (targetId == std::numeric_limits<EventId>::max()) {
			data_->nextIdExhausted = true;
		} else if (targetId >= data_->nextId) {
			data_->nextId = targetId + 1;
		}
	}

	// Remove from old position and re-insert at its sorted position.
	const auto oldOff = static_cast<std::ptrdiff_t>(oldIdx);
	data_->events.erase(data_->events.begin() + oldOff);
	data_->startTimes.erase(data_->startTimes.begin() + oldOff);
	data_->prefixMaxEnd.erase(data_->prefixMaxEnd.begin() + oldOff);
	repairPrefixMaxEndFrom(data_->events, data_->prefixMaxEnd, oldIdx);

	const auto [loIt, hiIt] = std::equal_range(
		data_->startTimes.begin(),
		data_->startTimes.end(),
		updated.start());
	const auto loIdx = static_cast<std::ptrdiff_t>(loIt - data_->startTimes.begin());
	const auto hiIdx = static_cast<std::ptrdiff_t>(hiIt - data_->startTimes.begin());

	const auto posIt = std::lower_bound(
		data_->events.begin() + loIdx,
		data_->events.begin() + hiIdx,
		updated,
		EventLess {});
	const auto insertIdx = static_cast<std::size_t>(posIt - data_->events.begin());
	const auto insertOff = static_cast<std::ptrdiff_t>(insertIdx);

	const Rational updatedStart = updated.start();
	const Rational updatedEnd = updated.end();
	const Rational newPrefixMax = (insertIdx == 0)
		? updatedEnd
		: maxRational(data_->prefixMaxEnd[insertIdx - 1], updatedEnd);

	data_->events.insert(posIt, std::move(updated));
	data_->startTimes.insert(data_->startTimes.begin() + insertOff, updatedStart);
	data_->prefixMaxEnd.insert(data_->prefixMaxEnd.begin() + insertOff, newPrefixMax);
	repairPrefixMaxEndFrom(data_->events, data_->prefixMaxEnd, insertIdx + 1);

	return true;
}

void Sequence::clear() noexcept {
	if (!data_) {
		return;
	}
	if (data_.use_count() == 1) {
		data_->events.clear();
		data_->startTimes.clear();
		data_->prefixMaxEnd.clear();
	} else {
		data_.reset();
	}
}

// -----------------------------------------------------------------------------
// Comparison & Formatting
// -----------------------------------------------------------------------------
bool Sequence::hasSameContent(const Sequence& other) const noexcept {
	const auto lhs = events();
	const auto rhs = other.events();
	if (lhs.size() != rhs.size()) {
		return false;
	}
	for (std::size_t i = 0; i < lhs.size(); ++i) {
		if (!lhs[i].hasSameContent(rhs[i])) {
			return false;
		}
	}
	return true;
}

std::string Sequence::toString() const {
	std::ostringstream oss;
	oss << *this;
	return oss.str();
}

bool Sequence::operator==(const Sequence& rhs) const noexcept {
	const auto lhsEv = events();
	const auto rhsEv = rhs.events();
	return std::equal(lhsEv.begin(), lhsEv.end(), rhsEv.begin(), rhsEv.end());
}

std::partial_ordering Sequence::operator<=>(const Sequence& rhs) const noexcept {
	const auto lhsEv = events();
	const auto rhsEv = rhs.events();
	return std::lexicographical_compare_three_way(
		lhsEv.begin(), lhsEv.end(),
		rhsEv.begin(), rhsEv.end());
}

std::ostream& operator<<(std::ostream& os, const Sequence& sequence) {
	os << "Sequence(" << sequence.size() << " events";
	if (const auto b = sequence.bounds(); b.has_value()) {
		os << ", [" << b->start() << ", " << b->end() << ")";
	}
	os << ')';
	return os;
}

// -----------------------------------------------------------------------------
// Private Helpers
// -----------------------------------------------------------------------------
void Sequence::ensureUnique() {
	if (!data_) {
		data_ = std::make_shared<Storage>();
	} else if (data_.use_count() > 1) {
		data_ = std::make_shared<Storage>(*data_);
	}
}

void Sequence::eraseSortedIndices(std::span<const size_type> sortedIndices) {
	if (sortedIndices.empty() || !data_) {
		return;
	}

	ensureUnique();

	std::size_t writeIdx = 0;
	std::size_t skipCursor = 0;
	const std::size_t n = data_->events.size();

	for (std::size_t readIdx = 0; readIdx < n; ++readIdx) {
		if (skipCursor < sortedIndices.size() && sortedIndices[skipCursor] == readIdx) {
			++skipCursor;
			continue;
		}
		if (writeIdx != readIdx) {
			data_->events[writeIdx] = std::move(data_->events[readIdx]);
		}
		++writeIdx;
	}

	data_->events.resize(writeIdx);
	rebuildIndices(data_->events, data_->startTimes, data_->prefixMaxEnd);
}

} // namespace duey

std::size_t std::hash<duey::Sequence>::operator()(const duey::Sequence& sequence) const noexcept {
	std::size_t seed = std::hash<std::size_t> {}(sequence.size());
	const std::hash<duey::NoteEvent> eventHasher {};
	for (const auto& ev : sequence.events()) {
		const std::size_t h = eventHasher(ev);
		seed ^= h + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
	}
	return seed;
}
