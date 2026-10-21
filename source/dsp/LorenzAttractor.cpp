#include "LorenzAttractor.h"
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

LorenzAttractor::LorenzAttractor() noexcept {
    mFreqMultipliers.fill(1.0f);
    reset();
}

void LorenzAttractor::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    reset();
}

void LorenzAttractor::reset() noexcept {
    // Canonical chaotic initial condition
    mX = 0.10f;
    mY = 0.0f;
    mZ = 0.0f;
    mNormX = 0.0f;
    mNormY = 0.0f;
    mNormZ = 0.0f;
    mQSpread = 1.0f;
    mPanOffsetRad = 0.0f;
    mFreqMultipliers.fill(1.0f);
    setRateHz(mRateHz);
    updateNormalizedOutputs();
}

void LorenzAttractor::setRateHz(float rateHz) noexcept {
    mRateHz = std::clamp(rateHz, 0.01f, 15.0f);
    // Bounded RK4 integration step size
    mDt = (mRateHz * 8.0f) / mSampleRate;
    mDt = std::clamp(mDt, 1.0e-5f, 0.005f);
}

void LorenzAttractor::setDepth(float depth) noexcept {
    mDepth = std::clamp(depth, 0.0f, 1.0f);
}

void LorenzAttractor::setDetuneMaxCents(float cents) noexcept {
    mMaxDetuneCents = std::clamp(cents, 0.0f, 150.0f);
}

void LorenzAttractor::setQModDepth(float qMod) noexcept {
    mQModDepth = std::clamp(qMod, 0.0f, 1.0f);
}

void LorenzAttractor::step() noexcept {
    const float h = mDt;

    // Classical 4th-Order Runge-Kutta (RK4)
    const StateDeriv k1 = evaluateDeriv(mX, mY, mZ);
    const StateDeriv k2 = evaluateDeriv(mX + 0.5f * h * k1.dx,
                                        mY + 0.5f * h * k1.dy,
                                        mZ + 0.5f * h * k1.dz);
    const StateDeriv k3 = evaluateDeriv(mX + 0.5f * h * k2.dx,
                                        mY + 0.5f * h * k2.dy,
                                        mZ + 0.5f * h * k2.dz);
    const StateDeriv k4 = evaluateDeriv(mX + h * k3.dx,
                                        mY + h * k3.dy,
                                        mZ + h * k3.dz);

    mX += (h * (1.0f / 6.0f)) * (k1.dx + 2.0f * k2.dx + 2.0f * k3.dx + k4.dx);
    mY += (h * (1.0f / 6.0f)) * (k1.dy + 2.0f * k2.dy + 2.0f * k3.dy + k4.dy);
    mZ += (h * (1.0f / 6.0f)) * (k1.dz + 2.0f * k2.dz + 2.0f * k3.dz + k4.dz);

    mX = flushDenormal(mX);
    mY = flushDenormal(mY);
    mZ = flushDenormal(mZ);

    // Anti-blowup safety guard
    if (!std::isfinite(mX) || !std::isfinite(mY) || !std::isfinite(mZ) ||
        std::abs(mX) > 100.0f || std::abs(mY) > 100.0f || mZ > 150.0f || mZ < -10.0f) [[unlikely]]
    {
        mX = 0.10f;
        mY = 0.0f;
        mZ = 0.0f;
    }

    updateNormalizedOutputs();
}

void LorenzAttractor::updateNormalizedOutputs() noexcept {
    // Lorenz typical bounds: X in [-20, 20], Y in [-25, 25], Z in [0, 50]
    mNormX = std::clamp(mX * (1.0f / 20.0f), -1.0f, 1.0f);
    mNormY = std::clamp(mY * (1.0f / 25.0f), -1.0f, 1.0f);
    mNormZ = std::clamp(mZ * (1.0f / 50.0f),  0.0f, 1.0f);

    // Chaotic modal detuning
    const float detuneCents = mNormX * mDepth * mMaxDetuneCents;
    const float ratioUp   = std::pow(2.0f, detuneCents * (1.0f / 1200.0f));
    const float ratioDown = std::pow(2.0f, -detuneCents * (1.0f / 1200.0f));

    // Anchor fundamental mode 0 so high-Q resonators stay pitch-locked to the exciter
    mFreqMultipliers[0] = 1.0f + 0.15f * (ratioDown - 1.0f);
    for (size_t i = 1; i < kNumModes; ++i) {
        // Odd modes detune up, even modes detune down
        mFreqMultipliers[i] = (i % 2 == 1) ? ratioUp : ratioDown;
    }

    // Modal Q-factor spread modulation
    mQSpread = std::clamp(1.0f + (mNormZ - 0.5f) * mDepth * mQModDepth * 2.0f, 0.2f, 5.0f);

    // Spatial panning offset angle in radians
    mPanOffsetRad = mNormY * mDepth * 0.45f;
}

} // namespace pf02::dsp
