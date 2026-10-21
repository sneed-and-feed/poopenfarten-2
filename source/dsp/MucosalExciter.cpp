#include "MucosalExciter.h"
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

MucosalExciter::MucosalExciter() noexcept {
    reset();
}

void MucosalExciter::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mPrng.setSeed(mBaseSeed);
    mDcBlocker.setCutoff(mSampleRate, 14.0f);
    reset();
}

void MucosalExciter::reset() noexcept {
    mDisplacement        = 0.0f; // Initial closed contact seal
    mMeanAperture        = 0.0f;
    mVelocity            = 0.0f;
    mAcceleration        = 0.0f;
    mAirVelocity         = 0.0f;
    mUpstreamPressure    = 0.0f;
    mPrevVolumeFlow      = 0.0f;
    mStuck               = true; // Starts in adhesive mucosal contact
    mWasInContact        = false;
    mBurstThresholdPa    = 350.0f;
    mJustOpened          = false;
    mOpeningTransientAmp = 0.0f;
    mImpactSlapAmp       = 0.0f;
    mPendingOpeningAmp   = 0.0f;
    mPendingSlapAmp      = 0.0f;
    mVactrolConductance  = 0.0f;
    mDcBlocker.reset();
}

void MucosalExciter::triggerSealBurst(float burstIntensity) noexcept {
    mStuck = false;
    mJustOpened = true;
    const float burstNorm = std::clamp(burstIntensity, 0.1f, 4.0f) * 1.45f;
    mOpeningTransientAmp = burstNorm;
    mPendingOpeningAmp = std::max(mPendingOpeningAmp, burstNorm);
    mUpstreamPressure = std::max(mUpstreamPressure, 650.0f * std::clamp(burstIntensity, 0.1f, 2.5f));
}

void MucosalExciter::triggerSlap(float velocity, float hardness) noexcept {
    const float normHardness = std::clamp(hardness, 1.2f, 3.0f);
    const float amp = std::clamp(std::abs(velocity), 0.05f, 2.0f) * 0.15f * std::pow(normHardness, 1.2f);
    mImpactSlapAmp = amp;
    mPendingSlapAmp = std::max(mPendingSlapAmp, amp);
}

float MucosalExciter::processSample(
    const MucosalExciterParams& params,
    float& outAirVelocity,
    float& outAperture) noexcept
{
    mJustOpened = (mPendingOpeningAmp > 0.0f);
    mOpeningTransientAmp = mPendingOpeningAmp;
    mImpactSlapAmp = mPendingSlapAmp;
    mPendingOpeningAmp = 0.0f;
    mPendingSlapAmp = 0.0f;

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
    if (pDrive < 1.0e-3f && mOpeningTransientAmp <= 0.0f && mImpactSlapAmp <= 0.0f) {
        mDisplacement = 0.0f;
        mMeanAperture *= 0.995f;
        mVelocity = 0.0f;
        mAcceleration = 0.0f;
        mAirVelocity = 0.0f;
        mUpstreamPressure = 0.0f;
        mPrevVolumeFlow = 0.0f;
        mStuck = true;
        mWasInContact = false;
        outAirVelocity = 0.0f;
        outAperture = 0.0f;
        return mDcBlocker.process(0.0f);
    }

    // 2. Fundamental Frequency & Scale-Invariant Viscoelastic Mechanics
    const float fEff = std::clamp(params.frequencyHz, 7.0f, 2000.0f);
    const float tension = std::clamp(params.mucosalTension, 0.0f, 1.0f);
    const float omega0 = kTwoPi * fEff;

    constexpr float mEff = 0.0012f; // 1.2 grams effective tissue mass
    // Calibrate linear spring stiffness so nonlinear impact + stiffening limit cycle matches fEff across all tensions
    const float kSpring = (0.342f - 0.045f * tension) * mEff * omega0 * omega0;
    const float rDamping = 2.0f * (0.08f - 0.025f * tension) * std::sqrt(kSpring * mEff);
    const float y0 = std::clamp(params.restingApertureMm * 0.001f, 0.0001f, 0.005f);
    const float fScale = kSpring * y0;
    const float vScale = std::max(1.0e-5f, std::sqrt(kSpring / mEff) * y0);

    // Apply scale-invariant pending kinetic impulses relative to vScale and y0
    if (mOpeningTransientAmp > 0.0f) {
        mVelocity = std::max(mVelocity, std::min(2.2f, mOpeningTransientAmp) * vScale);
        mDisplacement = std::max(mDisplacement, 0.12f * y0);
        mMeanAperture = std::max(mMeanAperture, mDisplacement);
    } else if (mImpactSlapAmp > 0.0f) {
        mVelocity = std::min(mVelocity, -std::min(1.6f, mImpactSlapAmp * 1.8f) * vScale);
    }

    // 3. Upstream Reservoir Pressure Accumulation & Subglottal/Colonic Compliance Venting
    const float omegaP = std::min(omega0, 0.12f * mSampleRate);
    const float fillRate = 3.5f * omegaP;
    const float openRatio = (mStuck || mDisplacement <= 0.0f) ? 0.0f : std::clamp(mDisplacement / y0, 0.0f, 6.0f);
    const float ventCoeff = 5.5f * openRatio * openRatio * omegaP;
    const float alphaTot = std::min(fillRate + ventCoeff, 0.45f * mSampleRate);
    const float pTarget = (fillRate + ventCoeff > 1.0e-6f) ? (fillRate * pDrive) / (fillRate + ventCoeff) : 0.0f;
    mUpstreamPressure = flushDenormal(std::clamp(mUpstreamPressure + alphaTot * dt * (pTarget - mUpstreamPressure), 0.0f, 4500.0f));

    // 4. Karnopp Stick-Slip Friction & Stribeck Breakaway Dynamics
    const float kKarnoppDeadband = 0.02f * vScale;
    const float stictionLevel = std::clamp(params.karnoppStiction, 0.0f, 1.0f);
    const float pNorm = std::clamp(mUpstreamPressure / 1500.0f, 0.0f, 2.5f);
    const float driveNorm = std::clamp(pDrive / 1500.0f, 0.0f, 2.5f);
    const float vStribeckRatio = std::clamp(params.stribeckBreakaway / 2.2f, 0.2f, 4.0f);

    if (mStuck) {
        if (mUpstreamPressure >= mBurstThresholdPa || (pDrive > 50.0f && mUpstreamPressure >= 0.75f * pDrive)) {
            mStuck = false;
            mJustOpened = true;
            const float surge = std::sqrt(std::max(0.0f, 2.0f * mUpstreamPressure / mEff)) * 0.038f + 1.6f;
            mVelocity = 1.15f * vScale * (0.6f + 0.6f * driveNorm);
            mOpeningTransientAmp = std::max(mOpeningTransientAmp, surge);
            mDisplacement = 0.08f * y0;
            mMeanAperture = std::max(mMeanAperture, mDisplacement);
            mBurstThresholdPa = (150.0f + 600.0f * stictionLevel) * (0.85f + 0.30f * mPrng.nextFloat());
        } else {
            mDisplacement = 0.0f;
            mVelocity = 0.0f;
            mAcceleration = 0.0f;
            mAirVelocity = 0.0f;
            outAirVelocity = 0.0f;
            outAperture = 0.0f;
            return mDcBlocker.process(0.0f);
        }
    }

    // 5. Airflow Velocity via Bernoulli Equation with Slit Vena Contracta
    if (!mStuck && mDisplacement > 1.0e-6f && mUpstreamPressure > 0.0f) {
        mAirVelocity = std::sqrt(2.0f * mUpstreamPressure / kAirDensityRho) * (openRatio / (0.20f + openRatio));
    } else {
        mAirVelocity = 0.0f;
    }
    mAirVelocity = flushDenormal(mAirVelocity);

    // 6. Scale-Invariant Force Balance on Mucosal Tissue (Titze Mucosal Wave + Bernoulli Suction)
    const float eqY = (0.48f - 0.10f * tension) * y0 * std::min(1.3f, driveNorm + 0.2f);
    const float dispOffset = mDisplacement - eqY;
    const float normDisp = dispOffset / y0;
    const float nonlinStiff = 1.0f + (0.85f + 0.50f * tension) * normDisp * normDisp;
    const float fSpring = -kSpring * dispOffset * nonlinStiff;
    const float fDamp   = -rDamping * mVelocity;

    const float velNorm = std::clamp(mVelocity / (vScale + 1.0e-6f), -3.0f, 3.0f);
    const float pressureMod = (mUpstreamPressure - 0.52f * pDrive) / std::max(100.0f, pDrive);
    const float fPressure = fScale * (
        0.65f * pressureMod +
        (0.32f + 0.15f * stictionLevel) * std::tanh(velNorm * 1.5f) * (1.0f - 0.35f * normDisp * normDisp)
    );
    const float fBernoulli = (mDisplacement > 1.0e-6f)
        ? (-fScale * (0.24f + 0.10f * tension) * pNorm * (openRatio / (0.25f + openRatio * openRatio)))
        : 0.0f;

    // Contact-zone Karnopp/Stribeck shear resistance (active in mucosal boundary layer y < 0.20 * y0)
    const float contactZone = std::clamp(1.0f - mDisplacement / (0.20f * y0), 0.0f, 1.0f);
    float fFriction = 0.0f;
    if (contactZone > 0.0f) {
        const float fStickMax = fScale * 0.18f * stictionLevel;
        if (std::abs(mVelocity) < kKarnoppDeadband) {
            fFriction = contactZone * std::clamp((mVelocity / (kKarnoppDeadband + 1.0e-9f)) * fStickMax, -fStickMax, fStickMax);
        } else {
            const float sgn = (mVelocity > 0.0f) ? 1.0f : -1.0f;
            const float decay = std::exp(-(velNorm * velNorm) / (vStribeckRatio * vStribeckRatio));
            fFriction = contactZone * sgn * fStickMax * (0.35f + 0.65f * decay);
        }
    }

    // 7. Hunt-Crossley Nonlinear Hertzian Impact Contact Mechanics (y <= 0)
    float fImpact = 0.0f;
    if (mDisplacement <= 0.0f) {
        const float penNorm = std::clamp(-mDisplacement / y0, 0.0f, 2.0f);
        const float vPenNorm = std::clamp(-mVelocity / (vScale + 1.0e-6f), -4.0f, 4.0f);

        const float hardness = std::clamp(params.tissueHardness, 1.2f, 3.0f);
        const float restitution = std::clamp(params.impactRestitution, 0.0f, 1.0f);
        const float kContactNorm = 8.5f + 5.0f * (hardness - 1.2f);
        const float lambdaImpact = 1.6f * (1.0f - 0.6f * restitution);

        const float xAlpha = std::pow(std::max(1.0e-6f, penNorm), hardness);
        fImpact = fScale * std::max(0.0f, kContactNorm * xAlpha * (1.0f + lambdaImpact * vPenNorm));

        // Detect impact collision entry transient
        if (!mWasInContact && vPenNorm > 0.15f) {
            mImpactSlapAmp = std::max(mImpactSlapAmp, std::min(1.5f, vPenNorm * 0.45f * std::pow(hardness * 0.5f, 1.1f)));
        }
        mWasInContact = true;

        // Only latch back into stuck seal if upstream drive pressure has genuinely collapsed
        if (pDrive < 8.0f && std::abs(mVelocity) < kKarnoppDeadband && mUpstreamPressure < mBurstThresholdPa * 0.15f) {
            mStuck = true;
            mDisplacement = 0.0f;
            mVelocity = 0.0f;
        }
    } else {
        mWasInContact = false;
    }

    // 8. Symplectic Velocity-Verlet Integration with Sub-Stepping Stability Guard
    const float totalForce = fSpring + fDamp + fPressure + fBernoulli + fImpact - fFriction;
    const float maxAccel = 22.0f * omega0 * vScale;
    const float aNext = std::clamp(totalForce / mEff, -maxAccel, maxAccel);

    if (!mStuck) {
        const float xNext = mDisplacement + mVelocity * dt + 0.5f * mAcceleration * dt * dt;
        mVelocity += 0.5f * (mAcceleration + aNext) * dt;
        mVelocity = std::clamp(mVelocity, -5.0f * vScale, 5.0f * vScale);
        mDisplacement = flushDenormal(std::clamp(xNext, -0.45f * y0, 3.5f * y0));
        mAcceleration = aNext;
    }

    // 9. Acoustic Output Signal (Glottal/Mucosal Volume Flow Pulse + Derivative)
    const float normAperture = (mDisplacement > 0.0f) ? std::clamp(mDisplacement / y0, 0.0f, 3.5f) : 0.0f;
    const float normAirVel = std::clamp(mAirVelocity / 32.0f, 0.0f, 1.8f);
    const float normVolumeFlow = normAperture * normAirVel;

    // Frequency-normalized flow derivative dU/dt / omega0 so gain is uniform across all registers
    const float dFlowNorm = ((normVolumeFlow - mPrevVolumeFlow) * mSampleRate) / std::max(80.0f, omega0);
    mPrevVolumeFlow = normVolumeFlow;

    const float rawWave = (normVolumeFlow * 0.52f)
                        + (std::clamp(dFlowNorm, -2.0f, 2.0f) * 0.34f)
                        + (std::min(mImpactSlapAmp, 1.5f) * 0.25f)
                        + (std::min(mOpeningTransientAmp, 2.2f) * 0.10f);
    const float acousticWave = std::tanh(rawWave * 0.90f) * 0.65f;

    outAirVelocity = mAirVelocity;
    outAperture    = std::max(0.0f, mDisplacement);
    if (outAperture > mMeanAperture) {
        mMeanAperture += 0.15f * (outAperture - mMeanAperture);
    } else {
        mMeanAperture *= 0.9985f;
    }

    return mDcBlocker.process(acousticWave);
}

} // namespace pf02::dsp
