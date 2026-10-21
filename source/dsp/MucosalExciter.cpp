#include "MucosalExciter.h"
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

MucosalExciter::MucosalExciter() noexcept {
    reset();
}

void MucosalExciter::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mPrng.setSeed(0x504F4F50454E32ULL);
    mDcBlocker.setCutoff(mSampleRate, 14.0f);
    reset();
}

void MucosalExciter::reset() noexcept {
    mDisplacement        = 0.0f; // Initial closed contact seal
    mVelocity            = 0.0f;
    mAcceleration        = 0.0f;
    mAirVelocity         = 0.0f;
    mUpstreamPressure    = 0.0f;
    mPrevVolumeFlow      = 0.0f;
    mStuck               = true; // Starts in adhesive mucosal contact
    mBurstThresholdPa    = 250.0f;
    mJustOpened          = false;
    mOpeningTransientAmp = 0.0f;
    mImpactSlapAmp       = 0.0f;
    mVactrolConductance  = 0.0f;
    mDcBlocker.reset();
}

void MucosalExciter::triggerSealBurst(float burstIntensity) noexcept {
    mStuck = false;
    mJustOpened = true;
    const float burstVel = std::clamp(burstIntensity, 0.1f, 10.0f) * 2.5f;
    mVelocity = burstVel;
    mOpeningTransientAmp = burstVel;
    mDisplacement = std::max(mDisplacement, 0.0003f);
}

void MucosalExciter::triggerSlap(float velocity, float hardness) noexcept {
    const float slapVel = -std::abs(velocity) * 2.0f;
    mVelocity = slapVel;
    const float normHardness = std::clamp(hardness, 1.2f, 3.0f);
    mImpactSlapAmp = std::abs(velocity) * std::pow(normHardness, 1.5f);
}

float MucosalExciter::processSample(
    const MucosalExciterParams& params,
    float& outAirVelocity,
    float& outAperture) noexcept
{
    mJustOpened = false;
    mOpeningTransientAmp = 0.0f;
    mImpactSlapAmp = 0.0f;

    const float dt = 1.0f / mSampleRate;

    // 1. Buchla 292 Optical Vactrol Dynamic Sag Droop
    // Fast attack (~2 ms), nonlinearly sagging decay tau * [1 + kappa * (1 - g)^2]
    const float uDrive = std::clamp(params.colonicPressure, 0.0f, 1.0f);
    float vactrolTau;
    if (uDrive >= mVactrolConductance) {
        vactrolTau = 0.002f; // 2 ms attack
    } else {
        constexpr float kSagKappa = 4.5f;
        const float sagFactor = 1.0f + kSagKappa * (1.0f - mVactrolConductance) * (1.0f - mVactrolConductance);
        const float tauDecaySec = std::max(0.005f, params.vactrolSagTauMs * 0.001f);
        vactrolTau = tauDecaySec * sagFactor;
    }

    const float alphaVactrol = 1.0f - std::exp(-dt / std::max(1.0e-5f, vactrolTau));
    mVactrolConductance += alphaVactrol * (uDrive - mVactrolConductance);
    mVactrolConductance = flushDenormal(std::clamp(mVactrolConductance, 0.0f, 1.0f));

    // Perceptual non-linear head pressure mapping: P_drive up to 2200 Pa (~22 cm H2O)
    const float pDrive = std::pow(mVactrolConductance, 1.8f) * 2200.0f;

    // Relaxation when drive pressure is near zero
    if (pDrive < 1.0e-3f) {
        mDisplacement = 0.0f;
        mVelocity = 0.0f;
        mAcceleration = 0.0f;
        mAirVelocity = 0.0f;
        mUpstreamPressure = 0.0f;
        mPrevVolumeFlow = 0.0f;
        mStuck = true; // Closed seal held by mucosal stiction
        outAirVelocity = 0.0f;
        outAperture = 0.0f;
        return mDcBlocker.process(0.0f);
    }

    // 2. Fundamental Frequency & Viscoelastic Mechanics
    const float fClamped = std::clamp(params.frequencyHz, 7.0f, 420.0f);
    const float tensionScale = 0.75f + 0.50f * std::clamp(params.mucosalTension, 0.0f, 1.0f);
    const float fEff = std::clamp(fClamped * tensionScale, 7.0f, 600.0f);
    const float omega0 = kTwoPi * fEff;

    // Effective tissue mass (grams to kg)
    constexpr float mEff = 0.0012f; // 1.2 grams
    const float kSpring = mEff * omega0 * omega0; // Spring stiffness
    const float rDamping = 2.0f * 0.075f * omega0 * mEff; // Damping

    // Resting aperture y0 (meters)
    const float y0 = std::clamp(params.restingApertureMm * 0.001f, 0.0001f, 0.005f);

    // 3. Upstream Reservoir Pressure Accumulation
    const float fillRate = 3.5f * omega0;
    const float currentArea = (mStuck || mDisplacement <= 0.0f) ? 0.0f : (kSlitWidthW * mDisplacement);
    const float currentVolumeFlow = currentArea * mAirVelocity;

    const float dPup = fillRate * (pDrive - mUpstreamPressure) - (currentVolumeFlow * omega0 * 10.0f);
    mUpstreamPressure = std::clamp(mUpstreamPressure + dt * dPup, 0.0f, 4500.0f);
    mUpstreamPressure = flushDenormal(mUpstreamPressure);

    // 4. Karnopp Stick-Slip Friction & Stribeck Dynamics
    constexpr float kKarnoppDeadband = 0.0015f; // vd = 0.0015 m/s
    const float stictionLevel = std::clamp(params.karnoppStiction, 0.0f, 1.0f);
    const float normalForce = (mUpstreamPressure * 0.00025f) + (stictionLevel * 0.35f);
    const float fs = 0.85f * normalForce; // Static breakaway force
    const float fc = 0.35f * normalForce; // Coulomb kinetic friction
    const float vStribeck = std::max(0.05f, params.stribeckBreakaway);

    // Adhesive mucosal seal stiction logic
    if (mStuck) {
        if (mUpstreamPressure >= mBurstThresholdPa || (pDrive > 50.0f && mUpstreamPressure >= (0.85f * pDrive))) {
            // Explosive seal breakaway!
            mStuck = false;
            mJustOpened = true;

            // Stribeck breakaway velocity surge
            const float surge = std::sqrt(std::max(0.0f, 2.0f * mUpstreamPressure / mEff)) * 0.038f + 1.6f;
            mVelocity = surge;
            mOpeningTransientAmp = surge;
            mDisplacement = 0.00020f; // Crack seal immediately
            // Randomize next burst threshold in Pa
            mBurstThresholdPa = (150.0f + 600.0f * stictionLevel) * (0.85f + 0.30f * mPrng.nextFloat());
        } else {
            // Locked in stick phase
            mDisplacement = 0.0f;
            mVelocity = 0.0f;
            mAcceleration = 0.0f;
            mAirVelocity = 0.0f;
            outAirVelocity = 0.0f;
            outAperture = 0.0f;
            return mDcBlocker.process(0.0f);
        }
    }

    // 5. Airflow Velocity via Bernoulli Equation
    if (!mStuck && mDisplacement > 1.0e-6f && mUpstreamPressure > 0.0f) {
        mAirVelocity = std::sqrt(2.0f * mUpstreamPressure / kAirDensityRho);
    } else {
        mAirVelocity = 0.0f;
    }
    mAirVelocity = flushDenormal(mAirVelocity);

    // 6. Force Balance on Mucosal Tissue
    // Spring & linear damping
    const float dispOffset = mDisplacement - y0;
    const float fSpring = -kSpring * dispOffset;
    const float fDamp   = -rDamping * mVelocity;

    // Upstream static pressure push
    const float fPressure = mUpstreamPressure * 0.00022f;

    // Bernoulli negative suction pressure in narrow slit: P_bernoulli = -0.5 * rho * v^2
    const float fBernoulli = (mDisplacement > 1.0e-6f) ? (-0.5f * kAirDensityRho * mAirVelocity * mAirVelocity * (kSlitWidthW * 0.004f)) : 0.0f;

    // Stribeck friction force during motion
    float fFriction = 0.0f;
    if (std::abs(mVelocity) < kKarnoppDeadband) {
        // Micro-stick shear resistance
        fFriction = std::clamp(mVelocity * 400.0f, -fs, +fs);
    } else {
        const float sgn = (mVelocity > 0.0f) ? 1.0f : -1.0f;
        const float decay = std::exp(-(mVelocity * mVelocity) / (vStribeck * vStribeck));
        fFriction = sgn * (fc + (fs - fc) * decay) + 0.12f * normalForce * mVelocity;
    }

    // 7. Hunt-Crossley Nonlinear Hertzian Impact Contact Mechanics (y <= 0)
    float fImpact = 0.0f;
    if (mDisplacement <= 0.0f) {
        const float penetrationX = -mDisplacement; // Penetration depth > 0
        const float xDot = -mVelocity;             // Penetration velocity

        const float hardnessNorm = std::clamp((params.tissueHardness - 1.2f) / 1.8f, 0.0f, 1.0f);
        const float kc = 1.0e6f * std::pow(10.0f, 2.8f * hardnessNorm);
        const float lambdaC = 40.0f * (1.0f - 0.4f * std::clamp(params.impactRestitution, 0.0f, 1.0f));
        const float alpha = std::clamp(params.tissueHardness, 1.2f, 3.0f);

        const float xPow = std::pow(std::max(1.0e-8f, penetrationX), alpha);
        // Hunt-Crossley contact formula: Fc = max(0, kc * x^alpha + lambda * x^alpha * x_dot)
        const float contactForce = std::max(0.0f, kc * xPow + lambdaC * xPow * xDot);
        fImpact = contactForce; // Pushes tissue back into positive y

        // Slapdown impulse telemetry
        if (mVelocity < -0.1f) {
            mImpactSlapAmp = contactForce * 0.002f;
        }

        // Check for adhesive stick latching
        if (std::abs(mVelocity) < kKarnoppDeadband * 2.0f && stictionLevel > 0.15f) {
            mStuck = true;
            mDisplacement = 0.0f;
            mVelocity = 0.0f;
        }
    }

    // 8. Symplectic Verlet Integration
    const float totalForce = fSpring + fDamp + fPressure + fBernoulli + fImpact - fFriction;
    const float aNext = totalForce / mEff;

    // Velocity-Verlet step
    const float xNext = mDisplacement + mVelocity * dt + 0.5f * mAcceleration * dt * dt;
    mVelocity += 0.5f * (mAcceleration + aNext) * dt;
    mDisplacement = flushDenormal(xNext);
    mAcceleration = aNext;

    // Clamp displacement to prevent runaway divergence under extreme chaos
    mDisplacement = std::clamp(mDisplacement, -0.001f, 0.015f);

    // 9. Acoustic Output Signal
    // Volume flow: U(t) = Area * v_air
    const float newArea = (mDisplacement > 0.0f) ? (kSlitWidthW * mDisplacement) : 0.0f;
    const float newVolumeFlow = newArea * mAirVelocity;
    // Acoustic pressure derivative dU/dt
    const float dFlowDt = (newVolumeFlow - mPrevVolumeFlow) * mSampleRate;
    mPrevVolumeFlow = newVolumeFlow;

    const float acousticWave = dFlowDt * 0.08f + mImpactSlapAmp * 0.5f + mOpeningTransientAmp * 0.35f;

    outAirVelocity = mAirVelocity;
    outAperture    = std::max(0.0f, mDisplacement);

    return mDcBlocker.process(acousticWave);
}

} // namespace pf02::dsp
