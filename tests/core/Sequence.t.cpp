
#include "duey/core/Sequence.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace duey::test {
namespace {

NoteEvent makeSpelledNote(
	Rational start,
	Rational duration,
	Letter letter,
	Accidental acc = Accidental::Natural,
	int octave = 4,
	std::uint8_t voice = 0,
	float velocity = 0.8f,
	std::optional<EventId> id = std::nullopt) {
	return NoteEvent {
		.span = TimeRange { start, duration },
		.pitch = SpelledPitch { letter, acc, octave },
		.velocity = velocity,
		.voice = voice,
		.id = id
	};
}

NoteEvent makeMidiNote(
	Rational start,
	Rational duration,
	int noteNumber,
	float centOffset = 0.0f,
	std::uint8_t voice = 0,
	float velocity = 0.8f,
	std::optional<EventId> id = std::nullopt) {
	return NoteEvent {
		.span = TimeRange { start, duration },
		.pitch = MidiPitch { noteNumber, centOffset },
		.velocity = velocity,
		.voice = voice,
		.id = id
	};
}

// Deterministic SplitMix64 PRNG for compiler-independent property tests.
class SplitMix64 {
public:
	explicit SplitMix64(std::uint64_t seed) : state_(seed) {}

	std::uint64_t next() {
		state_ += 0x9e3779b97f4a7c15ULL;
		std::uint64_t z = state_;
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
		return z ^ (z >> 31);
	}

	int nextInt(int lo, int hi) {
		const auto span = static_cast<std::uint64_t>(hi - lo + 1);
		return lo + static_cast<int>(next() % span);
	}

private:
	std::uint64_t state_;
};

// Brute-force oracle helpers to validate indexed Sequence queries.
std::vector<NoteEvent> oracleEventsIn(const Sequence& seq, const TimeRange& range) {
	std::vector<NoteEvent> out;
	for (const auto& ev : seq.events()) {
		if (range.contains(ev.start())) {
			out.push_back(ev);
		}
	}
	return out;
}

std::vector<NoteEvent> oracleEventsStartingAt(const Sequence& seq, const Rational& point) {
	std::vector<NoteEvent> out;
	for (const auto& ev : seq.events()) {
		if (ev.start() == point) {
			out.push_back(ev);
		}
	}
	return out;
}

std::vector<NoteEvent> oracleEventsOverlapping(const Sequence& seq, const TimeRange& range) {
	std::vector<NoteEvent> out;
	for (const auto& ev : seq.events()) {
		if (ev.span.overlaps(range)) {
			out.push_back(ev);
		}
	}
	return out;
}

std::vector<NoteEvent> oracleEventsAt(const Sequence& seq, const Rational& point) {
	std::vector<NoteEvent> out;
	for (const auto& ev : seq.events()) {
		if (ev.span.contains(point)) {
			out.push_back(ev);
		}
	}
	return out;
}

void verifySequenceInvariants(const Sequence& seq) {
	const auto evs = seq.events();
	std::unordered_set<EventId> seenIds;
	for (std::size_t i = 0; i < evs.size(); ++i) {
		EXPECT_TRUE(evs[i].isValid());
		ASSERT_TRUE(evs[i].id.has_value());
		EXPECT_TRUE(seenIds.insert(*evs[i].id).second);
		if (i > 0) {
			EXPECT_FALSE(std::is_gt(evs[i - 1] <=> evs[i]));
		}
	}
}

// -----------------------------------------------------------------------------
// Empty & Basic Accessor Tests
// -----------------------------------------------------------------------------
TEST(SequenceTest, DefaultConstructedIsEmpty) {
	const Sequence seq;
	EXPECT_TRUE(seq.empty());
	EXPECT_EQ(seq.size(), 0u);
	EXPECT_TRUE(seq.events().empty());
	EXPECT_EQ(seq.begin(), seq.end());
	EXPECT_EQ(seq.rbegin(), seq.rend());
	EXPECT_EQ(seq.startTime(), Rational { 0 });
	EXPECT_EQ(seq.endTime(), Rational { 0 });
	EXPECT_EQ(seq.duration(), Rational { 0 });
	EXPECT_FALSE(seq.bounds().has_value());
	EXPECT_TRUE(seq.voices().empty());
	EXPECT_EQ(seq.nextEventId(), 1u);
	EXPECT_EQ(seq.findById(1), nullptr);
	EXPECT_FALSE(seq.containsId(1));

	EXPECT_TRUE(seq.eventsIn(TimeRange { 0, 4 }).empty());
	EXPECT_TRUE(seq.eventsStartingAt(Rational { 0 }).empty());
	EXPECT_TRUE(seq.eventsOverlapping(TimeRange { 0, 4 }).empty());
	EXPECT_TRUE(seq.eventsAt(Rational { 0 }).empty());
	EXPECT_TRUE(seq.eventsInVoice(0).empty());

	EXPECT_THROW((void) seq.at(0), std::out_of_range);
	EXPECT_THROW((void) seq.front(), std::out_of_range);
	EXPECT_THROW((void) seq.back(), std::out_of_range);
}

// -----------------------------------------------------------------------------
// Sorting & Tie-Breaking Tests
// -----------------------------------------------------------------------------
TEST(SequenceTest, MaintainsDeterministicSortedOrder) {
	const NoteEvent eLate = makeSpelledNote({ 1, 2 }, { 1, 4 }, Letter::C);
	const NoteEvent eVoice1 = makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C, Accidental::Natural, 4, 1);
	const NoteEvent eVoice0High = makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::G, Accidental::Natural, 4, 0);
	const NoteEvent eVoice0LowMidi = makeMidiNote({ 0, 1 }, { 1, 4 }, 60, 0.0f, 0);
	const NoteEvent eVoice0LowSpelled = makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C, Accidental::Natural, 4, 0);
	const NoteEvent eEnharmonicSharp = makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::C, Accidental::Sharp, 4, 0);
	const NoteEvent eEnharmonicFlat = makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::D, Accidental::Flat, 4, 0);

	Sequence seq {
		eLate,
		eVoice1,
		eVoice0High,
		eVoice0LowMidi,
		eEnharmonicFlat,
		eVoice0LowSpelled,
		eEnharmonicSharp
	};

	verifySequenceInvariants(seq);
	ASSERT_EQ(seq.size(), 7u);

	// At start=0, voice=0: C4 (Spelled) < C4 (Midi) < G4 (Spelled)
	EXPECT_TRUE(seq[0].hasSameContent(eVoice0LowSpelled));
	EXPECT_TRUE(seq[1].hasSameContent(eVoice0LowMidi));
	EXPECT_TRUE(seq[2].hasSameContent(eVoice0High));
	// At start=0, voice=1
	EXPECT_TRUE(seq[3].hasSameContent(eVoice1));
	// At start=1/4: C#4 < Db4
	EXPECT_TRUE(seq[4].hasSameContent(eEnharmonicSharp));
	EXPECT_TRUE(seq[5].hasSameContent(eEnharmonicFlat));
	// At start=1/2
	EXPECT_TRUE(seq[6].hasSameContent(eLate));

	// Verify front(), back(), and reverse iteration
	EXPECT_EQ(seq.front(), seq[0]);
	EXPECT_EQ(seq.back(), seq[6]);
	std::vector<NoteEvent> rev(seq.rbegin(), seq.rend());
	ASSERT_EQ(rev.size(), 7u);
	EXPECT_EQ(rev.front(), seq.back());
	EXPECT_EQ(rev.back(), seq.front());
}

TEST(SequenceTest, ConstructFromMovedVectorAndSpan) {
	std::vector<NoteEvent> raw {
		makeSpelledNote({ 3, 4 }, { 1, 4 }, Letter::E),
		makeSpelledNote({ 0, 1 }, { 1, 2 }, Letter::C)
	};

	const Sequence fromSpan(std::span<const NoteEvent> { raw });
	const Sequence fromMoved(std::move(raw));

	EXPECT_EQ(fromSpan, fromMoved);
	EXPECT_EQ(fromSpan.size(), 2u);
	EXPECT_EQ(fromSpan[0].start(), Rational { 0 });
	EXPECT_EQ(fromSpan[1].start(), (Rational { 3, 4 }));

	const Sequence emptyMoved(std::vector<NoteEvent> {});
	EXPECT_TRUE(emptyMoved.empty());
}

// -----------------------------------------------------------------------------
// Validation & Strong Exception Safety Tests
// -----------------------------------------------------------------------------
TEST(SequenceTest, RejectsInvalidVelocityWithStrongExceptionSafety) {
	Sequence seq;
	seq.insert(makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C));

	NoteEvent badVelocityHigh = makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::D);
	badVelocityHigh.velocity = 1.2f;
	EXPECT_THROW((void) seq.insert(badVelocityHigh), std::out_of_range);

	NoteEvent badVelocityNeg = makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::D);
	badVelocityNeg.velocity = -0.01f;
	EXPECT_THROW((void) seq.insert(badVelocityNeg), std::out_of_range);

	NoteEvent badVelocityNan = makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::D);
	badVelocityNan.velocity = std::numeric_limits<float>::quiet_NaN();
	EXPECT_THROW((void) seq.insert(badVelocityNan), std::invalid_argument);

	EXPECT_EQ(seq.size(), 1u);

	// Bulk insert with one invalid event must leave sequence untouched.
	std::vector<NoteEvent> batch {
		makeSpelledNote({ 1, 2 }, { 1, 4 }, Letter::E),
		badVelocityNan
	};
	EXPECT_THROW((void) seq.insertBulk(batch), std::invalid_argument);
	EXPECT_EQ(seq.size(), 1u);
}

TEST(SequenceTest, CanonicalizesNegativeZeroVelocity) {
	Sequence seq;
	NoteEvent ev = makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C);
	ev.velocity = -0.0f;
	seq.insert(ev);
	EXPECT_EQ(seq[0].velocity, 0.0f);
	EXPECT_FALSE(std::signbit(seq[0].velocity));
}

// -----------------------------------------------------------------------------
// Timing Bounds & Queries
// -----------------------------------------------------------------------------
TEST(SequenceTest, BoundsAccountForLongEarlyNotesAndNegativeTimes) {
	Sequence seq;
	seq.insert(makeSpelledNote({ -1, 2 }, { 4, 1 }, Letter::C)); // [-1/2, 7/2)
	seq.insert(makeSpelledNote({ 1, 1 }, { 1, 4 }, Letter::D));  // [1, 5/4)

	EXPECT_EQ(seq.startTime(), (Rational { -1, 2 }));
	EXPECT_EQ(seq.endTime(), (Rational { 7, 2 }));
	EXPECT_EQ(seq.duration(), (Rational { 4, 1 }));
	ASSERT_TRUE(seq.bounds().has_value());
	EXPECT_EQ(*seq.bounds(), (TimeRange { { -1, 2 }, { 4, 1 } }));
}

TEST(SequenceTest, EventsInAndStartingAtBoundaries) {
	Sequence seq;
	const EventId id0 = seq.insert(makeSpelledNote({ 0, 1 }, { 1, 2 }, Letter::C));
	const EventId id1 = seq.insert(makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::D));
	const EventId id2 = seq.insert(makeSpelledNote({ 1, 4 }, { 1, 8 }, Letter::E));
	const EventId idZeroLen = seq.insert(makeSpelledNote({ 1, 2 }, { 0, 1 }, Letter::F));
	const EventId id3 = seq.insert(makeSpelledNote({ 1, 1 }, { 1, 4 }, Letter::G));

	// Half-open [1/4, 1/2): includes id1, id2; excludes idZeroLen at 1/2.
	const auto span1 = seq.eventsIn(TimeRange { { 1, 4 }, { 1, 4 } });
	ASSERT_EQ(span1.size(), 2u);
	EXPECT_EQ(span1[0].id, id1);
	EXPECT_EQ(span1[1].id, id2);

	// Half-open [1/2, 1): includes idZeroLen; excludes id3 at 1.
	const auto span2 = seq.eventsIn(TimeRange { { 1, 2 }, { 1, 2 } });
	ASSERT_EQ(span2.size(), 1u);
	EXPECT_EQ(span2[0].id, idZeroLen);

	// Empty query range returns nothing, even at an exact note start.
	EXPECT_TRUE(seq.eventsIn(TimeRange { { 1, 4 }, { 0, 1 } }).empty());
	EXPECT_TRUE(seq.eventsIn(TimeRange { { 1, 2 }, { 0, 1 } }).empty());

	// eventsStartingAt finds all notes with exact start.
	const auto atQuarter = seq.eventsStartingAt(Rational { 1, 4 });
	ASSERT_EQ(atQuarter.size(), 2u);
	EXPECT_EQ(atQuarter[0].id, id1);
	EXPECT_EQ(atQuarter[1].id, id2);

	const auto atZero = seq.eventsStartingAt(Rational { 0 });
	ASSERT_EQ(atZero.size(), 1u);
	EXPECT_EQ(atZero[0].id, id0);

	const auto atMissing = seq.eventsStartingAt(Rational { 3, 4 });
	EXPECT_TRUE(atMissing.empty());

	const auto atOne = seq.eventsStartingAt(Rational { 1, 1 });
	ASSERT_EQ(atOne.size(), 1u);
	EXPECT_EQ(atOne[0].id, id3);
}

TEST(SequenceTest, EventsOverlappingAndEventsAtHandleLongNotesAndZeroLength) {
	Sequence seq;
	const EventId idLong = seq.insert(makeSpelledNote({ 0, 1 }, { 10, 1 }, Letter::C));     // [0, 10)
	const EventId idShort1 = seq.insert(makeSpelledNote({ 1, 1 }, { 1, 1 }, Letter::D));    // [1, 2)
	const EventId idZero = seq.insert(makeSpelledNote({ 5, 1 }, { 0, 1 }, Letter::E));      // [5, 5)
	const EventId idShort2 = seq.insert(makeSpelledNote({ 6, 1 }, { 2, 1 }, Letter::F));    // [6, 8)

	// Query [3, 4): overlaps idLong despite idShort1 ending at 2.
	const auto ov34 = seq.eventsOverlapping(TimeRange { { 3, 1 }, { 1, 1 } });
	ASSERT_EQ(ov34.size(), 1u);
	EXPECT_EQ(ov34[0].id, idLong);

	// Query [4, 7): overlaps idLong and idShort2, NOT zero-length idZero.
	const auto ov47 = seq.eventsOverlapping(TimeRange { { 4, 1 }, { 3, 1 } });
	ASSERT_EQ(ov47.size(), 2u);
	EXPECT_EQ(ov47[0].id, idLong);
	EXPECT_EQ(ov47[1].id, idShort2);

	// Touching boundary [8, 10): only idLong overlaps (idShort2 ends at 8).
	const auto ov810 = seq.eventsOverlapping(TimeRange { { 8, 1 }, { 2, 1 } });
	ASSERT_EQ(ov810.size(), 1u);
	EXPECT_EQ(ov810[0].id, idLong);

	// Empty query range never overlaps.
	EXPECT_TRUE(seq.eventsOverlapping(TimeRange { { 5, 1 }, { 0, 1 } }).empty());

	// Point queries: half-open [start, end)
	EXPECT_EQ(seq.eventsAt(Rational { 1, 1 }).size(), 2u); // idLong + idShort1
	EXPECT_EQ(seq.eventsAt(Rational { 2, 1 }).size(), 1u); // idLong only (idShort1 ends at 2)
	EXPECT_EQ(seq.eventsAt(Rational { 5, 1 }).size(), 1u); // idLong only (idZero is empty)
	EXPECT_TRUE(seq.eventsAt(Rational { 10, 1 }).empty());
	EXPECT_TRUE(seq.eventsAt(Rational { -1, 1 }).empty());
	(void) idShort1;
	(void) idZero;
}

TEST(SequenceTest, VoicesAndEventsInVoice) {
	Sequence seq;
	seq.insert(makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C, Accidental::Natural, 4, 3));
	seq.insert(makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::D, Accidental::Natural, 4, 1));
	seq.insert(makeSpelledNote({ 1, 2 }, { 1, 4 }, Letter::E, Accidental::Natural, 4, 3));

	const auto v = seq.voices();
	EXPECT_EQ(v, (std::vector<std::uint8_t> { 1, 3 }));

	const auto v3 = seq.eventsInVoice(3);
	ASSERT_EQ(v3.size(), 2u);
	EXPECT_EQ(v3[0].start(), (Rational { 0, 1 }));
	EXPECT_EQ(v3[1].start(), (Rational { 1, 2 }));

	EXPECT_TRUE(seq.eventsInVoice(0).empty());
}

// -----------------------------------------------------------------------------
// EventId Lifecycle & Mutator Tests
// -----------------------------------------------------------------------------
TEST(SequenceTest, EventIdAssignmentAndUniqueness) {
	Sequence seq;
	const EventId a = seq.insert(makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C));
	const EventId b = seq.insert(makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::D));
	EXPECT_EQ(a, 1u);
	EXPECT_EQ(b, 2u);
	EXPECT_EQ(seq.nextEventId(), 3u);

	// Explicit ID ahead of nextId advances nextId.
	const EventId explicitId = seq.insert(makeSpelledNote({ 1, 2 }, { 1, 4 }, Letter::E, Accidental::Natural, 4, 0, 0.8f, 10));
	EXPECT_EQ(explicitId, 10u);
	EXPECT_EQ(seq.nextEventId(), 11u);

	const EventId afterExplicit = seq.insert(makeSpelledNote({ 3, 4 }, { 1, 4 }, Letter::F));
	EXPECT_EQ(afterExplicit, 11u);

	// Duplicate explicit ID must fail without mutating sequence.
	EXPECT_THROW(
		(void) seq.insert(makeSpelledNote({ 1, 1 }, { 1, 4 }, Letter::G, Accidental::Natural, 4, 0, 0.8f, 10)),
		std::invalid_argument);
	EXPECT_EQ(seq.size(), 4u);

	// Bulk insert mixing nullopt and explicit IDs must never collide.
	const auto bulkIds = seq.insertBulk({
		makeSpelledNote({ 2, 1 }, { 1, 4 }, Letter::A),
		makeSpelledNote({ 9, 4 }, { 1, 4 }, Letter::B, Accidental::Natural, 4, 0, 0.8f, 12),
		makeSpelledNote({ 5, 2 }, { 1, 4 }, Letter::C, Accidental::Natural, 5)
		});
	ASSERT_EQ(bulkIds.size(), 3u);
	EXPECT_EQ(bulkIds[1], 12u);
	EXPECT_NE(bulkIds[0], 12u);
	EXPECT_NE(bulkIds[2], 12u);
	verifySequenceInvariants(seq);

	// Duplicate explicit ID inside a bulk batch throws without mutating.
	const std::size_t sizeBefore = seq.size();
	EXPECT_THROW(
		(void) seq.insertBulk({
			makeSpelledNote({ 3, 1 }, { 1, 4 }, Letter::C, Accidental::Natural, 4, 0, 0.8f, 99),
			makeSpelledNote({ 4, 1 }, { 1, 4 }, Letter::D, Accidental::Natural, 4, 0, 0.8f, 99)
			}),
		std::invalid_argument);
	EXPECT_EQ(seq.size(), sizeBefore);

	// Explicit ID colliding with existing event throws without mutating.
	EXPECT_THROW(
		(void) seq.insertBulk({
			makeSpelledNote({ 3, 1 }, { 1, 4 }, Letter::C, Accidental::Natural, 4, 0, 0.8f, 50),
			makeSpelledNote({ 4, 1 }, { 1, 4 }, Letter::D, Accidental::Natural, 4, 0, 0.8f, 12)
			}),
		std::invalid_argument);
	EXPECT_EQ(seq.size(), sizeBefore);
}

TEST(SequenceTest, EventIdExhaustionAtUint64Max) {
	Sequence seq;
	constexpr EventId kMaxId = std::numeric_limits<EventId>::max();
	const EventId inserted = seq.insert(
		makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C, Accidental::Natural, 4, 0, 0.8f, kMaxId));
	EXPECT_EQ(inserted, kMaxId);

	// Auto-assigning when UINT64_MAX was already claimed must throw overflow_error.
	EXPECT_THROW((void) seq.insert(makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::D)), std::overflow_error);
	EXPECT_EQ(seq.size(), 1u);

	// Explicit smaller unused IDs can still be inserted.
	const EventId explicitSmall = seq.insert(
		makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::D, Accidental::Natural, 4, 0, 0.8f, 42));
	EXPECT_EQ(explicitSmall, 42u);
	EXPECT_EQ(seq.size(), 2u);
}

TEST(SequenceTest, RemoveAndUpdateMutations) {
	Sequence seq;
	const EventId id1 = seq.insert(makeSpelledNote({ 0, 1 }, { 4, 1 }, Letter::C));
	const EventId id2 = seq.insert(makeSpelledNote({ 1, 1 }, { 1, 2 }, Letter::D));
	const EventId id3 = seq.insert(makeSpelledNote({ 2, 1 }, { 1, 2 }, Letter::E));
	const EventId id4 = seq.insert(makeSpelledNote({ 3, 1 }, { 1, 2 }, Letter::F));

	// Update id2 to move after id4, keeping its EventId automatically.
	NoteEvent replacement = makeSpelledNote({ 5, 1 }, { 1, 1 }, Letter::G);
	EXPECT_TRUE(seq.updateById(id2, replacement));
	ASSERT_NE(seq.findById(id2), nullptr);
	EXPECT_EQ(seq.findById(id2)->start(), (Rational { 5, 1 }));
	EXPECT_EQ(seq.back().id, id2);
	EXPECT_EQ(seq.endTime(), (Rational { 6, 1 }));
	verifySequenceInvariants(seq);

	// Update missing ID returns false.
	EXPECT_FALSE(seq.updateById(9999, replacement));

	// Updating to an existing other EventId throws without mutating.
	NoteEvent conflicting = replacement.withId(id3);
	EXPECT_THROW((void) seq.updateById(id2, conflicting), std::invalid_argument);
	EXPECT_EQ(seq.findById(id2)->start(), (Rational { 5, 1 }));

	// removeById removes long note id1 and repairs endTime/prefixMaxEnd.
	EXPECT_TRUE(seq.removeById(id1));
	EXPECT_FALSE(seq.removeById(id1));
	EXPECT_EQ(seq.startTime(), (Rational { 2, 1 }));
	EXPECT_EQ(seq.endTime(), (Rational { 6, 1 }));

	// removeIn [2, 3) removes id3 only.
	EXPECT_EQ(seq.removeIn(TimeRange { { 2, 1 }, { 1, 1 } }), 1u);
	EXPECT_FALSE(seq.containsId(id3));
	EXPECT_TRUE(seq.containsId(id4));

	// removeIf removes id4 (start == 3).
	const auto removed = seq.removeIf([](const NoteEvent& ev) {
		return ev.start() == Rational { 3, 1 };
	});
	EXPECT_EQ(removed, 1u);
	EXPECT_EQ(seq.size(), 1u);
	EXPECT_EQ(seq[0].id, id2);

	// eraseAt
	EXPECT_THROW(seq.eraseAt(1), std::out_of_range);
	seq.eraseAt(0);
	EXPECT_TRUE(seq.empty());
}

// -----------------------------------------------------------------------------
// Copy-on-Write Semantics Tests
// -----------------------------------------------------------------------------
TEST(SequenceTest, CopyOnWriteSharesStorageAndDetachesOnMutation) {
	Sequence original;
	const EventId id1 = original.insert(makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C));
	const EventId id2 = original.insert(makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::D));

	Sequence copy = original;
	EXPECT_EQ(copy, original);
	// Shared internal buffer: pointer identity holds before mutation.
	EXPECT_EQ(copy.events().data(), original.events().data());

	// No-op mutations must NOT detach.
	EXPECT_FALSE(copy.removeById(999));
	EXPECT_EQ(copy.removeIn(TimeRange { { 10, 1 }, { 1, 1 } }), 0u);
	EXPECT_EQ(copy.removeIf([](const NoteEvent&) { return false; }), 0u);
	EXPECT_TRUE(copy.insertBulk({}).empty());
	EXPECT_EQ(copy.events().data(), original.events().data());

	// Mutating copy detaches without affecting original.
	const EventId id3 = copy.insert(makeSpelledNote({ 1, 2 }, { 1, 4 }, Letter::E));
	EXPECT_NE(copy.events().data(), original.events().data());
	EXPECT_EQ(original.size(), 2u);
	EXPECT_EQ(copy.size(), 3u);
	EXPECT_FALSE(original.containsId(id3));
	EXPECT_TRUE(copy.containsId(id1));
	EXPECT_TRUE(copy.containsId(id2));
	EXPECT_TRUE(copy.containsId(id3));

	// clear() on a shared copy detaches cleanly.
	Sequence copy2 = original;
	EXPECT_EQ(copy2.events().data(), original.events().data());
	copy2.clear();
	EXPECT_TRUE(copy2.empty());
	EXPECT_EQ(original.size(), 2u);
}

// -----------------------------------------------------------------------------
// Equality, Content Equality, Ordering, Formatting & Hashing
// -----------------------------------------------------------------------------
TEST(SequenceTest, EqualityOrderingHashAndFormatting) {
	Sequence s1;
	s1.insert(makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C));

	Sequence s2;
	s2.insert(makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C, Accidental::Natural, 4, 0, 0.8f, 99));

	// Different EventIds => operator!=, but hasSameContent is true.
	EXPECT_NE(s1, s2);
	EXPECT_TRUE(s1.hasSameContent(s2));

	Sequence s1Copy = s1;
	EXPECT_EQ(s1, s1Copy);
	EXPECT_EQ(std::hash<Sequence> {}(s1), std::hash<Sequence> {}(s1Copy));

	// Empty sequences are equal and hash identically regardless of prior history.
	Sequence cleared = s1;
	cleared.clear();
	Sequence freshEmpty;
	EXPECT_EQ(cleared, freshEmpty);
	EXPECT_EQ(std::hash<Sequence> {}(cleared), std::hash<Sequence> {}(freshEmpty));

	// Lexicographical ordering
	Sequence earlier;
	earlier.insert(makeSpelledNote({ 0, 1 }, { 1, 4 }, Letter::C));
	Sequence later;
	later.insert(makeSpelledNote({ 1, 4 }, { 1, 4 }, Letter::C));
	EXPECT_LT(earlier, later);

	// String formatting
	EXPECT_EQ(freshEmpty.toString(), "Sequence(0 events)");
	EXPECT_EQ(earlier.toString(), "Sequence(1 events, [0, 1/4))");
}

// -----------------------------------------------------------------------------
// Randomized Property & Oracle Tests
// -----------------------------------------------------------------------------
TEST(SequenceTest, RandomizedOperationsMatchBruteForceOracle) {
	SplitMix64 rng(0xDEADBEEFCAFE1234ULL);
	Sequence seq;

	auto randomRational = [&rng](int minNum, int maxNum) {
		static constexpr int kDens[] = { 1, 2, 3, 4, 6, 8 };
		const int num = rng.nextInt(minNum, maxNum);
		const int den = kDens[rng.nextInt(0, 5)];
		return Rational { num, den };
	};

	auto randomEvent = [&]() {
		const Rational start = randomRational(-8, 24);
		const Rational dur = randomRational(0, 16);
		const auto letter = static_cast<Letter>(rng.nextInt(0, 6));
		const auto acc = static_cast<Accidental>(rng.nextInt(-2, 2));
		const int octave = rng.nextInt(1, 7);
		const auto voice = static_cast<std::uint8_t>(rng.nextInt(0, 3));
		const float vel = static_cast<float>(rng.nextInt(0, 127)) / 127.0f;
		return makeSpelledNote(start, dur, letter, acc, octave, voice, vel);
	};

	for (int step = 0; step < 300; ++step) {
		const int action = rng.nextInt(0, 9);
		if (action <= 3) {
			(void) seq.insert(randomEvent());
		} else if (action == 4) {
			std::vector<NoteEvent> batch;
			const int count = rng.nextInt(1, 8);
			for (int i = 0; i < count; ++i) {
				batch.push_back(randomEvent());
			}
			(void) seq.insertBulk(batch);
		} else if (action == 5 && !seq.empty()) {
			const auto idx = static_cast<std::size_t>(rng.nextInt(0, static_cast<int>(seq.size()) - 1));
			const EventId id = *seq[idx].id;
			EXPECT_TRUE(seq.removeById(id));
		} else if (action == 6 && !seq.empty()) {
			const auto idx = static_cast<std::size_t>(rng.nextInt(0, static_cast<int>(seq.size()) - 1));
			const EventId id = *seq[idx].id;
			EXPECT_TRUE(seq.updateById(id, randomEvent()));
		} else if (action == 7) {
			const TimeRange r { randomRational(-8, 24), randomRational(0, 8) };
			(void) seq.removeIn(r);
		} else if (action == 8 && seq.size() > 25) {
			seq.removeIf([](const NoteEvent& ev) { return ev.voice == 2; });
		}

		verifySequenceInvariants(seq);

		// Validate all indexed queries against brute-force oracles.
		for (int q = 0; q < 5; ++q) {
			const Rational p = randomRational(-10, 30);
			const TimeRange r { p, randomRational(0, 10) };

			const auto gotIn = seq.eventsIn(r);
			const auto expIn = oracleEventsIn(seq, r);
			EXPECT_TRUE(std::equal(gotIn.begin(), gotIn.end(), expIn.begin(), expIn.end()));

			const auto gotStart = seq.eventsStartingAt(p);
			const auto expStart = oracleEventsStartingAt(seq, p);
			EXPECT_TRUE(std::equal(gotStart.begin(), gotStart.end(), expStart.begin(), expStart.end()));

			EXPECT_EQ(seq.eventsOverlapping(r), oracleEventsOverlapping(seq, r));
			EXPECT_EQ(seq.eventsAt(p), oracleEventsAt(seq, p));
		}

		if (seq.empty()) {
			EXPECT_FALSE(seq.bounds().has_value());
			EXPECT_EQ(seq.startTime(), Rational { 0 });
			EXPECT_EQ(seq.endTime(), Rational { 0 });
		} else {
			Rational minStart = seq[0].start();
			Rational maxEnd = seq[0].end();
			for (const auto& ev : seq) {
				if (ev.start() < minStart) {
					minStart = ev.start();
				}
				if (ev.end() > maxEnd) {
					maxEnd = ev.end();
				}
			}
			EXPECT_EQ(seq.startTime(), minStart);
			EXPECT_EQ(seq.endTime(), maxEnd);
			EXPECT_EQ(seq.duration(), maxEnd - minStart);
			EXPECT_EQ(seq.bounds(), TimeRange::fromEndpoints(minStart, maxEnd));
		}
	}
}

} // namespace
} // namespace duey::test
