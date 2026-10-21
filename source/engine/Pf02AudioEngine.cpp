#include "Pf02AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace pf02::engine {

Pf02AudioEngine::Pf02AudioEngine() noexcept {
    const auto& defs = parameters::getAllParameterDefs();
    for (size_t i = 0; i < parameters::kNumParameters; ++i) {
        mParameters[i] = defs[i].defaultValue;
    }
    reset();
}

void Pf02AudioEngine::prepare(double sampleRate, int maxBlockSize) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mMaxBlockSize = std::max(16, maxBlockSize);

    mVoiceManager.prepare(mSampleRate);
    mLorenzAttractor.prepare(mSampleRate);
    mMasteringChain.prepare(mSampleRate);

    // 60 Hz decimation interval
    mTelemetryIntervalSamples = static_cast<int>(mSampleRate / 60.0f);
    if (mTelemetryIntervalSamples < 64) mTelemetryIntervalSamples = 64;

    mTelemetrySampleCounter = 0;
    mWaveformWindowPos = 0;
    mWaveformWindowL.fill(0.0f);
    mWaveformWindowR.fill(0.0f);

    // Apply initial parameter state
    const auto& defs = parameters::getAllParameterDefs();
    for (size_t i = 0; i < parameters::kNumParameters; ++i) {
        setParameter(defs[i].id, mParameters[i]);
    }
}

void Pf02AudioEngine::reset() noexcept {
    mVoiceManager.reset();
    mLorenzAttractor.reset();
    mMasteringChain.reset();

    mTelemetryWriteIndex.store(0, std::memory_order_relaxed);
    mTelemetryReadIndex.store(0, std::memory_order_relaxed);
    mTelemetrySampleCounter = 0;
    mWaveformWindowPos = 0;
    mWaveformWindowL.fill(0.0f);
    mWaveformWindowR.fill(0.0f);
}

void Pf02AudioEngine::noteOn(int midiNote, float velocity, float freqHz) noexcept {
    mVoiceManager.noteOn(midiNote, velocity, freqHz);
}

void Pf02AudioEngine::noteOff(int midiNote, float velocity) noexcept {
    mVoiceManager.noteOff(midiNote, velocity);
}

void Pf02AudioEngine::pitchBend(float semitones) noexcept {
    mVoiceManager.setPitchBend(semitones);
}

void Pf02AudioEngine::allNotesOff() noexcept {
    mVoiceManager.allNotesOff();
}

void Pf02AudioEngine::triggerKineticBurst(std::string_view burstType, float velocity) noexcept {
    mVoiceManager.triggerKineticBurst(burstType, velocity);
}

void Pf02AudioEngine::applyMacroMappings() noexcept {
    // Macro X: Bauchpresse (Gut Squeeze) [0, 1]
    // Modulates colonic pressure (+/-0.25), mucosal tension (+/-0.20), and colonic saturation drive (+/-25%)
    const float squeezeDelta = (mMacroGutSqueeze - 0.50f) * 2.0f; // [-1.0, +1.0]
    const float basePressure = mParameters[static_cast<size_t>(parameters::ParameterId::ColonicPressure)];
    const float modPressure = std::clamp(basePressure + squeezeDelta * 0.25f, 0.0f, 1.0f);
    mVoiceManager.setParameter(parameters::ParameterId::ColonicPressure, modPressure);

    const float baseTension = mParameters[static_cast<size_t>(parameters::ParameterId::MucosalTension)];
    const float modTension = std::clamp(baseTension + squeezeDelta * 0.20f, 0.0f, 1.0f);
    mVoiceManager.setParameter(parameters::ParameterId::MucosalTension, modTension);

    const float baseDrive = mParameters[static_cast<size_t>(parameters::ParameterId::ColonicSaturationDrive)];
    const float modDrive = std::clamp(baseDrive + squeezeDelta * 25.0f, 0.0f, 100.0f);
    mMasteringParams.colonicSaturationDrive = modDrive;

    // Macro Y: Schleimhaut-Feuchte (Multiphase Moisture) [0, 1]
    // Modulates multiphase moisture (+/-0.35), bubble density (+/-30%), and droplet rate (+/-15 Hz)
    const float moistureDelta = (mMacroFluidMoisture - 0.50f) * 2.0f; // [-1.0, +1.0]
    const float baseMoisture = mParameters[static_cast<size_t>(parameters::ParameterId::MultiphaseMoisture)];
    const float modMoisture = std::clamp(baseMoisture + moistureDelta * 0.35f, 0.0f, 1.0f);
    mVoiceManager.setParameter(parameters::ParameterId::MultiphaseMoisture, modMoisture);

    const float baseBubbles = mParameters[static_cast<size_t>(parameters::ParameterId::MinnaertBubbleDensity)];
    const float modBubbles = std::clamp(baseBubbles + moistureDelta * 30.0f, 0.0f, 100.0f);
    mVoiceManager.setParameter(parameters::ParameterId::MinnaertBubbleDensity, modBubbles);

    const float baseDroplets = mParameters[static_cast<size_t>(parameters::ParameterId::PoissonDropletRate)];
    const float modDroplets = std::clamp(baseDroplets + moistureDelta * 15.0f, 0.0f, 60.0f);
    mVoiceManager.setParameter(parameters::ParameterId::PoissonDropletRate, modDroplets);
}

void Pf02AudioEngine::setParameter(parameters::ParameterId id, float value) noexcept {
    using PId = parameters::ParameterId;
    const size_t idx = static_cast<size_t>(id);
    if (idx < mParameters.size()) {
        mParameters[idx] = value;
    }

    switch (id) {
        case PId::PowerActive:
            mPowerActive = (value >= 0.5f);
            break;

        case PId::MacroGutSqueeze:
            mMacroGutSqueeze = std::clamp(value, 0.0f, 1.0f);
            applyMacroMappings();
            break;

        case PId::MacroFluidMoisture:
            mMacroFluidMoisture = std::clamp(value, 0.0f, 1.0f);
            applyMacroMappings();
            break;

        case PId::LorenzOrbitRate:
            mLorenzAttractor.setRateHz(value);
            break;

        case PId::LorenzChaosDepth:
            mLorenzAttractor.setDepth(value * 0.01f);
            break;

        case PId::DimensionDEnable:
            mMasteringParams.dimensionDEnable = (value >= 0.5f);
            break;

        case PId::DimensionDMode:
            mMasteringParams.dimensionDMode = static_cast<dsp::DimensionChorusMode>(static_cast<int>(std::round(value)));
            break;

        case PId::DimensionDMix:
            mMasteringParams.dimensionDMix = value;
            break;

        case PId::ColonicSaturationDrive:
            mMasteringParams.colonicSaturationDrive = value;
            break;

        case PId::VisceralSubReinforce:
            mMasteringParams.visceralSubReinforceDb = value;
            break;

        case PId::MasterVolumeDb:
            mMasteringParams.masterVolumeDb = value;
            break;

        default:
            break;
    }

    // Forward parameter to voice manager
    mVoiceManager.setParameter(id, value);
}

void Pf02AudioEngine::setParameter(std::string_view idStr, float value) noexcept {
    const auto pId = parameters::getParameterIdByName(idStr);
    setParameter(pId, value);
}

float Pf02AudioEngine::getParameter(parameters::ParameterId id) const noexcept {
    const size_t idx = static_cast<size_t>(id);
    return (idx < mParameters.size()) ? mParameters[idx] : 0.0f;
}

// ============================================================================
// Real-Time Audio Block Processing (Zero Heap Allocations)
// ============================================================================
void Pf02AudioEngine::processBlock(float* const* channelData, int numChannels, int numSamples) noexcept {
    dsp::ScopedNoDenormals denormalGuard;

    if (channelData == nullptr || numChannels <= 0 || numSamples <= 0) {
        return;
    }

    // If master power is off, emit pure digital silence
    if (!mPowerActive) {
        for (int ch = 0; ch < numChannels; ++ch) {
            if (channelData[ch] != nullptr) {
                std::fill_n(channelData[ch], numSamples, 0.0f);
            }
        }
        return;
    }

    // Chunk processing in slices of max internal buffer size
    int samplesProcessed = 0;
    while (samplesProcessed < numSamples) {
        const int sliceSize = std::min(numSamples - samplesProcessed, static_cast<int>(kMaxInternalBlockSize));

        // 1. Advance 3D RK4 chaotic Lorenz attractor and update voice modulations
        mLorenzAttractor.step();
        mVoiceManager.setLorenzModulation(
            mLorenzAttractor.getFrequencyMultipliers(),
            mLorenzAttractor.getQSpreadFactor(),
            mLorenzAttractor.getSpatialPanOffset()
        );

        // 2. Clear scratch buffers (zero allocation)
        std::fill_n(mVoiceBufferL.data(), sliceSize, 0.0f);
        std::fill_n(mVoiceBufferR.data(), sliceSize, 0.0f);

        // 3. Render polyphonic physical voices
        mVoiceManager.processBlock(mVoiceBufferL.data(), mVoiceBufferR.data(), sliceSize);

        // 4. Pass stereo bus through analog MasteringChain
        mMasteringChain.processBlock(mVoiceBufferL.data(), mVoiceBufferR.data(), sliceSize, mMasteringParams);

        // 5. Copy to host output channels & collect visualizer telemetry
        float* outL = channelData[0] + samplesProcessed;
        float* outR = (numChannels > 1 && channelData[1] != nullptr) ? (channelData[1] + samplesProcessed) : nullptr;

        for (int i = 0; i < sliceSize; ++i) {
            const float sL = mVoiceBufferL[i];
            const float sR = mVoiceBufferR[i];

            outL[i] = sL;
            if (outR != nullptr) {
                outR[i] = sR;
            }

            captureTelemetrySample(sL, sR);
        }

        samplesProcessed += sliceSize;
    }
}

#if defined(PF02_HAS_JUCE) && PF02_HAS_JUCE
void Pf02AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) noexcept {
    // Process MIDI event stream
    for (const auto metadata : midi) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        } else if (msg.isNoteOff()) {
            noteOff(msg.getNoteNumber(), msg.getFloatVelocity());
        } else if (msg.isPitchWheel()) {
            const float normVal = static_cast<float>(msg.getPitchWheelValue() - 8192) / 8192.0f;
            pitchBend(normVal * 2.0f); // Default +/-2 semitones
        } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
            allNotesOff();
        }
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float* channelData[2] = {
        numChannels > 0 ? buffer.getWritePointer(0) : nullptr,
        numChannels > 1 ? buffer.getWritePointer(1) : nullptr
    };

    processBlock(channelData, numChannels, numSamples);
}
#endif

// ============================================================================
// Lock-Free SPSC Telemetry Pipeline (60 Hz Decimation)
// ============================================================================
void Pf02AudioEngine::captureTelemetrySample(float sL, float sR) noexcept {
    // Capture to rolling 128-sample oscilloscope buffer
    mWaveformWindowL[mWaveformWindowPos] = sL;
    mWaveformWindowR[mWaveformWindowPos] = sR;
    mWaveformWindowPos = (mWaveformWindowPos + 1) & 127;

    ++mTelemetrySampleCounter;
    if (mTelemetrySampleCounter >= mTelemetryIntervalSamples) {
        mTelemetrySampleCounter = 0;
        dispatchTelemetryFrame();
    }
}

void Pf02AudioEngine::dispatchTelemetryFrame() noexcept {
    VisualizerTelemetryFrame frame;

    // Linearize the 128-sample window starting from current write position
    for (size_t i = 0; i < 128; ++i) {
        const size_t idx = (mWaveformWindowPos + i) & 127;
        frame.waveformL[i] = mWaveformWindowL[idx];
        frame.waveformR[i] = mWaveformWindowR[idx];
    }

    frame.aperture           = mVoiceManager.getActiveAperture();
    frame.colonicPressure    = mVoiceManager.getActiveColonicPressure();
    frame.airVelocity        = mVoiceManager.getActiveAirVelocity();
    frame.bubbleIntensity    = mVoiceManager.getActiveBubbleIntensity();
    frame.dropletTriggered   = mVoiceManager.wasDropletTriggered();
    frame.zenoFlux           = std::clamp(std::abs(frame.airVelocity) * 0.05f, 0.0f, 1.0f);

    // Active unroll K
    int overrideChoice = static_cast<int>(std::round(mParameters[static_cast<size_t>(parameters::ParameterId::ZenoUnrollOverride)]));
    switch (overrideChoice) {
        case 1: frame.activeUnrollK = 1; break;
        case 2: frame.activeUnrollK = 4; break;
        case 3: frame.activeUnrollK = 8; break;
        case 4: frame.activeUnrollK = 16; break;
        default:
            // Auto Zeno scheduling based on zenoFlux
            if (frame.zenoFlux > 0.70f) frame.activeUnrollK = 16;
            else if (frame.zenoFlux > 0.45f) frame.activeUnrollK = 8;
            else if (frame.zenoFlux > 0.20f) frame.activeUnrollK = 4;
            else frame.activeUnrollK = 1;
            break;
    }

    frame.lorenzX            = mLorenzAttractor.getNormalizedX();
    frame.lorenzY            = mLorenzAttractor.getNormalizedY();
    frame.lorenzZ            = mLorenzAttractor.getNormalizedZ();
    frame.modalEnergies      = mVoiceManager.getModalEnergies();
    frame.triadEnergyFlow    = mVoiceManager.getTriadEnergyFlow();
    frame.sphericalNormDrift = mVoiceManager.getLastRadialDrift();
    frame.peakLevelDb        = mMasteringChain.getPeakLevelDb();
    frame.rmsLevelDb         = mMasteringChain.getRmsLevelDb();

    // Push into lock-free SPSC ring buffer
    const size_t writeIdx = mTelemetryWriteIndex.load(std::memory_order_relaxed);
    const size_t readIdx = mTelemetryReadIndex.load(std::memory_order_acquire);

    // If queue is full, overwrite oldest slot
    if ((writeIdx - readIdx) >= kTelemetryQueueSize) {
        mTelemetryReadIndex.store(readIdx + 1, std::memory_order_release);
    }

    mTelemetryQueue[writeIdx & (kTelemetryQueueSize - 1)] = frame;
    mTelemetryWriteIndex.store(writeIdx + 1, std::memory_order_release);
}

bool Pf02AudioEngine::popTelemetry(VisualizerTelemetryFrame& outFrame) noexcept {
    const size_t readIdx = mTelemetryReadIndex.load(std::memory_order_relaxed);
    const size_t writeIdx = mTelemetryWriteIndex.load(std::memory_order_acquire);

    if (readIdx == writeIdx) {
        return false; // Queue empty
    }

    outFrame = mTelemetryQueue[readIdx & (kTelemetryQueueSize - 1)];
    mTelemetryReadIndex.store(readIdx + 1, std::memory_order_release);
    return true;
}

} // namespace pf02::engine
