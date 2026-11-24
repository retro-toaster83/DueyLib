
#include "duey/core/NoteEvent.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace duey::tests {
namespace {

static_assert(std::is_aggregate_v<NoteEvent>);
static_assert(std::is_copy_constructible_v<NoteEvent>);
static_assert(std::is_copy_assignable_v<NoteEvent>);
static_assert(std::is_move_constructible_v<NoteEvent>);
static_assert(std::is_move_assignable_v<NoteEvent>);

// Compile-time constexpr checks
constexpr NoteEvent kConstexprEvent {
	.span = TimeRange { Rational { 1, 4 }, Rational { 1, 2 } },
	.pitch = SpelledPitch { Letter::F, Accidental::Sharp, 4 },
	.velocity = 0.5f,
	.voice = 2,
	.id = EventId { 7 }
};
static_assert(kConstexprEvent.isValid());
static_assert(kConstexprEvent.start() == Rational { 1, 4 });
static_assert(kConstexprEvent.duration() == Rational { 1, 2 });
static_assert(kConstexprEvent.end() == Rational { 3, 4 });
static_assert(kConstexprEvent.isSpelled());
static_assert(!kConstexprEvent.isMidi());
static_assert(kConstexprEvent.midiNote() == 66);
static_assert(kConstexprEvent.pitchClass() == 6);
static_assert(kConstexprEvent.toMidiPitch() == MidiPitch { 66 });

TEST(NoteEventTest, DefaultConstructionAndDesignatedInitializers) {
	const NoteEvent def {};
	EXPECT_EQ(def.span, TimeRange {});
	EXPECT_TRUE(def.isSpelled());
	EXPECT_EQ(*def.spelledPitch(), (SpelledPitch { Letter::C, Accidental::Natural, 4 }));
	EXPECT_FLOAT_EQ(def.velocity, NoteEvent::kDefaultVelocity);
	EXPECT_EQ(def.voice, 0);
	EXPECT_FALSE(def.id.has_value());
	EXPECT_TRUE(def.isValid());
	EXPECT_TRUE(def.isZeroLength());

	const NoteEvent partial {
		.span = TimeRange { Rational { 0 }, Rational { 1, 4 } },
		.pitch = MidiPitch { 60, 15.0f }
	};
	EXPECT_TRUE(partial.isMidi());
	EXPECT_EQ(partial.spelledPitch(), nullptr);
	ASSERT_NE(partial.midiPitch(), nullptr);
	EXPECT_EQ(*partial.midiPitch(), (MidiPitch { 60, 15.0f }));
	EXPECT_FLOAT_EQ(partial.velocity, NoteEvent::kDefaultVelocity);
	EXPECT_EQ(partial.voice, 0);
	EXPECT_FALSE(partial.id.has_value());
}

TEST(NoteEventTest, MakeFactoryValidatesAndCanonicalizes) {
	const TimeRange span { Rational { 1, 8 }, Rational { 1, 4 } };
	const NoteEvent created = NoteEvent::make(
		span,
		SpelledPitch { Letter::A, Accidental::Natural, 4 },
		-0.0f,
		3,
		EventId { 99 });

	EXPECT_EQ(created.span, span);
	EXPECT_EQ(created.velocity, 0.0f);
	EXPECT_FALSE(std::signbit(created.velocity));
	EXPECT_EQ(created.voice, 3);
	EXPECT_EQ(created.id, EventId { 99 });

	EXPECT_THROW(
		(void) NoteEvent::make(span, SpelledPitch {}, -0.01f),
		std::out_of_range);
	EXPECT_THROW(
		(void) NoteEvent::make(span, SpelledPitch {}, 1.01f),
		std::out_of_range);
	EXPECT_THROW(
		(void) NoteEvent::make(span, SpelledPitch {}, std::numeric_limits<float>::quiet_NaN()),
		std::invalid_argument);
	EXPECT_THROW(
		(void) NoteEvent::make(span, SpelledPitch {}, std::numeric_limits<float>::infinity()),
		std::invalid_argument);
}

TEST(NoteEventTest, TimingAccessorsAndShift) {
	const NoteEvent event {
		.span = TimeRange { Rational { 1, 2 }, Rational { 3, 8 } },
		.pitch = SpelledPitch { Letter::D, Accidental::Flat, 5 },
		.velocity = 0.75f,
		.voice = 1,
		.id = EventId { 12 }
	};

	EXPECT_EQ(event.start(), (Rational { 1, 2 }));
	EXPECT_EQ(event.duration(), (Rational { 3, 8 }));
	EXPECT_EQ(event.end(), (Rational { 7, 8 }));
	EXPECT_FALSE(event.isZeroLength());

	const NoteEvent moved = event.shifted(Rational { -1, 4 });
	EXPECT_EQ(moved.start(), (Rational { 1, 4 }));
	EXPECT_EQ(moved.duration(), (Rational { 3, 8 }));
	EXPECT_EQ(moved.end(), (Rational { 5, 8 }));
	EXPECT_EQ(moved.pitch, event.pitch);
	EXPECT_EQ(moved.velocity, event.velocity);
	EXPECT_EQ(moved.voice, event.voice);
	EXPECT_EQ(moved.id, event.id);
	EXPECT_EQ(event.start(), (Rational { 1, 2 }));
}

TEST(NoteEventTest, PitchProjectionsAndSemitones) {
	const NoteEvent spelled {
		.span = TimeRange { Rational { 0 }, Rational { 1, 4 } },
		.pitch = SpelledPitch { Letter::G, Accidental::Flat, 4 }
	};
	EXPECT_EQ(spelled.midiNote(), 66);
	EXPECT_DOUBLE_EQ(spelled.toSemitones(), 66.0);
	EXPECT_EQ(spelled.pitchClass(), 6);
	EXPECT_EQ(spelled.toMidiPitch(), (MidiPitch { 66, 0.0f }));
	EXPECT_EQ(spelled.tryToMidiPitch(), (MidiPitch { 66, 0.0f }));
	// Preserves original spelling even when preferFlats is false.
	EXPECT_EQ(spelled.toSpelledPitch(false), (SpelledPitch { Letter::G, Accidental::Flat, 4 }));

	const NoteEvent outOfMidiRange {
		.span = TimeRange { Rational { 0 }, Rational { 1, 4 } },
		.pitch = SpelledPitch { Letter::C, Accidental::Natural, 10 }
	};
	EXPECT_EQ(outOfMidiRange.midiNote(), 132);
	EXPECT_FALSE(outOfMidiRange.tryToMidiPitch().has_value());
	EXPECT_THROW((void) outOfMidiRange.toMidiPitch(), std::out_of_range);

	const NoteEvent microtonal {
		.span = TimeRange { Rational { 0 }, Rational { 1, 4 } },
		.pitch = MidiPitch { 61, 25.0f }
	};
	EXPECT_EQ(microtonal.midiNote(), 61);
	EXPECT_DOUBLE_EQ(microtonal.toSemitones(), 61.25);
	EXPECT_EQ(microtonal.pitchClass(), 1);
	EXPECT_EQ(microtonal.toMidiPitch(), (MidiPitch { 61, 25.0f }));
	EXPECT_EQ(microtonal.toSpelledPitch(false), (SpelledPitch { Letter::C, Accidental::Sharp, 4 }));
	EXPECT_EQ(microtonal.toSpelledPitch(true), (SpelledPitch { Letter::D, Accidental::Flat, 4 }));
}

TEST(NoteEventTest, EnharmonicPitchComparison) {
	const NoteEvent fSharp { .pitch = SpelledPitch { Letter::F, Accidental::Sharp, 4 } };
	const NoteEvent gFlat { .pitch = SpelledPitch { Letter::G, Accidental::Flat, 4 } };
	const NoteEvent midi66 { .pitch = MidiPitch { 66, 0.0f } };
	const NoteEvent midi66Detuned { .pitch = MidiPitch { 66, 5.0f } };
	const NoteEvent belowZero { .pitch = SpelledPitch { Letter::C, Accidental::Natural, -2 } };
	const NoteEvent midi0 { .pitch = MidiPitch { 0, 0.0f } };

	EXPECT_NE(fSharp, gFlat);
	EXPECT_NE(fSharp, midi66);
	EXPECT_TRUE(fSharp.hasEnharmonicPitch(gFlat));
	EXPECT_TRUE(fSharp.hasEnharmonicPitch(midi66));
	EXPECT_TRUE(midi66.hasEnharmonicPitch(fSharp));
	EXPECT_FALSE(fSharp.hasEnharmonicPitch(midi66Detuned));
	EXPECT_FALSE(midi66Detuned.hasEnharmonicPitch(fSharp));
	EXPECT_FALSE(belowZero.hasEnharmonicPitch(midi0));
}

TEST(NoteEventTest, VelocityValidationClampingAndMidiRoundTrip) {
	EXPECT_TRUE(NoteEvent::isValidVelocity(0.0f));
	EXPECT_TRUE(NoteEvent::isValidVelocity(-0.0f));
	EXPECT_TRUE(NoteEvent::isValidVelocity(1.0f));
	EXPECT_FALSE(NoteEvent::isValidVelocity(-0.001f));
	EXPECT_FALSE(NoteEvent::isValidVelocity(1.001f));
	EXPECT_FALSE(NoteEvent::isValidVelocity(std::numeric_limits<float>::quiet_NaN()));
	EXPECT_FALSE(NoteEvent::isValidVelocity(std::numeric_limits<float>::infinity()));

	const float clampedNegZero = NoteEvent::clampVelocity(-0.0f);
	EXPECT_EQ(clampedNegZero, 0.0f);
	EXPECT_FALSE(std::signbit(clampedNegZero));
	EXPECT_FLOAT_EQ(NoteEvent::clampVelocity(-2.0f), 0.0f);
	EXPECT_FLOAT_EQ(NoteEvent::clampVelocity(2.0f), 1.0f);
	EXPECT_THROW(
		(void) NoteEvent::clampVelocity(std::numeric_limits<float>::quiet_NaN()),
		std::invalid_argument);

	NoteEvent invalid { .velocity = 1.5f };
	EXPECT_FALSE(invalid.isValid());
	EXPECT_THROW(invalid.validate(), std::out_of_range);
	EXPECT_THROW((void) invalid.toMidiVelocity(), std::out_of_range);

	const NoteEvent fixed = invalid.normalized();
	EXPECT_TRUE(fixed.isValid());
	EXPECT_FLOAT_EQ(fixed.velocity, 1.0f);

	for (int v = 0; v <= 127; ++v) {
		const float normalized = NoteEvent::velocityFromMidi(v);
		const NoteEvent e { .velocity = normalized };
		EXPECT_EQ(e.toMidiVelocity(), static_cast<std::uint8_t>(v));
	}
	EXPECT_THROW((void) NoteEvent::velocityFromMidi(-1), std::out_of_range);
	EXPECT_THROW((void) NoteEvent::velocityFromMidi(128), std::out_of_range);
}

TEST(NoteEventTest, ImmutableWithModifiers) {
	const NoteEvent base {
		.span = TimeRange { Rational { 0 }, Rational { 1, 4 } },
		.pitch = SpelledPitch { Letter::C, Accidental::Natural, 4 },
		.velocity = 0.5f,
		.voice = 1,
		.id = EventId { 42 }
	};

	const NoteEvent updated = base
		.withSpan(TimeRange { Rational { 1, 2 }, Rational { 1, 2 } })
		.withPitch(MidiPitch { 64, -10.0f })
		.withVelocity(0.25f)
		.withVoice(4);

	EXPECT_EQ(updated.span, (TimeRange { Rational { 1, 2 }, Rational { 1, 2 } }));
	EXPECT_EQ(updated.pitch, (NoteEvent::Pitch { MidiPitch { 64, -10.0f } }));
	EXPECT_FLOAT_EQ(updated.velocity, 0.25f);
	EXPECT_EQ(updated.voice, 4);
	EXPECT_EQ(updated.id, EventId { 42 });

	EXPECT_EQ(updated.withId(std::nullopt).id, std::nullopt);
	EXPECT_FLOAT_EQ(base.withClampedVelocity(5.0f).velocity, 1.0f);
	EXPECT_THROW((void) base.withVelocity(1.2f), std::out_of_range);
	EXPECT_THROW((void) base.withVelocity(std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
}

TEST(NoteEventTest, EqualitySameContentAndOrdering) {
	const NoteEvent a {
		.span = TimeRange { Rational { 0 }, Rational { 1, 4 } },
		.pitch = SpelledPitch { Letter::F, Accidental::Sharp, 4 },
		.velocity = 0.8f,
		.voice = 0,
		.id = EventId { 1 }
	};
	const NoteEvent aDiffId = a.withId(EventId { 2 });
	EXPECT_NE(a, aDiffId);
	EXPECT_TRUE(a.hasSameContent(aDiffId));
	EXPECT_LT(a, aDiffId);

	const NoteEvent eEarlierStart = a.withSpan(TimeRange { Rational { -1, 4 }, Rational { 1, 4 } });
	const NoteEvent eHigherVoice = a.withVoice(1);
	const NoteEvent eFlatBelow {
		.span = a.span,
		.pitch = MidiPitch { 66, -20.0f },
		.velocity = 0.8f,
		.voice = 0
	};
	const NoteEvent eEnharmonicGb = a.withPitch(SpelledPitch { Letter::G, Accidental::Flat, 4 });
	const NoteEvent eMidi66 = a.withPitch(MidiPitch { 66, 0.0f });
	const NoteEvent eSharpAbove {
		.span = a.span,
		.pitch = MidiPitch { 66, 20.0f },
		.velocity = 0.8f,
		.voice = 0
	};
	const NoteEvent eLonger = a.withSpan(TimeRange { Rational { 0 }, Rational { 1, 2 } });

	std::vector<NoteEvent> events {
		eHigherVoice,
		eSharpAbove,
		eMidi66,
		eLonger,
		eEnharmonicGb,
		aDiffId,
		a,
		eFlatBelow,
		eEarlierStart
	};
	std::sort(events.begin(), events.end());

	ASSERT_EQ(events.size(), 9u);
	EXPECT_EQ(events[0], eEarlierStart);
	EXPECT_EQ(events[1], eFlatBelow);
	EXPECT_EQ(events[2], a);
	EXPECT_EQ(events[3], aDiffId);
	EXPECT_EQ(events[4], eLonger);
	EXPECT_EQ(events[5], eEnharmonicGb);
	EXPECT_EQ(events[6], eMidi66);
	EXPECT_EQ(events[7], eSharpAbove);
	EXPECT_EQ(events[8], eHigherVoice);

	const NoteEvent nanVel { .velocity = std::numeric_limits<float>::quiet_NaN() };
	const NoteEvent validVel { .velocity = 0.5f };
	EXPECT_EQ((nanVel <=> validVel), std::partial_ordering::unordered);
}

TEST(NoteEventTest, HashConsistencyAndContainerSupport) {
	const NoteEvent zeroPos {
		.span = TimeRange { Rational { 1, 4 }, Rational { 1, 4 } },
		.pitch = SpelledPitch { Letter::E, Accidental::Flat, 4 },
		.velocity = 0.0f,
		.voice = 1,
		.id = EventId { 10 }
	};
	const NoteEvent zeroNeg = zeroPos.withVelocity(-0.0f);
	// Mutate directly to bypass canonicalization in withVelocity and verify hash equivalence.
	NoteEvent rawZeroNeg = zeroPos;
	rawZeroNeg.velocity = -0.0f;

	EXPECT_EQ(zeroPos, zeroNeg);
	EXPECT_EQ(zeroPos, rawZeroNeg);
	EXPECT_EQ(std::hash<NoteEvent> {}(zeroPos), std::hash<NoteEvent> {}(rawZeroNeg));

	std::unordered_set<NoteEvent> set;
	set.insert(zeroPos);
	set.insert(rawZeroNeg);
	EXPECT_EQ(set.size(), 1u);

	set.insert(zeroPos.withPitch(SpelledPitch { Letter::D, Accidental::Sharp, 4 }));
	set.insert(zeroPos.withPitch(MidiPitch { 63, 0.0f }));
	set.insert(zeroPos.withId(EventId { 11 }));
	EXPECT_EQ(set.size(), 4u);
}

TEST(NoteEventTest, FormattingAndStreamOutput) {
	const NoteEvent spelled {
		.span = TimeRange { Rational { 0 }, Rational { 1, 4 } },
		.pitch = SpelledPitch { Letter::F, Accidental::Sharp, 4 },
		.velocity = 0.5f,
		.voice = 2
	};
	EXPECT_EQ(
		spelled.toString(),
		"NoteEvent(span=[0, 1/4), pitch=F#4, velocity=0.5, voice=2)");

	const NoteEvent midiWithId {
		.span = TimeRange { Rational { 1, 4 }, Rational { 1, 2 } },
		.pitch = MidiPitch { 60, 25.0f },
		.velocity = -0.0f,
		.voice = 0,
		.id = EventId { 5 }
	};
	const std::string formatted = midiWithId.toString();
	EXPECT_NE(formatted.find("span=[1/4, 3/4)"), std::string::npos);
	EXPECT_NE(formatted.find("velocity=0,"), std::string::npos);
	EXPECT_NE(formatted.find("voice=0"), std::string::npos);
	EXPECT_NE(formatted.find("id=5"), std::string::npos);

	std::ostringstream oss;
	oss << spelled;
	EXPECT_EQ(oss.str(), spelled.toString());
}

} // namespace
} // namespace duey::tests
