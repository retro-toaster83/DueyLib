
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <limits>
#include <stdexcept>
#include <string>

namespace duey {

/**
 * @brief MIDI-projected pitch with sub-semitone cent deviation.
 *
 * Invariants:
 *  - noteNumber() is always in [0, 127].
 *  - centOffset() is always finite and normalized to [-50.0f, 50.0f), with -0.0f
 *    canonicalized to +0.0f.
 *
 * By convention, 60 represents C4 (middle C). SpelledPitch is canonical;
 * MidiPitch is a tuning-projected representation where enharmonics coincide.
 */
class MidiPitch {
public:
	static constexpr std::uint8_t kMinNoteNumber = 0;
	static constexpr std::uint8_t kMaxNoteNumber = 127;
	static constexpr float kCentsPerSemitone = 100.0f;
	static constexpr float kMinCentOffset = -50.0f;
	static constexpr float kMaxCentOffset = 50.0f; // exclusive upper bound

	constexpr MidiPitch() noexcept = default;

	constexpr explicit MidiPitch(int noteNumber, float centOffset = 0.0f) {
		constexpr float kMaxFloat = std::numeric_limits<float>::max();
		if (!(centOffset >= -kMaxFloat && centOffset <= kMaxFloat)) {
			throw std::invalid_argument("MidiPitch centOffset must be finite");
		}
		*this = normalize(noteNumber, static_cast<double>(centOffset));
	}

	/// Constructs a MidiPitch from continuous 12-TET semitones (e.g. 60.25 -> 60 + 25c).
	[[nodiscard]] static constexpr MidiPitch fromSemitones(double semitones) {
		constexpr double kMaxDouble = std::numeric_limits<double>::max();
		if (!(semitones >= -kMaxDouble && semitones <= kMaxDouble)) {
			throw std::invalid_argument("MidiPitch semitones must be finite");
		}
		if (semitones < -1.0 || semitones > 128.0) {
			throw std::out_of_range("MidiPitch semitones out of [0, 127] MIDI range");
		}

		const double shifted = semitones + 0.5;
		auto baseNote = static_cast<std::int64_t>(shifted);
		if (shifted < 0.0 && static_cast<double>(baseNote) != shifted) {
			--baseNote;
		}
		const double cents = (semitones - static_cast<double>(baseNote)) * 100.0;
		return normalize(baseNote, cents);
	}

	// -------------------------------------------------------------------------
	// Observers
	// -------------------------------------------------------------------------
	[[nodiscard]] constexpr std::uint8_t noteNumber() const noexcept { return noteNumber_; }
	[[nodiscard]] constexpr float centOffset() const noexcept { return centOffset_; }

	/// Returns the continuous semitone value (e.g. 61 + 30c -> 61.3).
	[[nodiscard]] constexpr double toSemitones() const noexcept {
		return static_cast<double>(noteNumber_) + static_cast<double>(centOffset_) / 100.0;
	}

	/// Returns true iff centOffset() is exactly 0.
	[[nodiscard]] constexpr bool isExactNote() const noexcept { return centOffset_ == 0.0f; }

	/// Returns the nearest 12-TET MIDI note with zero cent offset.
	[[nodiscard]] constexpr MidiPitch nearestNote() const noexcept {
		return MidiPitch { noteNumber_, 0.0f, NormalizedTag {} };
	}

	// -------------------------------------------------------------------------
	// Operations
	// -------------------------------------------------------------------------
	[[nodiscard]] constexpr MidiPitch transpose(int semitones) const {
		return normalize(static_cast<std::int64_t>(noteNumber_) + semitones, static_cast<double>(centOffset_));
	}

	[[nodiscard]] constexpr MidiPitch transposeCents(float cents) const {
		constexpr float kMaxFloat = std::numeric_limits<float>::max();
		if (!(cents >= -kMaxFloat && cents <= kMaxFloat)) {
			throw std::invalid_argument("MidiPitch cent delta must be finite");
		}
		return normalize(noteNumber_, static_cast<double>(centOffset_) + static_cast<double>(cents));
	}

	/// Signed distance (other - *this) in cents.
	[[nodiscard]] constexpr double centsTo(const MidiPitch& other) const noexcept {
		const std::int64_t noteDiff =
			static_cast<std::int64_t>(other.noteNumber_) - static_cast<std::int64_t>(noteNumber_);
		return static_cast<double>(noteDiff) * 100.0 +
			(static_cast<double>(other.centOffset_) - static_cast<double>(centOffset_));
	}

	/// Signed distance (other - *this) in semitones.
	[[nodiscard]] constexpr double semitonesTo(const MidiPitch& other) const noexcept {
		return centsTo(other) / 100.0;
	}

	/// Returns true iff the pitch difference is within centTolerance cents.
	[[nodiscard]] constexpr bool approxEqual(const MidiPitch& other, float centTolerance = 1e-3f) const noexcept {
		const double diff = centsTo(other);
		const double absDiff = (diff < 0.0) ? -diff : diff;
		return absDiff <= static_cast<double>(centTolerance);
	}

	// -------------------------------------------------------------------------
	// Formatting & Comparisons
	// -------------------------------------------------------------------------
	[[nodiscard]] std::string toString() const;

	[[nodiscard]] constexpr bool operator==(const MidiPitch& rhs) const noexcept = default;

	[[nodiscard]] constexpr std::strong_ordering operator<=>(const MidiPitch& rhs) const noexcept {
		if (const auto cmp = noteNumber_ <=> rhs.noteNumber_; cmp != 0) {
			return cmp;
		}
		if (centOffset_ < rhs.centOffset_) {
			return std::strong_ordering::less;
		}
		if (centOffset_ > rhs.centOffset_) {
			return std::strong_ordering::greater;
		}
		return std::strong_ordering::equal;
	}

private:
	struct NormalizedTag {};

	constexpr MidiPitch(std::uint8_t noteNumber, float centOffset, NormalizedTag) noexcept
		: noteNumber_(noteNumber), centOffset_(centOffset) {
	}

	[[nodiscard]] static constexpr MidiPitch normalize(std::int64_t noteNumber, double centOffset) {
		const double shifted = (centOffset + 50.0) / 100.0;
		// Reject extreme cent values early before converting double to int64_t.
		if (shifted < -3.0e9 || shifted > 3.0e9) {
			throw std::out_of_range("Normalized MIDI note number must be in [0, 127]");
		}

		auto semitoneShift = static_cast<std::int64_t>(shifted);
		if (shifted < 0.0 && static_cast<double>(semitoneShift) != shifted) {
			--semitoneShift;
		}

		std::int64_t adjustedNote = noteNumber + semitoneShift;
		const double rem = centOffset - static_cast<double>(semitoneShift) * 100.0;
		auto finalCents = static_cast<float>(rem);

		// Guard against double-to-float rounding pushing values onto/past +/-50.0f.
		if (finalCents >= kMaxCentOffset) {
			finalCents -= kCentsPerSemitone;
			++adjustedNote;
		} else if (finalCents < kMinCentOffset) {
			finalCents += kCentsPerSemitone;
			--adjustedNote;
		}

		if (finalCents == 0.0f) {
			finalCents = 0.0f; // Canonicalize -0.0f to +0.0f.
		}

		if (adjustedNote < kMinNoteNumber || adjustedNote > kMaxNoteNumber) {
			throw std::out_of_range("Normalized MIDI note number must be in [0, 127]");
		}

		return MidiPitch {
			static_cast<std::uint8_t>(adjustedNote),
			finalCents,
			NormalizedTag {}
		};
	}

	std::uint8_t noteNumber_ { 0 };
	float centOffset_ { 0.0f };
};

std::ostream& operator<<(std::ostream& os, const MidiPitch& pitch);

} // namespace duey

template <>
struct std::hash<duey::MidiPitch> {
	[[nodiscard]] std::size_t operator()(const duey::MidiPitch& pitch) const noexcept {
		const std::size_t h1 = std::hash<std::uint8_t> {}(pitch.noteNumber());
		const std::size_t h2 = std::hash<float> {}(pitch.centOffset());
		return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
	}
};
