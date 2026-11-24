
#pragma once

#include "duey/core/NoteEvent.hpp"
#include "duey/core/Rational.hpp"
#include "duey/core/TimeRange.hpp"

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace duey {

/**
 * @brief Sorted, value-semantic, copy-on-write container of NoteEvents.
 *
 * Invariants:
 *  - Events are ordered deterministically by NoteEvent::operator<=>
 *    (start, voice, pitch, duration, velocity, id).
 *  - Every stored NoteEvent is valid (non-negative duration, finite velocity in [0, 1]).
 *  - Every stored NoteEvent has a non-null, Sequence-unique EventId.
 *
 * An auxiliary start-time and prefix-max-end index enables O(log n) contiguous
 * span queries via eventsIn() and fast overlap pruning via eventsOverlapping().
 */
class Sequence {
public:
	using value_type = NoteEvent;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using const_reference = const NoteEvent&;
	using const_pointer = const NoteEvent*;
	using const_iterator = const NoteEvent*;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	// -------------------------------------------------------------------------
	// Construction & Assignment
	// -------------------------------------------------------------------------
	Sequence() noexcept;
	Sequence(const Sequence&) noexcept;
	Sequence(Sequence&&) noexcept;
	Sequence& operator=(const Sequence&) noexcept;
	Sequence& operator=(Sequence&&) noexcept;
	~Sequence();

	explicit Sequence(std::span<const NoteEvent> events);
	explicit Sequence(std::vector<NoteEvent>&& events);
	Sequence(std::initializer_list<NoteEvent> events);

	// -------------------------------------------------------------------------
	// Capacity & Element Access
	// -------------------------------------------------------------------------
	[[nodiscard]] bool empty() const noexcept;
	[[nodiscard]] size_type size() const noexcept;

	[[nodiscard]] const NoteEvent& operator[](size_type index) const noexcept;
	[[nodiscard]] const NoteEvent& at(size_type index) const;
	[[nodiscard]] const NoteEvent& front() const;
	[[nodiscard]] const NoteEvent& back() const;

	[[nodiscard]] std::span<const NoteEvent> events() const noexcept;

	[[nodiscard]] const_iterator begin() const noexcept;
	[[nodiscard]] const_iterator end() const noexcept;
	[[nodiscard]] const_iterator cbegin() const noexcept;
	[[nodiscard]] const_iterator cend() const noexcept;
	[[nodiscard]] const_reverse_iterator rbegin() const noexcept;
	[[nodiscard]] const_reverse_iterator rend() const noexcept;
	[[nodiscard]] const_reverse_iterator crbegin() const noexcept;
	[[nodiscard]] const_reverse_iterator crend() const noexcept;

	// -------------------------------------------------------------------------
	// Timing & Summary Observers
	// -------------------------------------------------------------------------
	/// Returns the earliest event start time, or 0 if empty.
	[[nodiscard]] Rational startTime() const noexcept;

	/// Returns the latest event end time across all events, or 0 if empty.
	[[nodiscard]] Rational endTime() const noexcept;

	/// Returns endTime() - startTime(), or 0 if empty.
	[[nodiscard]] Rational duration() const noexcept;

	/// Returns the enclosing [startTime(), endTime()) range, or std::nullopt if empty.
	[[nodiscard]] std::optional<TimeRange> bounds() const noexcept;

	/// Returns the sorted unique set of voice indices present in the sequence.
	[[nodiscard]] std::vector<std::uint8_t> voices() const;

	// -------------------------------------------------------------------------
	// Range & Identity Queries
	// -------------------------------------------------------------------------
	/// Returns all events whose start() lies in [range.start(), range.end()) in O(log n).
	[[nodiscard]] std::span<const NoteEvent> eventsIn(const TimeRange& range) const noexcept;

	/// Returns all events whose start() equals time in O(log n).
	[[nodiscard]] std::span<const NoteEvent> eventsStartingAt(const Rational& time) const noexcept;

	/// Returns all events whose span overlaps range, in sequence order.
	[[nodiscard]] std::vector<NoteEvent> eventsOverlapping(const TimeRange& range) const;

	/// Returns all events active at point (start <= point < end), in sequence order.
	[[nodiscard]] std::vector<NoteEvent> eventsAt(const Rational& point) const;

	/// Returns all events belonging to the given voice, in sequence order.
	[[nodiscard]] std::vector<NoteEvent> eventsInVoice(std::uint8_t voice) const;

	/// Finds an event by its EventId, or returns nullptr if absent.
	[[nodiscard]] const NoteEvent* findById(EventId id) const noexcept;

	[[nodiscard]] bool containsId(EventId id) const noexcept;
	[[nodiscard]] EventId nextEventId() const noexcept;

	// -------------------------------------------------------------------------
	// Mutators (Copy-on-Write)
	// -------------------------------------------------------------------------
	/// Inserts a single event using binary search, assigning an EventId if unset.
	EventId insert(const NoteEvent& event);

	/// Appends a batch of events, assigns missing EventIds, and sorts once.
	/// Returns the EventIds assigned/preserved in the input span's order.
	std::vector<EventId> insertBulk(std::span<const NoteEvent> events);
	std::vector<EventId> insertBulk(std::initializer_list<NoteEvent> events);

	/// Removes the event with the given EventId; returns true if removed.
	bool removeById(EventId id);

	/// Removes the event at index; throws std::out_of_range if index >= size().
	void eraseAt(size_type index);

	/// Removes all events whose start() lies in [range.start(), range.end()).
	size_type removeIn(const TimeRange& range);

	/// Removes all events matching predicate(const NoteEvent&); returns count removed.
	template <typename Pred>
		requires std::predicate<Pred&, const NoteEvent&>
	size_type removeIf(Pred&& predicate) {
		std::vector<size_type> toRemove;
		const auto evs = events();
		for (size_type i = 0; i < evs.size(); ++i) {
			if (std::invoke(predicate, evs[i])) {
				toRemove.push_back(i);
			}
		}
		if (toRemove.empty()) {
			return 0;
		}
		eraseSortedIndices(toRemove);
		return toRemove.size();
	}

	/// Replaces the event matching id with replacement, preserving id if replacement.id is nullopt.
	/// Returns true if an event with id was found and updated.
	bool updateById(EventId id, const NoteEvent& replacement);

	/// Removes all events from the sequence.
	void clear() noexcept;

	// -------------------------------------------------------------------------
	// Comparison & Formatting
	// -------------------------------------------------------------------------
	/// Compares musical content of all events in order, ignoring EventIds.
	[[nodiscard]] bool hasSameContent(const Sequence& other) const noexcept;

	[[nodiscard]] std::string toString() const;

	[[nodiscard]] bool operator==(const Sequence& rhs) const noexcept;
	[[nodiscard]] std::partial_ordering operator<=>(const Sequence& rhs) const noexcept;

private:
	struct Storage;

	void ensureUnique();
	void eraseSortedIndices(std::span<const size_type> sortedIndices);

	std::shared_ptr<Storage> data_;
};

std::ostream& operator<<(std::ostream& os, const Sequence& sequence);

} // namespace duey

template <>
struct std::hash<duey::Sequence> {
	[[nodiscard]] std::size_t operator()(const duey::Sequence& sequence) const noexcept;
};
