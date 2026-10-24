#include "MasteringChain.h"
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

MasteringChain::MasteringChain() noexcept {
    reset();
}

void MasteringChain::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);

    mDcBlockL.prepare(mSampleRate);
    mDcBlockR.prepare(mSampleRate);
    mSubResonator.prepare(mSampleRate);

    mBaseDelaySamples = std::clamp(mSampleRate * 0.0055f, 10.0f, static_cast<float>(kDelayCapacity - 500));

    // Compander filter coefficients
    // 1-pole high-shelf pre-emphasis: +6 dB boost above 1.8 kHz
    const float f1 = 1800.0f;
    const float f2 = 3600.0f;
    const float w1 = kTwoPi * (f1 / mSampleRate);
    const float w2 = kTwoPi * (f2 / mSampleRate);
    const float tanW1 = std::tan(w1 * 0.5f);
    const float tanW2 = std::tan(w2 * 0.5f);

    const float preDenom = 1.0f + tanW2;
    mPreShelf.b0 = (1.0f + tanW1 * 2.0f) / preDenom;
    mPreShelf.b1 = (-1.0f + tanW1 * 2.0f) / preDenom;
    mPreShelf.a1 = (-1.0f + tanW2) / preDenom;

    // De-emphasis: exact inverse
    const float deDenom = 1.0f + tanW1 * 2.0f;
    mDeemph1.b0 = (1.0f + tanW2) / deDenom;
    mDeemph1.b1 = (-1.0f + tanW2) / deDenom;
    mDeemph1.a1 = (-1.0f + tanW1 * 2.0f) / deDenom;
    mDeemph2 = mDeemph1;
    mDeemph3 = mDeemph1;

    // 2-pole Butterworth reconstruction lowpass at 9.5 kHz
    const float fcRecon = std::min(9500.0f, mSampleRate * 0.45f);
    const float c = std::tan(kPi * (fcRecon / mSampleRate));
    const float cSq = c * c;
    const float reconDenom = 1.0f + kSqrt2 * c + cSq;

    mReconLp1.b0 = cSq / reconDenom;
    mReconLp1.b1 = 2.0f * cSq / reconDenom;
    mReconLp1.b2 = cSq / reconDenom;
    mReconLp1.a1 = (2.0f * (cSq - 1.0f)) / reconDenom;
    mReconLp1.a2 = (1.0f - kSqrt2 * c + cSq) / reconDenom;
    mReconLp2 = mReconLp1;
    mReconLp3 = mReconLp1;

    reset();
}

void MasteringChain::reset() noexcept {
    mBbd1.reset();
    mBbd2.reset();
    mBbd3.reset();
    mLfoPhase = 0.0f;

    mPreShelf.reset();
    mReconLp1.reset();
    mReconLp2.reset();
    mReconLp3.reset();
    mDeemph1.reset();
    mDeemph2.reset();
    mDeemph3.reset();

    mDcBlockL.reset();
    mDcBlockR.reset();
    mSubResonator.reset();

    mPeakHold = 0.0f;
    mRmsAcc   = 0.0f;
    mPeakLevelDb = -100.0f;
    mRmsLevelDb  = -100.0f;

    updateChorusSettings(DimensionChorusMode::ModeII);
}

void MasteringChain::updateChorusSettings(DimensionChorusMode mode) noexcept {
    switch (mode) {
        case DimensionChorusMode::ModeI:
            mRateHz  = 0.40f;
            mDepthMs = 1.5f;
            break;
        case DimensionChorusMode::ModeII:
            mRateHz  = 0.55f;
            mDepthMs = 2.2f;
            break;
        case DimensionChorusMode::ModeIII:
            mRateHz  = 0.75f;
            mDepthMs = 3.2f;
            break;
        case DimensionChorusMode::ModeIV:
            mRateHz  = 1.10f;
            mDepthMs = 4.5f;
            break;
    }
    mLfoInc = (mRateHz * kTwoPi) / mSampleRate;
}

void MasteringChain::process(
    float inL,
    float inR,
    const MasteringChainParams& params,
    float& outL,
    float& outR) noexcept
{
    updateChorusSettings(params.dimensionDMode);

    // 1. 15 Hz Infrasonic DC Blocker
    float sigL = mDcBlockL.process(inL);
    float sigR = mDcBlockR.process(inR);

    // 2. Roland Dimension D Tri-Phase BBD Chorus
    if (params.dimensionDEnable && params.dimensionDMix > 0.01f) {
        const float monoIn = (sigL + sigR) * 0.5f;
        const float sideIn = (sigL - sigR) * 0.35f;
        // Compander pre-emphasis
        const float compIn = mPreShelf.process(monoIn);

        mBbd1.write(compIn + sideIn);
        mBbd2.write(compIn - sideIn);
        mBbd3.write(compIn);

        const float depthSamples = (mDepthMs * 0.001f) * mSampleRate;

        // Tri-phase LFO: 0 deg, 120 deg (2pi/3), 240 deg (4pi/3)
        constexpr float kPhaseOffset120 = kTwoPi * (1.0f / 3.0f);
        constexpr float kPhaseOffset240 = kTwoPi * (2.0f / 3.0f);

        const float mod1 = FastSinTable::sin(mLfoPhase);
        const float mod2 = FastSinTable::sin(mLfoPhase + kPhaseOffset120);
        const float mod3 = FastSinTable::sin(mLfoPhase + kPhaseOffset240);

        const float del1 = std::clamp(mBaseDelaySamples + depthSamples * mod1, 10.0f, static_cast<float>(kDelayCapacity - 4));
        const float del2 = std::clamp(mBaseDelaySamples + depthSamples * mod2, 10.0f, static_cast<float>(kDelayCapacity - 4));
        const float del3 = std::clamp(mBaseDelaySamples + depthSamples * mod3, 10.0f, static_cast<float>(kDelayCapacity - 4));

        // Read fractional delay with Catmull-Rom Hermite interpolation
        const float raw1 = mBbd1.readHermite(del1);
        const float raw2 = mBbd2.readHermite(del2);
        const float raw3 = mBbd3.readHermite(del3);

        // Reconstruction lowpass & de-emphasis
        const float tap1 = mDeemph1.process(mReconLp1.process(raw1));
        const float tap2 = mDeemph2.process(mReconLp2.process(raw2));
        const float tap3 = mDeemph3.process(mReconLp3.process(raw3));

        // Roland Dimension D Spatial Matrix (calibrated for unity power & zero mono-sum cancellation)
        const float rawWetL = 0.92f * (tap1 - 0.35f * tap2 + 0.45f * tap3);
        const float rawWetR = 0.92f * (-0.35f * tap1 + tap2 + 0.45f * tap3);

        const float mid = 0.5f * (rawWetL + rawWetR);
        const float side = 0.5f * (rawWetL - rawWetR);

        const float spread = std::clamp(params.dimensionDSpread, 0.0f, 1.5f);
        const float wetL = mid + spread * side;
        const float wetR = mid - spread * side;

        const float mix = std::clamp(params.dimensionDMix * 0.01f, 0.0f, 1.0f);
        sigL = (1.0f - mix) * sigL + mix * wetL;
        sigR = (1.0f - mix) * sigR + mix * wetR;

        // Advance LFO phase
        mLfoPhase += mLfoInc;
        if (mLfoPhase >= kTwoPi) mLfoPhase -= kTwoPi;
    }

    // 3. Phase-Locked Visceral Sub-Bass Reinforcement
    if (params.visceralSubReinforceDb > -35.0f) {
        const float subDrive = (sigL + sigR) * 0.5f;
        const float subWave = mSubResonator.process(subDrive);
        const float subGain = dbToGain(params.visceralSubReinforceDb);
        const float subSignal = subWave * subGain * 0.18f;
        sigL += subSignal;
        sigR += subSignal;
    }

    // 4. Colonic Drive & C^1 Soft-Knee Asymptotic BoundedSaturator
    const float driveNorm = std::clamp(params.colonicSaturationDrive * 0.01f, 0.0f, 1.0f);
    const float driveGain = 0.82f + 0.98f * driveNorm;
    sigL *= driveGain;
    sigR *= driveGain;

    // C^1 Soft-Knee Asymptotic Saturator (Knee = 0.72, Ceiling = 0.98) — never flat-tops
    auto hermiteSaturate = [](float x) noexcept -> float {
        if (!isFiniteFloat(x)) [[unlikely]] return 0.0f;
        const float absX = std::abs(x);
        constexpr float kKnee = 0.72f;
        constexpr float kSpan = 0.26f;

        if (absX <= kKnee) return x;
        const float sgn = (x > 0.0f) ? 1.0f : -1.0f;
        return sgn * (kKnee + kSpan * std::tanh((absX - kKnee) / kSpan));
    };

    sigL = hermiteSaturate(sigL);
    sigR = hermiteSaturate(sigR);

    // 5. Master Output Trim
    const float masterGain = dbToGain(params.masterVolumeDb);
    sigL *= masterGain;
    sigR *= masterGain;

    // 6. Post-Master Soft-Ceiling & Brickwall Protection (strictly < 0.99, never flat-tops)
    auto softCeiling = [](float x) noexcept -> float {
        if (!isFiniteFloat(x)) [[unlikely]] return 0.0f;
        const float absX = std::abs(x);
        if (absX <= 0.85f) return x;
        const float sgn = (x > 0.0f) ? 1.0f : -1.0f;
        return sgn * (0.85f + 0.14f * std::tanh((absX - 0.85f) / 0.14f));
    };

    sigL = flushDenormal(std::clamp(softCeiling(sigL), -kPeakCeiling, kPeakCeiling));
    sigR = flushDenormal(std::clamp(softCeiling(sigR), -kPeakCeiling, kPeakCeiling));

    outL = sigL;
    outR = sigR;

    // Telemetry ballistics update
    const float samplePeak = std::max(std::abs(sigL), std::abs(sigR));
    mPeakHold = std::max(samplePeak, mPeakHold * 0.9995f);
    mRmsAcc = 0.999f * mRmsAcc + 0.001f * (0.5f * (sigL * sigL + sigR * sigR));

    mPeakLevelDb = gainToDb(mPeakHold);
    mRmsLevelDb  = gainToDb(std::sqrt(std::max(1.0e-12f, mRmsAcc)));
}

void MasteringChain::processBlock(
    float* bufferL,
    float* bufferR,
    int numSamples,
    const MasteringChainParams& params) noexcept
{
    for (int i = 0; i < numSamples; ++i) {
        float outL = 0.0f;
        float outR = 0.0f;
        process(bufferL[i], bufferR[i], params, outL, outR);
        bufferL[i] = outL;
        bufferR[i] = outR;
    }
}

} // namespace pf02::dsp
