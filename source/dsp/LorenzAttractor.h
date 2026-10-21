#pragma once

#include "DspMath.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

/**
 * LorenzAttractor: 3D Continuous Chaotic Strange Attractor Engine.
 *
 * Implements:
 * 1. Lorenz differential equations:
 *    dx/dt = sigma * (y - x)
 *    dy/dt = x * (rho - z) - y
 *    dz/dt = x * y - beta * z
 * 2. Classical 4th-Order Runge-Kutta (RK4) numerical integration.
 * 3. Anti-blowup safety guard with automatic canonical re-seeding.
 * 4. Chaotic modulation generator for 16-mode odd/even detuning, Q-spread, and spatial panning.
 * 5. Telemetry interface for CRT Mode 3 3D Attractor projection.
 */
class LorenzAttractor {
public:
    static constexpr size_t kNumModes = 16;
    static constexpr float kSigma = 10.0f;
    static constexpr float kRho   = 28.0f;
    static constexpr float kBeta  = 8.0f / 3.0f; // 2.666667

    LorenzAttractor() noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setRateHz(float rateHz) noexcept;
    void setDepth(float depth) noexcept;          // 0.0 to 1.0 overall chaos depth
    void setDetuneMaxCents(float cents) noexcept; // 0 to 120 cents
    void setQModDepth(float qMod) noexcept;       // 0.0 to 1.0 Q spread modulation depth

    // Advances the chaotic attractor state by one sample
    void step() noexcept;

    // Direct state coordinates
    [[nodiscard]] float getX() const noexcept { return mX; }
    [[nodiscard]] float getY() const noexcept { return mY; }
    [[nodiscard]] float getZ() const noexcept { return mZ; }

    // Normalized coordinates in [-1, +1] (or [0, 1] for Z)
    [[nodiscard]] float getNormalizedX() const noexcept { return mNormX; }
    [[nodiscard]] float getNormalizedY() const noexcept { return mNormY; }
    [[nodiscard]] float getNormalizedZ() const noexcept { return mNormZ; }

    // 16-mode frequency ratio multipliers based on twin-lobe flipping
    [[nodiscard]] const std::array<float, kNumModes>& getFrequencyMultipliers() const noexcept {
        return mFreqMultipliers;
    }

    // Modal Q-factor spread modulation factor
    [[nodiscard]] float getQSpreadFactor() const noexcept { return mQSpread; }

    // Spatial panning angle offset in radians
    [[nodiscard]] float getSpatialPanOffset() const noexcept { return mPanOffsetRad; }

private:
    struct StateDeriv {
        float dx { 0.0f };
        float dy { 0.0f };
        float dz { 0.0f };
    };

    [[nodiscard]] static inline StateDeriv evaluateDeriv(float x, float y, float z) noexcept {
        return {
            kSigma * (y - x),
            x * (kRho - z) - y,
            x * y - kBeta * z
        };
    }

    void updateNormalizedOutputs() noexcept;

    float mSampleRate { 48000.0f };
    float mRateHz { 0.65f };
    float mDt { 0.005f };
    float mDepth { 0.25f };
    float mMaxDetuneCents { 45.0f };
    float mQModDepth { 0.50f };

    // Attractor physical state
    float mX { 0.10f };
    float mY { 0.0f };
    float mZ { 0.0f };

    // Normalized telemetry outputs
    float mNormX { 0.0f };
    float mNormY { 0.0f };
    float mNormZ { 0.0f };

    // Derived modulations
    std::array<float, kNumModes> mFreqMultipliers {};
    float mQSpread { 1.0f };
    float mPanOffsetRad { 0.0f };
};

} // namespace pf02::dsp
