#pragma once

#include "DspMath.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

struct AeroFluidParams {
    float beltramiVortexPitch   { 2.8f };   // Vortex wavenumber lambda [0.1, 10.0] rad/m
    float reynoldsTurbulence    { 0.40f };  // Jet shear & flutter [0.0, 1.0]
    float multiphaseMoisture    { 0.35f };  // Fluid moisture [0.0, 1.0]
    float fluidViscosity        { 0.30f };  // Bingham plastic viscosity [0.0, 1.0]
    float minnaertBubbleDensity { 40.0f };  // Bubble density [0.0, 100.0] %
    float poissonDropletRate    { 8.5f };   // Droplet pop rate [0.0, 60.0] Hz
    int   euclideanPeristalsisK { 5 };      // Active pulses k [1, 16]
    int   euclideanPeristalsisN { 16 };     // Resolution steps n [1, 16]
    float tempoBpm              { 120.0f }; // Peristaltic motor tempo [30.0, 300.0]
};

/**
 * AeroFluidEngine: Multiphase hydro-elastodynamic and aeroacoustic engine.
 *
 * Implements:
 * 1. Helical Beltrami vortex jet (curl v = lambda * v) with Reynolds flutter.
 * 2. Minnaert resonant gas-liquid bubble bank (f = 3.28325 / R).
 * 3. Poisson point-process stochastic droplet clock (Delta t = -ln(1-U)/lambda)
 *    with Irwin-Hall droplet mass.
 * 4. Bjorklund Euclidean peristaltic sequencer E(k, n).
 */
class AeroFluidEngine {
public:
    static constexpr size_t kMaxBubbleVoices = 16;
    static constexpr size_t kMaxDropletFilters = 4;

    AeroFluidEngine() noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Process single audio sample:
    // takes air velocity and aperture from mucosal exciter, plus external Beltrami flux telemetry
    [[nodiscard]] float processSample(
        float airVelocity,
        float aperture,
        float zenoFlux,
        const AeroFluidParams& params) noexcept;

    // Direct manual triggers for bubbles, droplets, and peristalsis
    void triggerBubble(float radiusMm, float intensity, float chirpRate = 1.00018f) noexcept;
    void triggerDroplet(float intensity) noexcept;
    void triggerPeristalticPulse(float intensity) noexcept;
    void triggerFlapBurst(float openingIntensity, float moisture, float dropletRate) noexcept;
    void setSeed(uint64_t seed) noexcept { mBaseSeed = seed; mPrng.setSeed(seed); }

    // Telemetry getters
    [[nodiscard]] float getBubbleActivity() const noexcept { return mBubbleActivity; }
    [[nodiscard]] bool  wasDropletTriggered() const noexcept { return mDropletTriggered; }
    [[nodiscard]] float getLastDropletAmplitude() const noexcept { return mLastDropletAmp; }
    [[nodiscard]] float getReynoldsNumber() const noexcept { return mReynoldsNumber; }
    [[nodiscard]] float getVortexWhistleLevel() const noexcept { return mVortexWhistleLevel; }
    [[nodiscard]] bool  wasEuclideanStepTriggered() const noexcept { return mEuclideanStepTriggered; }

private:
    struct BubbleVoice {
        bool  active      { false };
        float phase       { 0.0f };
        float phaseInc    { 0.0f };
        float chirp       { 1.00016f };
        float envelope    { 0.0f };
        float attackGain  { 0.0f };
        float decayRate   { 0.98f };
        float frequencyHz { 1500.0f };
    };

    // Biquad Direct-Form II Transposed Filter for droplet resonant pops
    struct BiquadDropletFilter {
        float b0 { 0.0f };
        float b1 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float s1 { 0.0f };
        float s2 { 0.0f };

        void reset() noexcept { s1 = 0.0f; s2 = 0.0f; }

        void setBandpass(float sampleRate, float freqHz, float q) noexcept {
            const float fs = (sampleRate > 100.0f) ? sampleRate : 48000.0f;
            const float fc = std::clamp(freqHz, 50.0f, fs * 0.48f);
            const float w0 = kTwoPi * (fc / fs);
            const float sinW0 = FastSinTable::sin(w0);
            const float cosW0 = FastSinTable::cos(w0);
            const float safeQ = std::max(0.1f, q);
            const float alpha = sinW0 / (2.0f * safeQ);

            const float a0 = 1.0f + alpha;
            const float invA0 = 1.0f / a0;

            b0 = (sinW0 * 0.5f) * invA0;
            b1 = 0.0f;
            b2 = (-sinW0 * 0.5f) * invA0;
            a1 = (-2.0f * cosW0) * invA0;
            a2 = (1.0f - alpha) * invA0;
        }

        [[nodiscard]] inline float process(float x) noexcept {
            const float y = b0 * x + s1;
            s1 = flushDenormal(b1 * x - a1 * y + s2);
            s2 = flushDenormal(b2 * x - a2 * y);
            return flushDenormal(y);
        }
    };

    void updatePoissonClock(float rateHz) noexcept;
    void updateEuclideanStep(const AeroFluidParams& p) noexcept;

    float mSampleRate { 48000.0f };
    uint64_t mBaseSeed { 0x4145524F464C55ULL };
    FastPRNG mPrng { 0x4145524F464C55ULL }; // "AEROFLU"

    // 1. Helical Beltrami Vortex Jet & Pink Noise State
    float mPinkB0 { 0.0f };
    float mPinkB1 { 0.0f };
    float mPinkB2 { 0.0f };
    float mVortexPhase { 0.0f };
    float mReynoldsNumber { 0.0f };
    float mVortexWhistleLevel { 0.0f };

    // 2. Minnaert Bubble Resonator Bank (16 voices)
    std::array<BubbleVoice, kMaxBubbleVoices> mBubbleVoices {};
    int mBubbleSpawnCountdown { 200 };
    float mBubbleActivity { 0.0f };

    // 3. Poisson Droplet Point-Process Clock
    int mPoissonCountdown { 4000 };
    bool mDropletTriggered { false };
    float mLastDropletAmp { 0.0f };
    std::array<BiquadDropletFilter, kMaxDropletFilters> mDropletFilters {};
    std::array<float, kMaxDropletFilters> mDropletImpulses {};
    size_t mDropletFilterIdx { 0 };

    // 4. Bjorklund Euclidean Sequencer
    float mEuclideanPhase { 0.0f };
    int   mEuclideanCurrentStep { 0 };
    bool  mEuclideanStepTriggered { false };

    // Filters for fluid viscosity acoustic damping
    OnePoleLowpass mViscosityFilter;
    OnePoleLowpass mGurgleFilter;
};

} // namespace pf02::dsp
