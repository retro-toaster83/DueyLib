
#pragma once

#include "MidiPitch.hpp"
#include "Rational.hpp"
#include "SpelledPitch.hpp"
#include "TimeRange.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace duey {

/// Monotonic per-Sequence event identifier used to preserve identity across transforms.
using EventId = std::uint64_t;

/**
 * @brief Symbolic note event combining a rational time span, pitch, velocity, and voice.
 *
 * Intentionally kept as a C++20 aggregate struct so designated initializers
 * (e.g. `NoteEvent { .span = ..., .pitch = ... }`) remain valid.
 *
 * Conventions:
 *  - `span` is a half-open TimeRange `[start, end)` with `duration >= 0`.
 *  - `pitch` holds either a canonical `SpelledPitch` or a projected `MidiPitch`.
 *  - `velocity` is normalized to `[0.0f, 1.0f]`.
 *  - `voice` identifies a polyphonic voice/lane index in `[0, 255]`.
 *  - `id` is an optional stable `EventId` assigned by `Sequence`.
 */
struct NoteEvent {
	using Pitch = std::variant<SpelledPitch, MidiPitch>;

	static constexpr float kMinVelocity = 0.0f;
	static constexpr float kMaxVelocity = 1.0f;
	static constexpr float kDefaultVelocity = 1.0f;
	static constexpr std::uint8_t kDefaultVoice = 0;

	TimeRange span {};
	Pitch pitch { SpelledPitch {} };
	float velocity { kDefaultVelocity };
	std::uint8_t voice { kDefaultVoice };
	std::optional<EventId> id { std::nullopt };

	// -------------------------------------------------------------------------
	// Factories & Validation
	// -------------------------------------------------------------------------

	/// Validating factory that enforces finite, in-range velocity and non-negative span duration.
	[[nodiscard]] static constexpr NoteEvent make(
		const TimeRange& span,
		const Pitch& pitch,
		float velocity = kDefaultVelocity,
		std::uint8_t voice = kDefaultVoice,
		std::optional<EventId> id = std::nullopt) {
		NoteEvent event {
			.span = span,
			.pitch = pitch,
			.velocity = velocity,
			.voice = voice,
			.id = id
		};
		event.validate();
		if (event.velocity == 0.0f) {
			event.velocity = 0.0f; // Canonicalize -0.0f to +0.0f.
		}
		return event;
	}

	[[nodiscard]] static constexpr bool isValidVelocity(float value) noexcept {
		constexpr float kMaxFloat = std::numeric_limits<float>::max();
		if (!(value >= -kMaxFloat && value <= kMaxFloat)) {
			return false;
		}
		return value >= kMinVelocity && value <= kMaxVelocity;
	}

	/// Clamps a finite velocity to [0.0f, 1.0f] and canonicalizes -0.0f to +0.0f.
	[[nodiscard]] static constexpr float clampVelocity(float value) {
		constexpr float kMaxFloat = std::numeric_limits<float>::max();
		if (!(value >= -kMaxFloat && value <= kMaxFloat)) {
			throw std::invalid_argument("NoteEvent velocity must be finite");
		}
		if (value <= kMinVelocity) {
			return 0.0f;
		}
		if (value >= kMaxVelocity) {
			return kMaxVelocity;
		}
		return value;
	}

	/// Converts a 7-bit MIDI velocity in [0, 127] to normalized [0.0f, 1.0f].
	[[nodiscard]] static constexpr float velocityFromMidi(int midiVelocity) {
		if (midiVelocity < 0 || midiVelocity > 127) {
			throw std::out_of_range("MIDI velocity must be in [0, 127]");
		}
		return static_cast<float>(midiVelocity) / 127.0f;
	}

	[[nodiscard]] constexpr bool isValid() const noexcept {
		return !span.duration().isNegative() && isValidVelocity(velocity);
	}

	constexpr void validate() const {
		if (span.duration().isNegative()) {
			throw std::invalid_argument("NoteEvent span duration cannot be negative");
		}
		constexpr float kMaxFloat = std::numeric_limits<float>::max();
		if (!(velocity >= -kMaxFloat && velocity <= kMaxFloat)) {
			throw std::invalid_argument("NoteEvent velocity must be finite");
		}
		if (velocity < kMinVelocity || velocity > kMaxVelocity) {
			throw std::out_of_range("NoteEvent velocity must be in [0.0, 1.0]");
		}
	}

	// -------------------------------------------------------------------------
	// Timing Observers
	// -------------------------------------------------------------------------
	[[nodiscard]] constexpr Rational start() const noexcept { return span.start(); }
	[[nodiscard]] constexpr Rational duration() const noexcept { return span.duration(); }
	[[nodiscard]] constexpr Rational end() const { return span.end(); }
	[[nodiscard]] constexpr bool isZeroLength() const noexcept { return span.duration().isZero(); }

	// -------------------------------------------------------------------------
	// Pitch Observers & Projections
	// -------------------------------------------------------------------------
	[[nodiscard]] constexpr bool isSpelled() const noexcept {
		return std::holds_alternative<SpelledPitch>(pitch);
	}

	[[nodiscard]] constexpr bool isMidi() const noexcept {
		return std::holds_alternative<MidiPitch>(pitch);
	}

	[[nodiscard]] constexpr const SpelledPitch* spelledPitch() const noexcept {
		return std::get_if<SpelledPitch>(&pitch);
	}

	[[nodiscard]] constexpr const MidiPitch* midiPitch() const noexcept {
		return std::get_if<MidiPitch>(&pitch);
	}

	/// Projects the event's pitch onto MidiPitch; throws std::out_of_range if outside [0, 127].
	[[nodiscard]] constexpr MidiPitch toMidiPitch() const {
		if (const auto* spelled = spelledPitch()) {
			return MidiPitch { spelled->midiNote(), 0.0f };
		}
		return std::get<MidiPitch>(pitch);
	}

	/// Non-throwing MidiPitch projection; returns std::nullopt if SpelledPitch is outside [0, 127].
	[[nodiscard]] constexpr std::optional<MidiPitch> tryToMidiPitch() const noexcept {
		if (const auto* spelled = spelledPitch()) {
			const auto note = spelled->toMidiNoteNumber();
			if (!note.has_value()) {
				return std::nullopt;
			}
			return MidiPitch { static_cast<int>(*note), 0.0f };
		}
		return std::get<MidiPitch>(pitch);
	}

	/// Returns the canonical SpelledPitch if present, or projects MidiPitch using preferFlats.
	[[nodiscard]] constexpr SpelledPitch toSpelledPitch(bool preferFlats = false) const noexcept {
		if (const auto* spelled = spelledPitch()) {
			return *spelled;
		}
		return SpelledPitch::fromMidiNote(static_cast<int>(std::get<MidiPitch>(pitch).noteNumber()), preferFlats);
	}

	/// Unclamped 12-TET integer MIDI note number.
	[[nodiscard]] constexpr int midiNote() const noexcept {
		if (const auto* spelled = spelledPitch()) {
			return spelled->midiNote();
		}
		return static_cast<int>(std::get<MidiPitch>(pitch).noteNumber());
	}

	/// Continuous 12-TET semitone position (including cent offset for MidiPitch).
	[[nodiscard]] constexpr double toSemitones() const noexcept {
		if (const auto* spelled = spelledPitch()) {
			return static_cast<double>(spelled->midiNote());
		}
		return std::get<MidiPitch>(pitch).toSemitones();
	}

	/// Pitch class in [0, 11], where C = 0.
	[[nodiscard]] constexpr int pitchClass() const noexcept {
		if (const auto* spelled = spelledPitch()) {
			return spelled->pitchClass();
		}
		return static_cast<int>(std::get<MidiPitch>(pitch).noteNumber() % 12);
	}

	/// Quantizes normalized velocity [0.0f, 1.0f] to a 7-bit MIDI velocity in [0, 127].
	[[nodiscard]] constexpr std::uint8_t toMidiVelocity() const {
		validate();
		const int rounded = static_cast<int>(velocity * 127.0f + 0.5f);
		return static_cast<std::uint8_t>(rounded > 127 ? 127 : rounded);
	}

	/// Explicit enharmonic equivalence check on pitch (ignoring span, velocity, voice, and id).
	[[nodiscard]] constexpr bool hasEnharmonicPitch(const NoteEvent& other) const noexcept {
		if (isSpelled() && other.isSpelled()) {
			return std::get<SpelledPitch>(pitch).isEnharmonicWith(std::get<SpelledPitch>(other.pitch));
		}
		if (isMidi() && other.isMidi()) {
			return std::get<MidiPitch>(pitch) == std::get<MidiPitch>(other.pitch);
		}
		if (const auto* spelled = spelledPitch()) {
			const auto& midi = std::get<MidiPitch>(other.pitch);
			return midi.isExactNote() && spelled->midiNote() == static_cast<int>(midi.noteNumber());
		}
		const auto& midi = std::get<MidiPitch>(pitch);
		const auto& spelled = std::get<SpelledPitch>(other.pitch);
		return midi.isExactNote() && static_cast<int>(midi.noteNumber()) == spelled.midiNote();
	}

	/// Compares musical content (span, pitch, velocity, voice) while ignoring EventId.
	[[nodiscard]] constexpr bool hasSameContent(const NoteEvent& other) const noexcept {
		return span == other.span
			&& pitch == other.pitch
			&& velocity == other.velocity
			&& voice == other.voice;
	}

	// -------------------------------------------------------------------------
	// Value-Returning Modifiers
	// -------------------------------------------------------------------------
	[[nodiscard]] constexpr NoteEvent withSpan(const TimeRange& newSpan) const noexcept {
		NoteEvent copy = *this;
		copy.span = newSpan;
		return copy;
	}

	[[nodiscard]] constexpr NoteEvent withPitch(const Pitch& newPitch) const noexcept {
		NoteEvent copy = *this;
		copy.pitch = newPitch;
		return copy;
	}

	[[nodiscard]] constexpr NoteEvent withVelocity(float newVelocity) const {
		constexpr float kMaxFloat = std::numeric_limits<float>::max();
		if (!(newVelocity >= -kMaxFloat && newVelocity <= kMaxFloat)) {
			throw std::invalid_argument("NoteEvent velocity must be finite");
		}
		if (newVelocity < kMinVelocity || newVelocity > kMaxVelocity) {
			throw std::out_of_range("NoteEvent velocity must be in [0.0, 1.0]");
		}
		NoteEvent copy = *this;
		copy.velocity = (newVelocity == 0.0f) ? 0.0f : newVelocity;
		return copy;
	}

	[[nodiscard]] constexpr NoteEvent withClampedVelocity(float newVelocity) const {
		NoteEvent copy = *this;
		copy.velocity = clampVelocity(newVelocity);
		return copy;
	}

	[[nodiscard]] constexpr NoteEvent withVoice(std::uint8_t newVoice) const noexcept {
		NoteEvent copy = *this;
		copy.voice = newVoice;
		return copy;
	}

	[[nodiscard]] constexpr NoteEvent withId(std::optional<EventId> newId) const noexcept {
		NoteEvent copy = *this;
		copy.id = newId;
		return copy;
	}

	[[nodiscard]] constexpr NoteEvent shifted(const Rational& delta) const {
		NoteEvent copy = *this;
		copy.span = span.shift(delta);
		return copy;
	}

	/// Returns a copy with finite velocity clamped to [0.0f, 1.0f] and -0.0f canonicalized.
	[[nodiscard]] constexpr NoteEvent normalized() const {
		NoteEvent copy = *this;
		copy.velocity = clampVelocity(velocity);
		return copy;
	}

	// -------------------------------------------------------------------------
	// Formatting & Comparisons
	// -------------------------------------------------------------------------
	[[nodiscard]] std::string toString() const;

	[[nodiscard]] constexpr bool operator==(const NoteEvent& rhs) const noexcept = default;

	/// Orders by (start, voice, pitch, duration, velocity, id) for deterministic Sequence storage.
	[[nodiscard]] constexpr std::partial_ordering operator<=>(const NoteEvent& rhs) const noexcept {
		if (const auto cmp = span.start() <=> rhs.span.start(); cmp != 0) {
			return cmp;
		}
		if (const auto cmp = voice <=> rhs.voice; cmp != 0) {
			return cmp;
		}
		if (const auto cmp = comparePitch(pitch, rhs.pitch); cmp != 0) {
			return cmp;
		}
		if (const auto cmp = span.duration() <=> rhs.span.duration(); cmp != 0) {
			return cmp;
		}
		if (const auto cmp = velocity <=> rhs.velocity; cmp != 0) {
			return cmp;
		}
		return id <=> rhs.id;
	}

	[[nodiscard]] static constexpr std::strong_ordering comparePitch(
		const Pitch& lhs,
		const Pitch& rhs) noexcept {
		const std::int64_t lhsNote = pitchMidiNote64(lhs);
		const std::int64_t rhsNote = pitchMidiNote64(rhs);
		if (const auto cmp = lhsNote <=> rhsNote; cmp != 0) {
			return cmp;
		}

		const float lhsCents = pitchCentOffset(lhs);
		const float rhsCents = pitchCentOffset(rhs);
		if (lhsCents < rhsCents) {
			return std::strong_ordering::less;
		}
		if (lhsCents > rhsCents) {
			return std::strong_ordering::greater;
		}

		if (const auto cmp = lhs.index() <=> rhs.index(); cmp != 0) {
			return cmp;
		}

		if (lhs.index() == 0) {
			return std::get<0>(lhs) <=> std::get<0>(rhs);
		}
		return std::get<1>(lhs) <=> std::get<1>(rhs);
	}

private:
	[[nodiscard]] static constexpr std::int64_t pitchMidiNote64(const Pitch& p) noexcept {
		if (const auto* spelled = std::get_if<SpelledPitch>(&p)) {
			return static_cast<std::int64_t>(spelled->midiNote());
		}
		return static_cast<std::int64_t>(std::get<MidiPitch>(p).noteNumber());
	}

	[[nodiscard]] static constexpr float pitchCentOffset(const Pitch& p) noexcept {
		if (const auto* midi = std::get_if<MidiPitch>(&p)) {
			return midi->centOffset();
		}
		return 0.0f;
	}
};

std::ostream& operator<<(std::ostream& os, const NoteEvent& event);

} // namespace duey

template <>
struct std::hash<duey::NoteEvent> {
	[[nodiscard]] std::size_t operator()(const duey::NoteEvent& event) const noexcept {
		auto combine = [](std::size_t& seed, std::size_t value) noexcept {
			seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
		};

		const float canonicalVelocity = (event.velocity == 0.0f) ? 0.0f : event.velocity;

		std::size_t seed = std::hash<duey::Rational> {}(event.span.start());
		combine(seed, std::hash<duey::Rational> {}(event.span.duration()));
		combine(seed, std::hash<duey::NoteEvent::Pitch> {}(event.pitch));
		combine(seed, std::hash<float> {}(canonicalVelocity));
		combine(seed, std::hash<std::uint8_t> {}(event.voice));
		combine(seed, std::hash<std::optional<duey::EventId>> {}(event.id));
		return seed;
	}
};
