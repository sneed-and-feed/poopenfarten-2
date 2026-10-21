#pragma once

#include "DspMath.h"
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

struct MucosalExciterParams {
    float colonicPressure    { 0.70f };  // Upstream pressure drive [0.0, 1.0]
    float mucosalTension     { 0.50f };  // Tissue elasticity [0.0, 1.0]
    float restingApertureMm  { 1.0f };   // Resting aperture y0 [0.1, 5.0] mm
    float karnoppStiction    { 0.65f };  // Static seal stiction [0.0, 1.0]
    float stribeckBreakaway  { 2.2f };   // Stribeck characteristic velocity vs [0.1, 10.0] m/s
    float tissueHardness     { 2.1f };   // Hertzian contact exponent alpha [1.2, 3.0]
    float impactRestitution  { 0.45f };  // Collision damping lambda_c [0.0, 1.0]
    float vactrolSagTauMs    { 85.0f };  // Buchla 292 decay sag tau [20.0, 500.0] ms
    float frequencyHz        { 55.0f };  // Fundamental pitch [7.0, 420.0] Hz
};

/**
 * MucosalExciter: First-principles mucosal relaxation valve combining:
 * 1. Karnopp velocity-deadband stick-slip friction (vd = 0.0015 m/s).
 * 2. Stribeck exponential breakaway curve for explosive seal-burst transients.
 * 3. Hunt-Crossley nonlinear Hertzian impact contact mechanics (y <= 0) with symplectic Verlet integration.
 * 4. Buchla 292 optical vactrol sag droop on upstream colonic drive.
 */
class MucosalExciter {
public:
    MucosalExciter() noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Process a single audio sample:
    // returns acoustic output pressure/velocity, outputs instantaneous air velocity and aperture
    [[nodiscard]] float processSample(
        const MucosalExciterParams& params,
        float& outAirVelocity,
        float& outAperture) noexcept;

    // Direct kinetic triggers
    void triggerSealBurst(float burstIntensity) noexcept;
    void triggerSlap(float velocity, float hardness = 2.1f) noexcept;

    // Telemetry and status queries
    [[nodiscard]] float getAperture() const noexcept { return mDisplacement; }
    [[nodiscard]] float getAirVelocity() const noexcept { return mAirVelocity; }
    [[nodiscard]] float getColonicPressure() const noexcept { return mUpstreamPressure; }
    [[nodiscard]] float getVactrolConductance() const noexcept { return mVactrolConductance; }
    [[nodiscard]] bool  isSealStuck() const noexcept { return mStuck; }
    [[nodiscard]] bool  wasOpeningTransient() const noexcept { return mJustOpened; }
    [[nodiscard]] float getOpeningTransientAmp() const noexcept { return mOpeningTransientAmp; }
    [[nodiscard]] float getImpactSlapAmp() const noexcept { return mImpactSlapAmp; }
    [[nodiscard]] float getTissueVelocity() const noexcept { return mVelocity; }

private:
    float mSampleRate { 48000.0f };
    FastPRNG mPrng { 0x504F4F50454E32ULL }; // "POOPEN2"

    // Mucosal valve mechanical state
    float mDisplacement       { 0.001f }; // y(t) aperture opening (meters)
    float mVelocity           { 0.0f };   // dy/dt tissue wall velocity (m/s)
    float mAcceleration       { 0.0f };   // d^2y/dt^2 (m/s^2)
    float mAirVelocity        { 0.0f };   // Bernoulli slit air velocity (m/s)
    float mUpstreamPressure   { 0.0f };   // P_gut(t) upstream head (Pa)
    float mPrevVolumeFlow     { 0.0f };   // For acoustic derivative dU/dt

    // Karnopp & Stribeck friction state
    bool  mStuck              { false };  // Sticky mucosal seal locked state
    float mBurstThresholdPa   { 300.0f }; // Dynamic breakaway pressure threshold
    bool  mJustOpened         { false };  // Impulse flag on breakaway
    float mOpeningTransientAmp{ 0.0f };
    float mImpactSlapAmp      { 0.0f };

    // Buchla 292 optical vactrol sag state
    float mVactrolConductance { 0.0f };

    // Anti-rumble highpass filter (~14 Hz)
    OnePoleHighpass mDcBlocker;
};

} // namespace pf02::dsp
