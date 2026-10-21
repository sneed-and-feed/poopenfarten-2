#pragma once

#include "PhysicalVoice.h"
#include "../parameters/Parameters.h"

#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace pf02::engine {

enum class NotePriority {
    Last,
    Low,
    High
};

struct NoteEntry {
    int   midiNote { -1 };
    float velocity { 0.0f };
    float freqHz   { 0.0f };
};

/**
 * VoiceManager: High-performance polyphonic physical voice allocator.
 *
 * Implements:
 * 1. 8-Voice Polyphonic Allocation with lowest-velocity / oldest voice stealing
 *    coupled with a 5 ms de-click crossfade.
 * 2. Switchable Mono-Legato mode with note priority stack (Last, Low, High)
 *    and smooth portamento pitch glide.
 * 3. Stereo Unison mode: 4 voices panned across the stereo field with
 *    detune offsets (+/-7, +/-14 cents) and phase decorrelation.
 * 4. Thread-safe lock-free parameter distribution across all voices.
 *
 * Guarantees:
 * - Exactly zero audio-thread heap allocations.
 * - Zero lookahead latency.
 */
class VoiceManager {
public:
    static constexpr size_t kMaxVoices = 8;
    static constexpr size_t kMaxNoteStack = 32;

    VoiceManager() noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Voicing mode & priority
    void setVoicingMode(parameters::VoicingMode mode) noexcept;
    void setNotePriority(NotePriority priority) noexcept;
    void setPortamentoGlideMs(float glideMs) noexcept;
    void setPitchBend(float semitones) noexcept;

    // MIDI Lifecycle
    void noteOn(int midiNote, float velocity, float freqHz = 0.0f) noexcept;
    void noteOff(int midiNote, float velocity = 0.0f) noexcept;
    void allNotesOff() noexcept;
    void triggerKineticBurst(std::string_view burstType, float velocity = 1.0f) noexcept;

    // Parameter distribution
    void setParameter(parameters::ParameterId id, float value) noexcept;
    void setParameter(std::string_view idStr, float value) noexcept;

    // Lorenz Attractor Modulation Distribution
    void setLorenzModulation(
        const std::array<float, 16>& freqMultipliers,
        float qSpreadFactor,
        float panOffsetRad) noexcept;

    // Audio Block Rendering (Stereo accumulation)
    void processBlock(
        float* bufferL,
        float* bufferR,
        int numSamples,
        float externalPressureMod = 0.0f) noexcept;

    // Voice & Telemetry queries
    [[nodiscard]] size_t getActiveVoiceCount() const noexcept;
    [[nodiscard]] float getActiveAperture() const noexcept;
    [[nodiscard]] float getActiveColonicPressure() const noexcept;
    [[nodiscard]] float getActiveAirVelocity() const noexcept;
    [[nodiscard]] float getActiveBubbleIntensity() const noexcept;
    [[nodiscard]] bool wasDropletTriggered() const noexcept;
    [[nodiscard]] std::array<float, 16> getModalEnergies() const noexcept;
    [[nodiscard]] std::array<float, 16> getTriadEnergyFlow() const noexcept;
    [[nodiscard]] float getLastRadialDrift() const noexcept;

private:
    // Polyphonic Voice Stealing Helper
    size_t findVoiceToSteal() noexcept;

    // Mono-Legato Note Stack Management
    void pushNoteStack(int midiNote, float velocity, float freqHz = 0.0f) noexcept;
    void removeNoteStack(int midiNote) noexcept;
    [[nodiscard]] int getPriorityNote(float& outVelocity, float& outFreqHz) const noexcept;

    // Subsystem state
    float mSampleRate { 48000.0f };
    parameters::VoicingMode mVoicingMode { parameters::VoicingMode::MonoLegato };
    NotePriority mNotePriority { NotePriority::Last };
    float mGlideTimeMs { 45.0f };
    float mPitchBendSemitones { 0.0f };

    uint64_t mClock { 0 };

    // 8 Pre-allocated Physical Voices
    std::array<PhysicalVoice, kMaxVoices> mVoices;

    // Mono-Legato Note Stack (Fixed-capacity array, zero allocations)
    std::array<NoteEntry, kMaxNoteStack> mNoteStack {};
    size_t mNoteStackCount { 0 };

    // Unison Detune Offsets (+/-14, +/-7 cents)
    static inline constexpr std::array<float, 4> kUnisonDetuneCents = {
        -14.0f, -7.0f, +7.0f, +14.0f
    };
    static inline constexpr std::array<float, 4> kUnisonPanOffsets = {
        -0.80f, -0.30f, +0.30f, +0.80f
    };
};

} // namespace pf02::engine
