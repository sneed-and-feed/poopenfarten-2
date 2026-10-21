#include "ModalResonatorMatrix.h"
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

ModalResonatorMatrix::ModalResonatorMatrix() noexcept {
    reset();
}

void ModalResonatorMatrix::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);

    // Prime delay line lengths in samples
    // D1 = 13.7 ms, D2 = 19.3 ms
    mDelay1Samples = std::clamp(mSampleRate * 0.0137f, 10.0f, static_cast<float>(kDelay1Cap - 4));
    mDelay2Samples = std::clamp(mSampleRate * 0.0193f, 10.0f, static_cast<float>(kDelay2Cap - 4));

    mDiffuserFilter1.reset();
    mDiffuserFilter1.setCutoff(mSampleRate, 3600.0f);
    mDiffuserFilter2.reset();
    mDiffuserFilter2.setCutoff(mSampleRate, 3600.0f);

    reset();
}

void ModalResonatorMatrix::reset() noexcept {
    mG.fill(0.01f);
    mK.fill(0.1f);
    mA1.fill(1.0f);
    mS1.fill(0.0f);
    mS2.fill(0.0f);
    mQNorm.fill(1.0f);
    mCurrentFrequencies.fill(100.0f);
    mModalEnergies.fill(0.0f);
    mSlowState.fill(0.0f);
    mTriadEnergyFlow.fill(0.0f);

    mLorenzFreqMult.fill(1.0f);
    mLorenzQSpread = 1.0f;
    mLorenzPanOffset = 0.0f;

    mDelay1.reset();
    mDelay2.reset();
    mDiffuserFilter1.reset();
    mDiffuserFilter2.reset();

    mTriadCascade.reset();
    mCachedBoundaryMode = -1;
    mCachedCavityModes.fill(0.0f);
}

void ModalResonatorMatrix::dampStates(float factor) noexcept {
    for (size_t i = 0; i < kNumModes; ++i) {
        mS1[i] = flushDenormal(mS1[i] * factor);
        mS2[i] = flushDenormal(mS2[i] * factor);
        mSlowState[i] = flushDenormal(mSlowState[i] * factor);
    }
}

void ModalResonatorMatrix::computeBaseRatios(GeometricManifold manifold, float morph, ModalArray& outRatios) const noexcept {
    const float morphClamped = std::clamp(morph, 0.0f, 1.0f);
    const ModalArray* rA = nullptr;
    const ModalArray* rB = nullptr;

    switch (manifold) {
        case GeometricManifold::SigmoidHorn:
            rA = &kSigmoidHornRatios;
            rB = &kChladniRingRatios;
            break;
        case GeometricManifold::ChladniRing:
            rA = &kChladniRingRatios;
            rB = &kGlottalColonicRatios;
            break;
        case GeometricManifold::GlottalColonic:
            rA = &kGlottalColonicRatios;
            rB = &kPorcelainBowlRatios;
            break;
        case GeometricManifold::PorcelainBowl:
            rA = &kPorcelainBowlRatios;
            rB = &kSigmoidHornRatios;
            break;
        default:
            rA = &kGlottalColonicRatios;
            rB = &kGlottalColonicRatios;
            break;
    }

    for (size_t i = 0; i < kNumModes; ++i) {
        outRatios[i] = (*rA)[i] + morphClamped * ((*rB)[i] - (*rA)[i]);
    }
}

void ModalResonatorMatrix::getDampingQ(DampingProfile profile, ModalArray& outQ) const noexcept {
    float qBase = 20.0f;
    float exponent = 1.0f;
    float slope = 0.1f;

    switch (profile) {
        case DampingProfile::MucosalTissue:
            qBase = 15.0f;
            slope = 0.14f;
            exponent = 1.15f;
            break;
        case DampingProfile::Porcelain:
            qBase = 125.0f;
            slope = 0.025f;
            exponent = 0.85f;
            break;
        case DampingProfile::WetElastomer:
            qBase = 32.0f;
            slope = 0.08f;
            exponent = 1.0f;
            break;
        case DampingProfile::BellBronze:
            qBase = 240.0f;
            slope = 0.008f;
            exponent = 0.70f;
            break;
        case DampingProfile::StiffCartilage:
            qBase = 24.0f;
            slope = 0.06f;
            exponent = 0.95f;
            break;
    }

    for (size_t i = 0; i < kNumModes; ++i) {
        const float mIdx = static_cast<float>(i);
        const float loss = 1.0f + slope * std::pow(mIdx, exponent);
        outQ[i] = std::max(1.5f, qBase / loss);
    }
}

void ModalResonatorMatrix::setLorenzModulation(
    const ModalArray& freqMultipliers,
    float qSpreadFactor,
    float panOffsetRad) noexcept
{
    mLorenzFreqMult = freqMultipliers;
    mLorenzQSpread = std::clamp(qSpreadFactor, 0.2f, 5.0f);
    mLorenzPanOffset = panOffsetRad;
}

void ModalResonatorMatrix::updateFilterCoefficients(const ModalMatrixParams& params) noexcept {
    ModalArray baseRatios {};
    computeBaseRatios(params.manifold, params.manifoldMorph, baseRatios);

    ModalArray baseQ {};
    getDampingQ(params.damping, baseQ);

    const float f0 = std::clamp(params.fundamentalHz, 7.0f, 2000.0f);
    const float nyquist = mSampleRate * 0.49f;

    // Golden-Ratio Stereo Panning law: Phi = 137.507764 deg (2.39996323 rad)
    const float spread = std::clamp(params.stereoSpread * 0.01f, 0.0f, 1.0f);

    for (size_t i = 0; i < kNumModes; ++i) {
        // Modal frequency with Lorenz chaotic multiplier
        const float fTarget = f0 * baseRatios[i] * mLorenzFreqMult[i];
        const float fClamped = std::clamp(fTarget, 10.0f, nyquist);
        mCurrentFrequencies[i] = fClamped;

        // Lorenz Q-factor spread: spreads low vs high modes
        const float modeSpread = 1.0f + (mLorenzQSpread - 1.0f) * (static_cast<float>(i) - 7.5f) / 8.0f;
        const float qMode = std::clamp(baseQ[i] * std::max(0.2f, modeSpread), 1.0f, 400.0f);

        // Discrete frequency warp and damping
        const float g = std::tan(kPi * (fClamped / mSampleRate));
        const float k = 1.0f / qMode;
        mG[i] = g;
        mK[i] = k;
        mA1[i] = 1.0f / (1.0f + g * (g + k));

        // Input Q-normalization: scales excitation so internal SVF states s1, s2 remain bounded
        // while preserving rich resonant presence for high-Q profiles (Porcelain Q=125, BellBronze Q=240).
        const float highQComp = std::pow(std::max(1.0f, qMode / 18.0f), 0.38f);
        mQNorm[i] = k * std::sqrt(8.0f + 1.8f * qMode) * highQComp;

        // Golden Ratio Panning coefficients
        const float angle = std::fmod(static_cast<float>(i) * kGoldenAngleRad + mLorenzPanOffset, kTwoPi);
        const float panPan = std::clamp(std::cos(angle) * spread, -1.0f, 1.0f);
        // Constant-power sine/cosine panning law
        const float normPan = 0.5f * (panPan + 1.0f); // [0, 1]
        mPanL[i] = std::cos(kHalfPi * normPan);
        mPanR[i] = std::sin(kHalfPi * normPan);
    }
}

void ModalResonatorMatrix::processSample(
    float exciterInput,
    const ModalMatrixParams& params,
    float& outL,
    float& outR) noexcept
{
    updateFilterCoefficients(params);

    // 1. Read Acyclic Delay Diffuser Taps for Householder tail
    const float diff1 = mDiffuserFilter1.process(mDelay1.readHermite(mDelay1Samples));
    const float diff2 = mDiffuserFilter2.process(mDelay2.readHermite(mDelay2Samples));
    const float tailDiff = (diff1 + diff2) * 0.5f;

    // Excitation input injected into modal resonators
    const float xIn = exciterInput + tailDiff * 0.14f;

    // 2. 16-Pole Zavalishin TPT SVF Bandpass Matrix Step
    ModalArray modeOutputs {};

    for (size_t i = 0; i < kNumModes; ++i) {
        const float modeIn = xIn * mQNorm[i];
        const float vHp = mA1[i] * (modeIn - (mK[i] + mG[i]) * mS1[i] - mS2[i]);
        const float vBp = mG[i] * vHp + mS1[i];
        const float vLp = mG[i] * vBp + mS2[i];

        // Non-linear acoustic radiation resistance damping F_rad ~ v^2
        const float vSq = vBp * vBp;
        constexpr float kRadiationCoeff = 0.08f;
        const float radDamp = 1.0f / (1.0f + kRadiationCoeff * vSq);

        float nextS1 = (2.0f * vBp - mS1[i]) * radDamp;
        float nextS2 = 2.0f * vLp - mS2[i];

        // Contractive soft-saturation state bounding to prevent feedback blowup
        if (std::abs(nextS1) > 2.0f) {
            const float sgn = (nextS1 > 0.0f) ? 1.0f : -1.0f;
            nextS1 = sgn * (2.0f + 2.0f * std::tanh((std::abs(nextS1) - 2.0f) * 0.5f));
        }
        if (std::abs(nextS2) > 2.0f) {
            const float sgn = (nextS2 > 0.0f) ? 1.0f : -1.0f;
            nextS2 = sgn * (2.0f + 2.0f * std::tanh((std::abs(nextS2) - 2.0f) * 0.5f));
        }

        mS1[i] = flushDenormal(std::clamp(nextS1, -4.0f, 4.0f));
        mS2[i] = flushDenormal(std::clamp(nextS2, -4.0f, 4.0f));

        // Mode output (already Q-normalized at input so s1/s2 remain linear)
        const float yOut = vBp;
        modeOutputs[i] = flushDenormal(yOut);

        // Track modal energy for CRT oscilloscope telemetry
        mModalEnergies[i] = flushDenormal(0.999f * mModalEnergies[i] + 0.001f * (yOut * yOut));
    }

    // 3. Fast O(N) Orthogonal Householder Energy Scattering Matrix
    // H = I - (2 / 16) 1 1^T
    const float hhCoupling = std::clamp(params.householderCoupling, 0.0f, 1.0f);
    applyHouseholderScattering16(modeOutputs, hhCoupling);

    // Feed Householder diffused energy sum into acyclic prime delay lines (bounded feedback < 1)
    float modalSum = 0.0f;
    for (size_t i = 0; i < kNumModes; ++i) modalSum += modeOutputs[i];
    mDelay1.write(modalSum * 0.025f);
    mDelay2.write(-modalSum * 0.025f);

    // 4. DarkSnail Skew-Symmetric Bilinear Triad Coupling & SlowFastTriadCascade in so(16)
    // Smooth slow state vector
    for (size_t i = 0; i < kNumModes; ++i) {
        mSlowState[i] = flushDenormal(0.995f * mSlowState[i] + 0.005f * modeOutputs[i]);
    }

    mTriadCascade.setCascadeGain(params.triadCouplingGain);
    mTriadCascade.setUnrollSteps(params.zenoUnrollK);

    ModalArray coupledModes = modeOutputs;
    if (params.triadCouplingGain > 0.001f) {
        coupledModes = mTriadCascade.processCascade(mSlowState, modeOutputs, mTriadOp, params.zenoUnrollK);
        for (size_t i = 0; i < kNumModes; ++i) {
            mTriadEnergyFlow[i] = flushDenormal(std::abs(coupledModes[i] - modeOutputs[i]));
        }
    } else {
        mTriadEnergyFlow.fill(0.0f);
    }

    // 5. 2D Continuous Spectral Jordan Cavity Resonance Mix
    const float cavityMix = std::clamp(params.porcelainCavityMix * 0.01f, 0.0f, 1.0f);
    if (cavityMix > 0.001f) {
        if (mCachedBoundaryMode != params.porcelainBoundaryMode) {
            mCachedBoundaryMode = params.porcelainBoundaryMode;
            mJordanField.generateCavityField(1.0f, 0.65f, params.porcelainBoundaryMode);
            mCachedCavityModes = mJordanField.projectTo16Modes();
        }
        for (size_t i = 0; i < kNumModes; ++i) {
            const float cavityGain = 0.78f + 0.65f * std::abs(mCachedCavityModes[i]);
            coupledModes[i] = flushDenormal(coupledModes[i] * ((1.0f - cavityMix) + cavityMix * cavityGain));
        }
    }

    // 6. Golden-Ratio Stereo Modal Panning Accumulation
    float accL = 0.0f;
    float accR = 0.0f;

    for (size_t i = 0; i < kNumModes; ++i) {
        accL += coupledModes[i] * mPanL[i];
        accR += coupledModes[i] * mPanR[i];
    }

    outL = flushDenormal(accL * 0.42f);
    outR = flushDenormal(accR * 0.42f);
}

} // namespace pf02::dsp
