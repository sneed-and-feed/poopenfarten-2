#pragma once

#include "../dsp/DspMath.h"
#include "../dsp/MucosalExciter.h"
#include "../dsp/AeroFluidEngine.h"
#include "../dsp/ModalResonatorMatrix.h"
#include "../parameters/Parameters.h"

#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace pf02::engine {

/**
 * PhysicalVoice: Single-voice coupled physical-modeling pipeline.
 *
 * Couples:
 * 1. MucosalExciter: Karnopp stick-slip friction, Stribeck breakaway, Hunt-Crossley impact, Buchla 292 sag.
 * 2. AeroFluidEngine: Beltrami vortex jet, Minnaert bubbles, Poisson droplets, Bjorklund sequencer.
 * 3. ModalResonatorMatrix: 16-pole Zavalishin TPT SVF, Householder scattering, DarkSnail TriadCascade.
 * 4. Local Vactrol envelope gate and dynamic frequency tracking / portamento glide.
 *
 * Guarantees:
 * - Exactly zero audio-thread heap allocations.
 * - Zero lookahead latency.
 * - Branchless IEEE 754 denormal flushing on all state variables.
 */
class PhysicalVoice {
public:
    PhysicalVoice() noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Note lifecycle
    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(float velocity = 0.0f) noexcept;
    void killQuick() noexcept; // Immediate de-click kill for voice stealing

    // Parameter distribution
    void setParameter(parameters::ParameterId id, float value) noexcept;
    void setParameter(std::string_view idStr, float value) noexcept;

    // Pitch control & Portamento
    void setTargetPitchMidi(float midiNote, bool immediate = false) noexcept;
    void setGlideTimeMs(float glideMs) noexcept;
    void setPitchBendOffset(float semitones) noexcept;

    // Modulation from Lorenz attractor
    void setLorenzModulation(
        const std::array<float, 16>& freqMultipliers,
        float qSpreadFactor,
        float panOffsetRad) noexcept;

    // Audio rendering
    // Returns mono sum of left and right
    [[nodiscard]] float processSample(float externalPressureMod = 0.0f) noexcept;

    // Stereo rendering
    void processSample(float& outL, float& outR, float externalPressureMod = 0.0f) noexcept;

    // Status queries
    [[nodiscard]] bool isActive() const noexcept { return mActive; }
    [[nodiscard]] bool isNoteHeld() const noexcept { return mGateState == GateState::NoteOn; }
    [[nodiscard]] int getMidiNote() const noexcept { return mMidiNote; }
    [[nodiscard]] float getVelocity() const noexcept { return mVelocity; }
    [[nodiscard]] uint64_t getOnsetTime() const noexcept { return mOnsetTime; }
    [[nodiscard]] float getEnvelopeLevel() const noexcept { return mEnvelope; }
    [[nodiscard]] float getAperture() const noexcept { return mMucosalExciter.getAperture(); }
    [[nodiscard]] float getColonicPressure() const noexcept { return mMucosalExciter.getColonicPressure(); }
    [[nodiscard]] float getAirVelocity() const noexcept { return mMucosalExciter.getAirVelocity(); }
    [[nodiscard]] const std::array<float, 16>& getModalAmplitudes() const noexcept {
        return mModalMatrix.getModalEnergies();
    }
    [[nodiscard]] const std::array<float, 16>& getTriadEnergyFlow() const noexcept {
        return mModalMatrix.getTriadEnergyFlow();
    }
    [[nodiscard]] float getBubbleActivity() const noexcept { return mAeroFluidEngine.getBubbleActivity(); }
    [[nodiscard]] bool wasDropletTriggered() const noexcept { return mAeroFluidEngine.wasDropletTriggered(); }
    [[nodiscard]] float getLastRadialDrift() const noexcept { return mModalMatrix.getLastRadialDrift(); }

private:
    enum class GateState {
        Off,
        NoteOn,
        Release
    };

    void updateFrequencyGlide() noexcept;

    float mSampleRate { 48000.0f };
    bool  mActive { false };
    GateState mGateState { GateState::Off };

    int      mMidiNote { -1 };
    float    mVelocity { 0.0f };
    uint64_t mOnsetTime { 0 };

    // Frequency & Portamento
    float mCurrentFreqHz { 55.0f };
    float mTargetFreqHz { 55.0f };
    float mBaseMidiNote { 33.0f }; // A1
    float mTargetMidiNote { 33.0f };
    float mPitchBendOffsetSemitones { 0.0f };
    float mGlideTimeMs { 45.0f };
    float mGlideCoeff { 1.0f };

    // Vactrol envelope gate
    float mEnvelope { 0.0f };
    float mAttackCoeff { 0.05f };
    float mReleaseCoeff { 0.999f };

    // Stealing de-click crossfade
    bool  mStealingFade { false };
    float mStealFadeGain { 1.0f };
    float mStealFadeStep { 0.005f }; // 5 ms fade-out

    // Core Physical Subsystems
    dsp::MucosalExciter       mMucosalExciter;
    dsp::AeroFluidEngine      mAeroFluidEngine;
    dsp::ModalResonatorMatrix mModalMatrix;

    // Subsystem parameter structures
    dsp::MucosalExciterParams mMucosalParams {};
    dsp::AeroFluidParams      mFluidParams {};
    dsp::ModalMatrixParams    mModalParams {};
};

} // namespace pf02::engine

namespace pf02::dsp {
    using PhysicalVoice = pf02::engine::PhysicalVoice;
}
