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
    if (std::isfinite(freqHz) && freqHz > 0.0f) {
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
    mMidiNote = midiNote;
    mVelocity = std::clamp(velocity, 0.0f, 1.0f);
    mOnsetTime = 0; // Updated by VoiceManager timestamp

    // Pitch assignment: snap immediately unless re-articulating the same held note
    setTargetPitchMidi(static_cast<float>(midiNote), !wasHeldSameNote, freqHz);
    if (!wasHeldSameNote && mActive) {
        mModalMatrix.dampStates(0.25f);
    }

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

    if (burstType == "impulse") {
        mMucosalExciter.triggerSlap(safeVel, 2.8f);
    } else if (burstType == "breakaway") {
        mMucosalExciter.triggerSealBurst(safeVel * 2.0f);
    } else if (burstType == "splatter") {
        mAeroFluidEngine.triggerFlapBurst(safeVel, 0.9f, 30.0f);
    } else if (burstType == "cavity") {
        mMucosalExciter.triggerSealBurst(safeVel * 1.4f);
    } else {
        mMucosalExciter.triggerSlap(safeVel, 2.5f);
    }

    if (!wasHeld) {
        noteOn(33, safeVel);
        mEnvelope = std::max(mEnvelope, 0.95f * safeVel);
        noteOff(0.0f);
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
    mModalMatrix.dampStates(0.20f);
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

    // 1. Portamento frequency slew
    mCurrentFreqHz += mGlideCoeff * (mTargetFreqHz - mCurrentFreqHz);
    mCurrentFreqHz = dsp::flushDenormal(mCurrentFreqHz);
    mMucosalParams.frequencyHz = mCurrentFreqHz;
    mModalParams.fundamentalHz = mCurrentFreqHz;

    // 2. Compute dynamic colonic pressure drive
    auto localExciterParams = mMucosalParams;
    const float velocityScaling = 0.35f + 0.65f * mVelocity;
    localExciterParams.colonicPressure = std::clamp(
        mMucosalParams.colonicPressure * velocityScaling + externalPressureMod,
        0.0f, 1.0f
    );

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
        mFluidParams
    );

    // 5. Couple Excitation into 16-Pole Modal Resonator Matrix
    const float totalExcitation = exciterSig + fluidSig;
    mModalMatrix.processSample(totalExcitation, mModalParams, outL, outR);

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
        }
    }

    outL *= mEnvelope;
    outR *= mEnvelope;

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
