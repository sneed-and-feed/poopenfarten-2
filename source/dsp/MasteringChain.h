#pragma once

#include "DspMath.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

enum class DimensionChorusMode : int {
    ModeI   = 0, // Subtle spatial expansion (rate 0.40 Hz, depth 1.5 ms)
    ModeII  = 1, // Gentle acoustic warmth (rate 0.55 Hz, depth 2.2 ms)
    ModeIII = 2, // Deep visceral ensemble (rate 0.75 Hz, depth 3.2 ms)
    ModeIV  = 3  // Dynamic all-buttons swirl (rate 1.10 Hz, depth 4.5 ms)
};

struct MasteringChainParams {
    bool                dimensionDEnable       { true };
    DimensionChorusMode dimensionDMode         { DimensionChorusMode::ModeII };
    float               dimensionDMix          { 40.0f }; // [0.0, 100.0] %
    float               dimensionDSpread       { 1.0f };  // Stereo width [0.0, 1.5]
    float               colonicSaturationDrive { 30.0f }; // Harmonic overdrive [0.0, 100.0] %
    float               visceralSubReinforceDb { 0.0f };  // Sub-bass boost [-36.0, +6.0] dB
    float               masterVolumeDb         { 0.0f };  // Master output trim [-36.0, +6.0] dB
};

/**
 * MasteringChain: High-end analog-modeled output stage.
 *
 * Implements:
 * 1. Roland Dimension D tri-phase BBD chorus (0 deg, 120 deg, 240 deg)
 *    with NE570 compander emulation and guaranteed mono-sum immunity (|Wet_mono| > 0).
 * 2. 15 Hz infrasonic DC blocker (2nd-order TPT highpass).
 * 3. Phase-locked visceral sub-bass displacement reinforcement.
 * 4. C^1 Hermite soft-knee BoundedSaturator with strict ceiling containment (<= 1.05 peak).
 * 5. True-peak brickwall ceiling limiter.
 */
class MasteringChain {
public:
    static constexpr size_t kDelayCapacity = 4096;
    static constexpr float  kPeakCeiling = 1.05f; // Absolute brickwall output ceiling

    MasteringChain() noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Process a single stereo sample pair in-place
    void process(
        float inL,
        float inR,
        const MasteringChainParams& params,
        float& outL,
        float& outR) noexcept;

    // Process stereo buffer in-place
    void processBlock(
        float* bufferL,
        float* bufferR,
        int numSamples,
        const MasteringChainParams& params) noexcept;

    [[nodiscard]] float getPeakLevelDb() const noexcept { return mPeakLevelDb; }
    [[nodiscard]] float getRmsLevelDb() const noexcept { return mRmsLevelDb; }

private:
    void updateChorusSettings(DimensionChorusMode mode) noexcept;

    // Compander 1-pole high-shelf pre-emphasis filter (+6 dB above 1.8 kHz)
    struct CompanderShelf {
        float b0 { 1.0f };
        float b1 { 0.0f };
        float a1 { 0.0f };
        float state { 0.0f };

        void reset() noexcept { state = 0.0f; }

        [[nodiscard]] inline float process(float x) noexcept {
            const float y = b0 * x + state;
            state = flushDenormal(b1 * x - a1 * y);
            return flushDenormal(y);
        }
    };

    // Compander de-emphasis filter (-6 dB above 1.8 kHz)
    struct CompanderDeemphasis {
        float b0 { 1.0f };
        float b1 { 0.0f };
        float a1 { 0.0f };
        float state { 0.0f };

        void reset() noexcept { state = 0.0f; }

        [[nodiscard]] inline float process(float x) noexcept {
            const float y = b0 * x + state;
            state = flushDenormal(b1 * x - a1 * y);
            return flushDenormal(y);
        }
    };

    // 2-pole Butterworth reconstruction lowpass filter (9.5 kHz)
    struct ReconstructionLowpass {
        float b0 { 1.0f };
        float b1 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float s1 { 0.0f };
        float s2 { 0.0f };

        void reset() noexcept { s1 = 0.0f; s2 = 0.0f; }

        [[nodiscard]] inline float process(float x) noexcept {
            const float y = b0 * x + s1;
            s1 = flushDenormal(b1 * x - a1 * y + s2);
            s2 = flushDenormal(b2 * x - a2 * y);
            return flushDenormal(y);
        }
    };

    // 2-pole TPT Highpass for 15 Hz DC blocking
    class TptHighpass15Hz {
    public:
        void prepare(float sampleRate) noexcept {
            const float fs = (sampleRate > 100.0f) ? sampleRate : 48000.0f;
            const float g = std::tan(kPi * (15.0f / fs));
            constexpr float k = 1.41421356f; // Butterworth Q = 0.7071
            mA1 = 1.0f / (1.0f + g * (g + k));
            mG = g;
            mK = k;
            reset();
        }

        void reset() noexcept {
            mS1 = 0.0f;
            mS2 = 0.0f;
        }

        [[nodiscard]] inline float process(float x) noexcept {
            const float vHp = mA1 * (x - (mK + mG) * mS1 - mS2);
            const float vBp = mG * vHp + mS1;
            const float vLp = mG * vBp + mS2;
            mS1 = flushDenormal(2.0f * vBp - mS1);
            mS2 = flushDenormal(2.0f * vLp - mS2);
            return flushDenormal(vHp);
        }

    private:
        float mA1 { 1.0f };
        float mG  { 0.001f };
        float mK  { 1.414f };
        float mS1 { 0.0f };
        float mS2 { 0.0f };
    };

    // Visceral Sub-bass Reinforcement Resonator (38 Hz tuned lowpass/sub harmonic)
    class VisceralSubResonator {
    public:
        void prepare(float sampleRate) noexcept {
            const float fs = (sampleRate > 100.0f) ? sampleRate : 48000.0f;
            const float g = std::tan(kPi * (38.0f / fs));
            constexpr float k = 0.90f; // Controlled Q = 1.11 warm sub reinforcement without resonant peaking
            mA1 = 1.0f / (1.0f + g * (g + k));
            mG = g;
            mK = k;
            reset();
        }

        void reset() noexcept {
            mS1 = 0.0f;
            mS2 = 0.0f;
        }

        [[nodiscard]] inline float process(float x) noexcept {
            const float vHp = mA1 * (x - (mK + mG) * mS1 - mS2);
            const float vBp = mG * vHp + mS1;
            const float vLp = mG * vBp + mS2;
            mS1 = flushDenormal(2.0f * vBp - mS1);
            mS2 = flushDenormal(2.0f * vLp - mS2);
            // Return resonant lowpass + bandpass body
            return flushDenormal(vLp + vBp * 0.35f);
        }

    private:
        float mA1 { 1.0f };
        float mG  { 0.002f };
        float mK  { 0.90f };
        float mS1 { 0.0f };
        float mS2 { 0.0f };
    };

    float mSampleRate { 48000.0f };

    // Roland Dimension D 3-Phase BBD State
    FixedDelayLine<kDelayCapacity> mBbd1;
    FixedDelayLine<kDelayCapacity> mBbd2;
    FixedDelayLine<kDelayCapacity> mBbd3;
    float mLfoPhase { 0.0f };
    float mLfoInc   { 0.001f };
    float mRateHz   { 0.55f };
    float mDepthMs  { 2.2f };
    float mBaseDelaySamples { 264.0f }; // ~5.5 ms

    // Compander filters
    CompanderShelf       mPreShelf;
    ReconstructionLowpass mReconLp1, mReconLp2, mReconLp3;
    CompanderDeemphasis  mDeemph1, mDeemph2, mDeemph3;

    // Sub & DC filters
    TptHighpass15Hz      mDcBlockL;
    TptHighpass15Hz      mDcBlockR;
    VisceralSubResonator mSubResonator;

    // Telemetry ballistics
    float mPeakHold { 0.0f };
    float mRmsAcc   { 0.0f };
    float mPeakLevelDb { -100.0f };
    float mRmsLevelDb  { -100.0f };
};

} // namespace pf02::dsp
