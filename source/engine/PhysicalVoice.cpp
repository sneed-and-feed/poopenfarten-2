#include "PhysicalVoice.h"

namespace pf02::engine {

PhysicalVoice::PhysicalVoice() noexcept {
    reset();
}

void PhysicalVoice::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mMucosalExciter.prepare(mSampleRate);
    mAeroFluidEngine.prepare(mSampleRate);
    mModalMatrix.prepare(mSampleRate);

    // Attack time ~ 2 ms
    mAttackCoeff = 1.0f - std::exp(-1.0f / (0.002f * mSampleRate));

    // Release default tau ~ 85 ms
    const float tauS = std::max(0.02f, mMucosalParams.vactrolSagTauMs * 0.001f);
    mReleaseCoeff = std::exp(-1.0f / (tauS * mSampleRate));

    // Steal de-click step over 5 ms
    mStealFadeStep = 1.0f / (0.005f * mSampleRate);

    updateFrequencyGlide();
}

void PhysicalVoice::reset() noexcept {
    mActive = false;
    mGateState = GateState::Off;
    mMidiNote = -1;
    mVelocity = 0.0f;
    mOnsetTime = 0;
    mEnvelope = 0.0f;
    mStealingFade = false;
    mStealFadeGain = 1.0f;
    mBurstMode = BurstMode::None;
    mBurstWasHeld = false;
    mBurstSamplePos = 0;
    mBurstDurationSamples = 0;
    mBurstIntensity = 0.0f;
    mBurstPhase = 0.0f;
    mRetrigDampSamples = 0;

    mMucosalExciter.reset();
    mAeroFluidEngine.reset();
    mModalMatrix.reset();
}

void PhysicalVoice::updateFrequencyGlide() noexcept {
    if (mGlideTimeMs <= 0.5f) {
        mGlideCoeff = 1.0f;
    } else {
        const float glideTauS = mGlideTimeMs * 0.001f;
        mGlideCoeff = 1.0f - std::exp(-1.0f / (glideTauS * mSampleRate));
    }
}

void PhysicalVoice::setTargetPitchMidi(float midiNote, bool immediate, float freqHz) noexcept {
    mTargetMidiNote = midiNote;
    if (dsp::isFiniteFloat(freqHz) && freqHz > 0.0f) {
        mBaseTargetFreqHz = freqHz;
        mTargetFreqHz = mBaseTargetFreqHz * std::pow(2.0f, mPitchBendOffsetSemitones / 12.0f);
    } else {
        mBaseTargetFreqHz = 0.0f;
        const float effectiveNote = mTargetMidiNote + mPitchBendOffsetSemitones;
        mTargetFreqHz = 440.0f * std::pow(2.0f, (effectiveNote - 69.0f) / 12.0f);
    }
    mTargetFreqHz = std::clamp(mTargetFreqHz, 7.0f, 2000.0f);

    if (immediate) {
        mCurrentFreqHz = mTargetFreqHz;
        mBaseMidiNote = mTargetMidiNote;
    }
}

void PhysicalVoice::setGlideTimeMs(float glideMs) noexcept {
    mGlideTimeMs = std::clamp(glideMs, 0.0f, 500.0f);
    updateFrequencyGlide();
}

void PhysicalVoice::setPitchBendOffset(float semitones) noexcept {
    mPitchBendOffsetSemitones = semitones;
    setTargetPitchMidi(mTargetMidiNote, false, mBaseTargetFreqHz);
}

void PhysicalVoice::noteOn(int midiNote, float velocity, float freqHz) noexcept {
    if (velocity <= 0.001f) {
        noteOff(0.0f);
        return;
    }

    const bool wasHeldSameNote = mActive && (mGateState == GateState::NoteOn) && (mMidiNote == midiNote);
    if (mActive && !wasHeldSameNote) {
        // Smooth per-sample modal damping over ~1.8 ms on voice steal or re-articulation (no single-sample step jump)
        mRetrigDampSamples = std::max(1, static_cast<int>(0.0018f * mSampleRate));
    }

    mMidiNote = midiNote;
    mVelocity = std::clamp(velocity, 0.0f, 1.0f);
    mOnsetTime = 0; // Updated by VoiceManager timestamp
    mBurstMode = BurstMode::None;
    mBurstWasHeld = false;
    mBurstSamplePos = 0;
    mBurstDurationSamples = 0;

    // Immediate pitch assignment for inactive voice; fast smooth slew when re-articulating an active voice
    setTargetPitchMidi(static_cast<float>(midiNote), !mActive, freqHz);

    // Initial mechanical excitation slap
    mMucosalExciter.triggerSlap(mVelocity, mMucosalParams.tissueHardness);

    // High velocity triggers seal breakaway burst
    if (mVelocity > 0.60f) {
        mMucosalExciter.triggerSealBurst(mVelocity);
    }

    // Trigger aerodynamic mucosal flap & droplets
    mAeroFluidEngine.triggerFlapBurst(
        mVelocity,
        mFluidParams.multiphaseMoisture,
        mFluidParams.poissonDropletRate
    );

    mGateState = GateState::NoteOn;
    mActive = true;
    mStealingFade = false;
    mStealFadeGain = 1.0f;
}

void PhysicalVoice::triggerKineticBurst(std::string_view burstType, float velocity) noexcept {
    const float safeVel = std::clamp(velocity, 0.1f, 1.0f);
    const bool wasHeld = isNoteHeld();

    mBurstWasHeld = wasHeld;
    mVelocity = std::max(mVelocity, safeVel);
    mBurstIntensity = safeVel;
    mBurstSamplePos = 0;
    mBurstPhase = 0.0f;
    mStealingFade = false;
    mStealFadeGain = 1.0f;

    if (!wasHeld) {
        if (mActive) {
            mRetrigDampSamples = std::max(1, static_cast<int>(0.0015f * mSampleRate));
        } else {
            mModalMatrix.dampStates(0.0f);
        }
        mActive = true;
        mGateState = GateState::Release;
        mEnvelope = std::max(mEnvelope, 0.96f * safeVel);
    }

    if (burstType == "impulse" || burstType == "IMPULS-SCHLAG") {
        // 1. IMPULS-SCHLAG: Deep Dirac/Hertzian sphincter impact thud (170 Hz -> 36.7 Hz fast pitch drop, 68 ms tight decay)
        mBurstMode = BurstMode::Impulse;
        mBurstDurationSamples = static_cast<int>(0.14f * mSampleRate);
        if (!wasHeld) {
            mMidiNote = 26;
            setTargetPitchMidi(26.0f, true, 36.71f); // D1
            mReleaseCoeff = std::exp(-1.0f / (0.068f * mSampleRate));
        }
        mMucosalExciter.triggerSlap(safeVel * 1.8f, 2.95f);
    } else if (burstType == "breakaway" || burstType == "KARNOPP-ABRISS") {
        // 2. KARNOPP-ABRISS: Violent 31 Hz stick-slip mucosal tear / fluttering raspberry rip (94 Hz -> 49 Hz, 195 ms decay)
        mBurstMode = BurstMode::Breakaway;
        mBurstDurationSamples = static_cast<int>(0.32f * mSampleRate);
        if (!wasHeld) {
            mMidiNote = 33;
            setTargetPitchMidi(33.0f, true, 55.0f); // A1
            mReleaseCoeff = std::exp(-1.0f / (0.195f * mSampleRate));
        }
        mMucosalExciter.triggerSealBurst(safeVel * 2.2f);
    } else if (burstType == "splatter" || burstType == "MINNAERT-PLOPP") {
        // 3. MINNAERT-PLOPP: Hydro-acoustic Minnaert bubble cluster & Poisson droplet cascade with upward pitch chirp
        mBurstMode = BurstMode::Splatter;
        mBurstDurationSamples = static_cast<int>(0.28f * mSampleRate);
        if (!wasHeld) {
            mMidiNote = 48;
            setTargetPitchMidi(48.0f, true, 130.81f); // C3
            mReleaseCoeff = std::exp(-1.0f / (0.165f * mSampleRate));
        }
        mAeroFluidEngine.triggerBubble(5.4f, safeVel * 0.95f, 1.00038f);
        mAeroFluidEngine.triggerBubble(3.6f, safeVel * 0.90f, 1.00032f);
        mAeroFluidEngine.triggerBubble(2.2f, safeVel * 0.85f, 1.00028f);
        mAeroFluidEngine.triggerDroplet(safeVel * 0.95f);
        mAeroFluidEngine.triggerDroplet(safeVel * 0.80f);
    } else if (burstType == "cavity" || burstType == "JORDAN-RESONANZ") {
        // 4. JORDAN-RESONANZ: Struck 2D Jordan-enclosure porcelain ceramic bowl resonance (146.8 Hz, high-Q ceramic ring, 360 ms decay)
        mBurstMode = BurstMode::Cavity;
        mBurstDurationSamples = static_cast<int>(0.55f * mSampleRate);
        if (!wasHeld) {
            mMidiNote = 50;
            setTargetPitchMidi(50.0f, true, 146.83f); // D3
            mReleaseCoeff = std::exp(-1.0f / (0.36f * mSampleRate));
            mModalMatrix.exciteCavityField(1.5f, 0.85f, 3);
        }
        mMucosalExciter.triggerSlap(safeVel * 1.4f, 2.85f);
    } else {
        mBurstMode = BurstMode::Impulse;
        mBurstDurationSamples = static_cast<int>(0.14f * mSampleRate);
        if (!wasHeld) {
            mMidiNote = 33;
            setTargetPitchMidi(33.0f, true, 55.0f);
            mReleaseCoeff = std::exp(-1.0f / (0.085f * mSampleRate));
        }
        mMucosalExciter.triggerSlap(safeVel, 2.8f);
    }
}

void PhysicalVoice::noteOff(float velocity) noexcept {
    (void)velocity;
    mGateState = GateState::Release;
    const float tauS = std::max(0.02f, mMucosalParams.vactrolSagTauMs * 0.001f);
    mReleaseCoeff = std::exp(-1.0f / (tauS * mSampleRate));
}

void PhysicalVoice::killQuick() noexcept {
    mStealingFade = true;
    mStealFadeGain = 1.0f;
}

void PhysicalVoice::setLorenzModulation(
    const std::array<float, 16>& freqMultipliers,
    float qSpreadFactor,
    float panOffsetRad) noexcept {
    mModalMatrix.setLorenzModulation(freqMultipliers, qSpreadFactor, panOffsetRad);
}

void PhysicalVoice::setParameter(parameters::ParameterId id, float value) noexcept {
    using PId = parameters::ParameterId;
    switch (id) {
        // Deck 01
        case PId::ColonicPressure:
            mMucosalParams.colonicPressure = value;
            break;
        case PId::MucosalTension:
            mMucosalParams.mucosalTension = value;
            break;
        case PId::RestingAperture:
            mMucosalParams.restingApertureMm = value;
            break;
        case PId::KarnoppStiction:
            mMucosalParams.karnoppStiction = value;
            break;
        case PId::StribeckBreakaway:
            mMucosalParams.stribeckBreakaway = value;
            break;
        case PId::TissueHardness:
            mMucosalParams.tissueHardness = value;
            break;
        case PId::ImpactRestitution:
            mMucosalParams.impactRestitution = value;
            break;
        case PId::VactrolSagTau:
            mMucosalParams.vactrolSagTauMs = value;
            if (mSampleRate > 100.0f) {
                const float tauS = std::max(0.02f, value * 0.001f);
                mReleaseCoeff = std::exp(-1.0f / (tauS * mSampleRate));
            }
            break;

        // Deck 02
        case PId::BeltramiVortexPitch:
            mFluidParams.beltramiVortexPitch = value;
            break;
        case PId::ReynoldsTurbulence:
            mFluidParams.reynoldsTurbulence = value;
            break;
        case PId::MultiphaseMoisture:
            mFluidParams.multiphaseMoisture = value;
            break;
        case PId::FluidViscosity:
            mFluidParams.fluidViscosity = value;
            break;
        case PId::MinnaertBubbleDensity:
            mFluidParams.minnaertBubbleDensity = value;
            break;
        case PId::PoissonDropletRate:
            mFluidParams.poissonDropletRate = value;
            break;
        case PId::EuclideanPeristalsisK:
            mFluidParams.euclideanPeristalsisK = static_cast<int>(std::round(value));
            break;
        case PId::EuclideanPeristalsisN:
            mFluidParams.euclideanPeristalsisN = static_cast<int>(std::round(value));
            break;

        // Deck 03
        case PId::TriadCouplingGain:
            mModalParams.triadCouplingGain = value;
            break;
        case PId::So16SkewGenerator:
            mModalParams.so16SkewGenerator = value;
            break;
        case PId::SphericalRetraction:
            mModalParams.sphericalRetraction = (value >= 0.5f);
            break;
        case PId::HouseholderCoupling:
            mModalParams.householderCoupling = value;
            break;
        case PId::GeometricManifold:
            mModalParams.manifold = static_cast<dsp::GeometricManifold>(static_cast<int>(std::round(value)));
            break;
        case PId::DampingProfile:
            mModalParams.damping = static_cast<dsp::DampingProfile>(static_cast<int>(std::round(value)));
            break;
        case PId::PorcelainCavityMix:
            mModalParams.porcelainCavityMix = value;
            break;
        case PId::PorcelainBoundaryMode:
            mModalParams.porcelainBoundaryMode = static_cast<int>(std::round(value));
            break;

        // Deck 04
        case PId::ZenoUnrollOverride: {
            int kChoice = static_cast<int>(std::round(value));
            int activeK = 4;
            switch (kChoice) {
                case 1: activeK = 1; break;
                case 2: activeK = 4; break;
                case 3: activeK = 8; break;
                case 4: activeK = 16; break;
                default: activeK = 4; break;
            }
            mModalParams.zenoUnrollK = activeK;
            break;
        }

        // Deck 06
        case PId::GoldenStereoSpread:
            mModalParams.stereoSpread = value;
            break;
        case PId::GlidePortamentoMs:
            setGlideTimeMs(value);
            break;

        default:
            break;
    }
}

void PhysicalVoice::setParameter(std::string_view idStr, float value) noexcept {
    const auto pId = parameters::getParameterIdByName(idStr);
    setParameter(pId, value);
}

float PhysicalVoice::processSample(float externalPressureMod) noexcept {
    float outL = 0.0f, outR = 0.0f;
    processSample(outL, outR, externalPressureMod);
    return (outL + outR) * 0.5f;
}

void PhysicalVoice::processSample(float& outL, float& outR, float externalPressureMod) noexcept {
    if (!mActive) {
        outL = 0.0f;
        outR = 0.0f;
        return;
    }

    // 1. Portamento frequency slew & smooth re-articulation modal damping
    if (mRetrigDampSamples > 0) {
        --mRetrigDampSamples;
        mModalMatrix.dampStates(0.985f);
        mCurrentFreqHz += std::max(mGlideCoeff, 0.18f) * (mTargetFreqHz - mCurrentFreqHz);
    } else {
        mCurrentFreqHz += mGlideCoeff * (mTargetFreqHz - mCurrentFreqHz);
    }
    mCurrentFreqHz = dsp::flushDenormal(mCurrentFreqHz);
    mMucosalParams.frequencyHz = mCurrentFreqHz;
    mModalParams.fundamentalHz = mCurrentFreqHz;

    // 2. Compute dynamic colonic pressure drive
    auto localExciterParams = mMucosalParams;
    auto localFluidParams = mFluidParams;
    auto localModalParams = mModalParams;
    const float velocityScaling = 0.35f + 0.65f * mVelocity;
    const float baseFreq = mCurrentFreqHz;
    const float basePressure = std::clamp(
        mMucosalParams.colonicPressure * velocityScaling + externalPressureMod,
        0.0f, 1.0f
    );
    localExciterParams.colonicPressure = basePressure;

    float burstModalInject = 0.0f;
    float burstDirectL = 0.0f;
    float burstDirectR = 0.0f;
    float fluidMixBoost = 0.25f;
    float standaloneAttackGain = 1.0f;

    if (mBurstMode != BurstMode::None) {
        const bool inActiveWindow = mBurstSamplePos < mBurstDurationSamples;
        const int clampedPos = std::min(mBurstSamplePos, mBurstDurationSamples);
        const float tSec = static_cast<float>(clampedPos) / mSampleRate;
        const float bInt = mBurstIntensity;
        const float attackDur = mBurstWasHeld ? 0.0030f : 0.0014f;
        const float attackWin = 0.5f * (1.0f - std::cos(std::min(1.0f, tSec / attackDur) * dsp::kPi));
        if (!mBurstWasHeld && inActiveWindow && tSec < attackDur) {
            standaloneAttackGain = attackWin;
        }

        const float releaseStart = 0.72f * static_cast<float>(mBurstDurationSamples);
        const float releaseWin = !inActiveWindow
            ? 0.0f
            : ((static_cast<float>(mBurstSamplePos) > releaseStart)
                ? (0.5f * (1.0f + std::cos(dsp::kPi * (static_cast<float>(mBurstSamplePos) - releaseStart) / std::max(1.0f, static_cast<float>(mBurstDurationSamples) - releaseStart))))
                : 1.0f);
        const float burstWin = mBurstWasHeld ? (attackWin * releaseWin) : releaseWin;
        const float paramBlend = mBurstWasHeld ? burstWin : 1.0f;

        if (mBurstMode == BurstMode::Impulse) {
            // 1. IMPULS-SCHLAG: Fast exponential pitch drop (172 Hz -> 36.7 Hz) + Hertzian impact shockwave
            const float fSweep = 36.71f + 135.0f * std::exp(-tSec / 0.015f);
            if (inActiveWindow) {
                mBurstPhase += (dsp::kTwoPi * fSweep) / mSampleRate;
                if (mBurstPhase >= dsp::kTwoPi) mBurstPhase -= dsp::kTwoPi;
            }

            const float subThud = std::sin(mBurstPhase) * attackWin * releaseWin * std::exp(-tSec / 0.036f) * bInt;
            const float diracClick = std::sin(dsp::kTwoPi * 680.0f * tSec) * attackWin * releaseWin * std::exp(-tSec / 0.0024f) * bInt;

            localExciterParams.frequencyHz = baseFreq + paramBlend * (fSweep - baseFreq);
            localModalParams.fundamentalHz = baseFreq + paramBlend * (fSweep - baseFreq);
            if (!mBurstWasHeld) {
                localModalParams.manifold = dsp::GeometricManifold::SigmoidHorn;
                localModalParams.damping = dsp::DampingProfile::StiffCartilage;
            }
            localExciterParams.tissueHardness = mMucosalParams.tissueHardness + paramBlend * (2.95f - mMucosalParams.tissueHardness);
            const float bPress = 0.36f * std::exp(-tSec / 0.028f) * bInt;
            localExciterParams.colonicPressure = basePressure + paramBlend * (bPress - basePressure);
            localFluidParams.multiphaseMoisture = mFluidParams.multiphaseMoisture * (1.0f - paramBlend);
            localFluidParams.poissonDropletRate = mFluidParams.poissonDropletRate * (1.0f - paramBlend);

            burstModalInject = subThud * 0.95f + diracClick * 0.70f;
            burstDirectL = subThud * 0.92f + diracClick * 0.28f;
            burstDirectR = subThud * 0.92f + diracClick * 0.28f;
            fluidMixBoost = 0.25f + paramBlend * (0.0f - 0.25f);
        } else if (mBurstMode == BurstMode::Breakaway) {
            // 2. KARNOPP-ABRISS: Violent 31 Hz stick-slip friction flutter & mucosal raspberry tear
            constexpr float flutterRate = 31.0f;
            const int flutterCycleSamples = std::max(1, static_cast<int>(mSampleRate / flutterRate));
            const float flutterPhase = std::fmod(tSec * flutterRate, 1.0f);

            if (inActiveWindow && mBurstSamplePos > 0 && (mBurstSamplePos % flutterCycleSamples == 0) && tSec < 0.23f) {
                mMucosalExciter.triggerSealBurst(bInt * 1.65f * releaseWin);
            }

            const float fTear = (52.0f + 42.0f * std::exp(-tSec / 0.16f)) * (0.84f + 0.32f * std::cos(dsp::kTwoPi * flutterPhase));
            if (inActiveWindow) {
                mBurstPhase += (dsp::kTwoPi * fTear) / mSampleRate;
                if (mBurstPhase >= dsp::kTwoPi) mBurstPhase -= dsp::kTwoPi;
            }

            const float stickSlipGate = 0.54f + 0.46f * std::tanh(3.2f * std::sin(dsp::kTwoPi * flutterPhase));
            const float rawHarmonics = std::sin(mBurstPhase) +
                                       0.75f * std::sin(2.0f * mBurstPhase) +
                                       0.50f * std::sin(3.0f * mBurstPhase) +
                                       0.30f * std::sin(4.0f * mBurstPhase);
            const float tearWave = std::tanh(1.65f * rawHarmonics) * stickSlipGate * attackWin * releaseWin * std::exp(-tSec / 0.21f) * bInt;

            localExciterParams.frequencyHz = baseFreq + paramBlend * (fTear - baseFreq);
            localModalParams.fundamentalHz = baseFreq + paramBlend * (fTear - baseFreq);
            const float bPress = std::clamp(0.95f * stickSlipGate * bInt, 0.0f, 1.0f);
            localExciterParams.colonicPressure = basePressure + paramBlend * (bPress - basePressure);
            localExciterParams.karnoppStiction = mMucosalParams.karnoppStiction + paramBlend * (0.98f - mMucosalParams.karnoppStiction);
            localExciterParams.stribeckBreakaway = mMucosalParams.stribeckBreakaway + paramBlend * (7.8f - mMucosalParams.stribeckBreakaway);
            localExciterParams.mucosalTension = mMucosalParams.mucosalTension + paramBlend * (0.84f - mMucosalParams.mucosalTension);
            localExciterParams.restingApertureMm = mMucosalParams.restingApertureMm + paramBlend * (0.35f - mMucosalParams.restingApertureMm);
            if (!mBurstWasHeld) {
                localModalParams.manifold = dsp::GeometricManifold::ChladniRing;
                localModalParams.damping = dsp::DampingProfile::WetElastomer;
            }
            localModalParams.triadCouplingGain = mModalParams.triadCouplingGain + paramBlend * (1.55f - mModalParams.triadCouplingGain);

            burstModalInject = tearWave * 0.95f;
            burstDirectL = tearWave * 0.52f;
            burstDirectR = -tearWave * 0.46f;
        } else if (mBurstMode == BurstMode::Splatter) {
            // 3. MINNAERT-PLOPP: Hydro-acoustic Minnaert bubble cascade & upward-chirping cavity plop
            const int cascadeInterval = std::max(1, static_cast<int>(0.014f * mSampleRate));
            if (inActiveWindow && mBurstSamplePos > 0 && (mBurstSamplePos % cascadeInterval == 0) && tSec < 0.20f) {
                const int stepIdx = mBurstSamplePos / cascadeInterval;
                const float rMm = 1.1f + 4.4f * (static_cast<float>((stepIdx * 5 + 3) % 11) / 10.0f);
                mAeroFluidEngine.triggerBubble(rMm, bInt * 0.88f * releaseWin, 1.00036f);
                mAeroFluidEngine.triggerDroplet(bInt * 0.65f * releaseWin);
            }

            const float chirpProgress = std::min(1.0f, tSec / 0.065f);
            const float fPlop = 175.0f + 365.0f * (chirpProgress * chirpProgress);
            if (inActiveWindow) {
                mBurstPhase += (dsp::kTwoPi * fPlop) / mSampleRate;
                if (mBurstPhase >= dsp::kTwoPi) mBurstPhase -= dsp::kTwoPi;
            }

            const float plopEnv = std::sin(std::min(1.0f, tSec / 0.004f) * dsp::kHalfPi) * releaseWin * std::exp(-tSec / 0.052f);
            const float plopWave = std::sin(mBurstPhase) * plopEnv * bInt;

            localExciterParams.frequencyHz = baseFreq + paramBlend * (fPlop - baseFreq);
            localModalParams.fundamentalHz = baseFreq + paramBlend * (fPlop - baseFreq);
            const float bPress = 0.08f * std::exp(-tSec / 0.030f);
            localExciterParams.colonicPressure = basePressure + paramBlend * (bPress - basePressure);
            localFluidParams.multiphaseMoisture = mFluidParams.multiphaseMoisture + paramBlend * (0.98f - mFluidParams.multiphaseMoisture);
            if (!mBurstWasHeld) {
                localModalParams.damping = dsp::DampingProfile::WetElastomer;
                localModalParams.porcelainBoundaryMode = 2;
            }
            localModalParams.porcelainCavityMix = mModalParams.porcelainCavityMix + paramBlend * (65.0f - mModalParams.porcelainCavityMix);

            burstModalInject = plopWave * 0.52f;
            burstDirectL = plopWave * 0.38f;
            burstDirectR = plopWave * 0.34f;
            fluidMixBoost = 0.25f + burstWin * (0.88f - 0.25f);
        } else if (mBurstMode == BurstMode::Cavity) {
            // 4. JORDAN-RESONANZ: Struck 2D Jordan-enclosure porcelain ceramic bowl standing waves
            constexpr float fBowl = 146.83f;
            localExciterParams.frequencyHz = baseFreq + paramBlend * (fBowl - baseFreq);
            localModalParams.fundamentalHz = baseFreq + paramBlend * (fBowl - baseFreq);
            if (!mBurstWasHeld) {
                localModalParams.manifold = dsp::GeometricManifold::PorcelainBowl;
                localModalParams.damping = dsp::DampingProfile::BellBronze;
                localModalParams.porcelainBoundaryMode = 3;
            }
            localModalParams.porcelainCavityMix = mModalParams.porcelainCavityMix + paramBlend * (100.0f - mModalParams.porcelainCavityMix);
            localModalParams.householderCoupling = mModalParams.householderCoupling + paramBlend * (0.82f - mModalParams.householderCoupling);
            localModalParams.triadCouplingGain = mModalParams.triadCouplingGain + paramBlend * (0.35f - mModalParams.triadCouplingGain);
            const float bPress = (tSec < 0.008f) ? (0.45f * bInt) : 0.0f;
            localExciterParams.colonicPressure = basePressure + paramBlend * (bPress - basePressure);

            const float ceramicRingL = (
                0.55f * std::sin(dsp::kTwoPi * 146.83f * tSec) * std::exp(-tSec / 0.26f) +
                0.42f * std::sin(dsp::kTwoPi * 221.71f * tSec + 0.4f) * std::exp(-tSec / 0.22f) +
                0.32f * std::sin(dsp::kTwoPi * 314.22f * tSec + 1.1f) * std::exp(-tSec / 0.18f) +
                0.22f * std::sin(dsp::kTwoPi * 530.05f * tSec + 2.0f) * std::exp(-tSec / 0.14f)
            ) * attackWin * releaseWin * bInt;
            const float ceramicRingR = (
                0.55f * std::sin(dsp::kTwoPi * 146.83f * tSec) * std::exp(-tSec / 0.26f) -
                0.42f * std::sin(dsp::kTwoPi * 221.71f * tSec + 0.4f) * std::exp(-tSec / 0.22f) +
                0.32f * std::sin(dsp::kTwoPi * 314.22f * tSec + 1.1f) * std::exp(-tSec / 0.18f) -
                0.22f * std::sin(dsp::kTwoPi * 530.05f * tSec + 2.0f) * std::exp(-tSec / 0.14f)
            ) * attackWin * releaseWin * bInt;

            if (tSec < 0.018f) {
                const float strikeEnv = attackWin * std::exp(-tSec / 0.0032f);
                burstModalInject = ceramicRingL * strikeEnv * 1.35f;
            }
            burstDirectL = ceramicRingL * 0.44f;
            burstDirectR = ceramicRingR * 0.44f;
            fluidMixBoost = 0.25f + burstWin * (0.06f - 0.25f);
        }

        if (inActiveWindow) {
            ++mBurstSamplePos;
            if (mBurstSamplePos >= mBurstDurationSamples && mBurstWasHeld) {
                mBurstMode = BurstMode::None;
            }
        }
    }

    // 3. Process Mucosal Valve Excitation
    float airVelocity = 0.0f;
    float aperture = 0.0f;
    const float exciterSig = mMucosalExciter.processSample(
        localExciterParams,
        airVelocity,
        aperture
    );

    // 4. Process Multiphase AeroFluid Flow
    const float fluidSig = mAeroFluidEngine.processSample(
        airVelocity,
        aperture,
        0.0f, // external zeno flux
        localFluidParams
    );

    // 5. Couple Excitation into 16-Pole Modal Resonator Matrix
    const float totalExcitation = exciterSig + fluidSig + burstModalInject;
    mModalMatrix.processSample(totalExcitation, localModalParams, outL, outR);

    // 6. Local Vactrol Envelope Gate Lifecycle
    if (mGateState == GateState::NoteOn) {
        mEnvelope += mAttackCoeff * (1.0f - mEnvelope);
    } else if (mGateState == GateState::Release) {
        mEnvelope *= mReleaseCoeff;
        mEnvelope = dsp::flushDenormal(mEnvelope);
        if (mEnvelope < 1.0e-4f) {
            mEnvelope = 0.0f;
            mActive = false;
            mGateState = GateState::Off;
            mBurstMode = BurstMode::None;
            mBurstWasHeld = false;
        }
    }

    outL = (outL + fluidSig * fluidMixBoost + burstDirectL) * mEnvelope * standaloneAttackGain;
    outR = (outR + fluidSig * fluidMixBoost * 0.92f + burstDirectR) * mEnvelope * standaloneAttackGain;

    // 7. Steal De-Click Linear/Cosine Fade-Out
    if (mStealingFade) {
        mStealFadeGain -= mStealFadeStep;
        if (mStealFadeGain <= 0.0f) {
            mStealFadeGain = 0.0f;
            mStealingFade = false;
            mActive = false;
            mGateState = GateState::Off;
        }
        outL *= mStealFadeGain;
        outR *= mStealFadeGain;
    }

    outL = dsp::flushDenormal(outL);
    outR = dsp::flushDenormal(outR);
}

} // namespace pf02::engine
