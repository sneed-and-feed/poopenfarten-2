#include "VoiceManager.h"
#include <cmath>

namespace pf02::engine {

VoiceManager::VoiceManager() noexcept {
    for (size_t i = 0; i < kMaxVoices; ++i) {
        mVoices[i].setSeedOffset(i);
    }
    reset();
}

void VoiceManager::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    for (size_t i = 0; i < kMaxVoices; ++i) {
        mVoices[i].setSeedOffset(i);
        mVoices[i].prepare(mSampleRate);
    }
}

void VoiceManager::reset() noexcept {
    for (auto& v : mVoices) {
        v.reset();
    }
    mNoteStackCount = 0;
    mClock = 0;
    mHeadroomGain = 1.0f;
}

void VoiceManager::setVoicingMode(parameters::VoicingMode mode) noexcept {
    if (mVoicingMode != mode) {
        allNotesOff();
        mVoicingMode = mode;
    }
}

void VoiceManager::setNotePriority(NotePriority priority) noexcept {
    mNotePriority = priority;
}

void VoiceManager::setPortamentoGlideMs(float glideMs) noexcept {
    mGlideTimeMs = glideMs;
    for (auto& v : mVoices) {
        v.setGlideTimeMs(glideMs);
    }
}

void VoiceManager::setPitchBend(float semitones) noexcept {
    mPitchBendSemitones = semitones;
    for (auto& v : mVoices) {
        v.setPitchBendOffset(semitones);
    }
}

void VoiceManager::setParameter(parameters::ParameterId id, float value) noexcept {
    if (id == parameters::ParameterId::VoicingMode) {
        auto mode = static_cast<parameters::VoicingMode>(static_cast<int>(std::round(value)));
        setVoicingMode(mode);
    } else if (id == parameters::ParameterId::GlidePortamentoMs) {
        setPortamentoGlideMs(value);
    }

    for (auto& v : mVoices) {
        v.setParameter(id, value);
    }
}

void VoiceManager::setParameter(std::string_view idStr, float value) noexcept {
    const auto pId = parameters::getParameterIdByName(idStr);
    setParameter(pId, value);
}

void VoiceManager::setLorenzModulation(
    const std::array<float, 16>& freqMultipliers,
    float qSpreadFactor,
    float panOffsetRad) noexcept {
    for (auto& v : mVoices) {
        v.setLorenzModulation(freqMultipliers, qSpreadFactor, panOffsetRad);
    }
}

// ============================================================================
// Mono-Legato Note Priority Stack
// ============================================================================
void VoiceManager::pushNoteStack(int midiNote, float velocity, float freqHz) noexcept {
    // Check if already in stack, update velocity and freqHz if so
    for (size_t i = 0; i < mNoteStackCount; ++i) {
        if (mNoteStack[i].midiNote == midiNote) {
            mNoteStack[i].velocity = velocity;
            mNoteStack[i].freqHz = freqHz;
            // Move to top of stack
            NoteEntry entry = mNoteStack[i];
            for (size_t j = i; j + 1 < mNoteStackCount; ++j) {
                mNoteStack[j] = mNoteStack[j + 1];
            }
            mNoteStack[mNoteStackCount - 1] = entry;
            return;
        }
    }

    if (mNoteStackCount < kMaxNoteStack) {
        mNoteStack[mNoteStackCount++] = { midiNote, velocity, freqHz };
    } else {
        // Drop oldest
        for (size_t i = 0; i + 1 < kMaxNoteStack; ++i) {
            mNoteStack[i] = mNoteStack[i + 1];
        }
        mNoteStack[kMaxNoteStack - 1] = { midiNote, velocity, freqHz };
    }
}

void VoiceManager::removeNoteStack(int midiNote) noexcept {
    for (size_t i = 0; i < mNoteStackCount; ++i) {
        if (mNoteStack[i].midiNote == midiNote) {
            for (size_t j = i; j + 1 < mNoteStackCount; ++j) {
                mNoteStack[j] = mNoteStack[j + 1];
            }
            --mNoteStackCount;
            break;
        }
    }
}

int VoiceManager::getPriorityNote(float& outVelocity, float& outFreqHz) const noexcept {
    if (mNoteStackCount == 0) {
        outVelocity = 0.0f;
        outFreqHz = 0.0f;
        return -1;
    }

    switch (mNotePriority) {
        case NotePriority::Last: {
            const auto& entry = mNoteStack[mNoteStackCount - 1];
            outVelocity = entry.velocity;
            outFreqHz = entry.freqHz;
            return entry.midiNote;
        }
        case NotePriority::Low: {
            size_t minIdx = 0;
            int minNote = mNoteStack[0].midiNote;
            for (size_t i = 1; i < mNoteStackCount; ++i) {
                if (mNoteStack[i].midiNote < minNote) {
                    minNote = mNoteStack[i].midiNote;
                    minIdx = i;
                }
            }
            outVelocity = mNoteStack[minIdx].velocity;
            outFreqHz = mNoteStack[minIdx].freqHz;
            return minNote;
        }
        case NotePriority::High: {
            size_t maxIdx = 0;
            int maxNote = mNoteStack[0].midiNote;
            for (size_t i = 1; i < mNoteStackCount; ++i) {
                if (mNoteStack[i].midiNote > maxNote) {
                    maxNote = mNoteStack[i].midiNote;
                    maxIdx = i;
                }
            }
            outVelocity = mNoteStack[maxIdx].velocity;
            outFreqHz = mNoteStack[maxIdx].freqHz;
            return maxNote;
        }
    }
    outVelocity = mNoteStack[mNoteStackCount - 1].velocity;
    outFreqHz = mNoteStack[mNoteStackCount - 1].freqHz;
    return mNoteStack[mNoteStackCount - 1].midiNote;
}

// ============================================================================
// Polyphonic Voice Stealing
// ============================================================================
size_t VoiceManager::findVoiceToSteal() noexcept {
    // 1. First priority: lowest velocity among released voices
    size_t candidate = 0;
    float lowestScore = 1.0e12f;

    for (size_t i = 0; i < kMaxVoices; ++i) {
        const auto& v = mVoices[i];
        float score = 0.0f;

        if (!v.isNoteHeld()) {
            // Released voice is preferred for stealing
            score = v.getEnvelopeLevel() * 100.0f;
        } else {
            // Held voice: score by velocity and age (older = lower onset time)
            score = 1000.0f + v.getVelocity() * 50.0f + static_cast<float>(v.getOnsetTime() % 1000);
        }

        if (score < lowestScore) {
            lowestScore = score;
            candidate = i;
        }
    }
    return candidate;
}

// ============================================================================
// MIDI Note Lifecycle
// ============================================================================
void VoiceManager::noteOn(int midiNote, float velocity, float freqHz) noexcept {
    ++mClock;

    if (mVoicingMode == parameters::VoicingMode::MonoLegato) {
        const bool isLegato = (mNoteStackCount > 0) && mVoices[0].isActive() && mVoices[0].isNoteHeld();
        pushNoteStack(midiNote, velocity, freqHz);

        float prioVel = velocity;
        float prioFreqHz = freqHz;
        int prioNote = getPriorityNote(prioVel, prioFreqHz);

        if (!isLegato) {
            // Fresh note attack (or re-articulation from Release)
            mVoices[0].noteOn(prioNote, prioVel, prioFreqHz);
            mVoices[0].setOnsetTime(mClock);
        } else {
            // Legato portamento transition: glide frequency smoothly
            mVoices[0].setTargetPitchMidi(static_cast<float>(prioNote), false, prioFreqHz);
        }
    } else if (mVoicingMode == parameters::VoicingMode::Poly8) {
        // 1. Check if same note is already active on a voice
        for (size_t i = 0; i < kMaxVoices; ++i) {
            if (mVoices[i].isActive() && mVoices[i].getMidiNote() == midiNote) {
                mVoices[i].noteOn(midiNote, velocity, freqHz);
                mVoices[i].setOnsetTime(mClock);
                return;
            }
        }

        // 2. Search for a free/inactive voice
        for (size_t i = 0; i < kMaxVoices; ++i) {
            if (!mVoices[i].isActive()) {
                mVoices[i].noteOn(midiNote, velocity, freqHz);
                mVoices[i].setOnsetTime(mClock);
                return;
            }
        }

        // 3. All voices active: steal voice with smooth re-articulation modal damping
        const size_t stealIdx = findVoiceToSteal();
        mVoices[stealIdx].noteOn(midiNote, velocity, freqHz);
        mVoices[stealIdx].setOnsetTime(mClock);
    } else if (mVoicingMode == parameters::VoicingMode::StereoUnison) {
        const bool isLegato = (mNoteStackCount > 0) && mVoices[0].isActive() && mVoices[0].isNoteHeld();
        pushNoteStack(midiNote, velocity, freqHz);

        float prioVel = velocity;
        float prioFreqHz = freqHz;
        int prioNote = getPriorityNote(prioVel, prioFreqHz);

        // 4 voices assigned to a single note with detuning offsets and staggered initial phases
        for (size_t v = 0; v < 4; ++v) {
            const float detunedMidi = static_cast<float>(prioNote) + (kUnisonDetuneCents[v] / 100.0f);
            const float detunedFreq = (std::isfinite(prioFreqHz) && prioFreqHz > 0.0f)
                ? (prioFreqHz * std::pow(2.0f, kUnisonDetuneCents[v] / 1200.0f))
                : 0.0f;
            if (!isLegato) {
                mVoices[v].noteOn(prioNote, prioVel, detunedFreq);
                mVoices[v].setTargetPitchMidi(detunedMidi, true, detunedFreq);
                mVoices[v].setInitialPhaseOffset(static_cast<float>(v) * 0.25f);
                mVoices[v].setOnsetTime(mClock);
            } else {
                mVoices[v].setTargetPitchMidi(detunedMidi, false, detunedFreq);
            }
        }
    }
}

void VoiceManager::noteOff(int midiNote, float velocity) noexcept {
    if (mVoicingMode == parameters::VoicingMode::MonoLegato) {
        removeNoteStack(midiNote);
        if (mNoteStackCount > 0) {
            float prioVel = 0.0f;
            float prioFreqHz = 0.0f;
            int prioNote = getPriorityNote(prioVel, prioFreqHz);
            mVoices[0].setTargetPitchMidi(static_cast<float>(prioNote), false, prioFreqHz);
        } else {
            mVoices[0].noteOff(velocity);
        }
    } else if (mVoicingMode == parameters::VoicingMode::Poly8) {
        for (size_t i = 0; i < kMaxVoices; ++i) {
            if (mVoices[i].isActive() && mVoices[i].getMidiNote() == midiNote && mVoices[i].isNoteHeld()) {
                mVoices[i].noteOff(velocity);
            }
        }
    } else if (mVoicingMode == parameters::VoicingMode::StereoUnison) {
        removeNoteStack(midiNote);
        if (mNoteStackCount > 0) {
            float prioVel = 0.0f;
            float prioFreqHz = 0.0f;
            int prioNote = getPriorityNote(prioVel, prioFreqHz);
            for (size_t v = 0; v < 4; ++v) {
                const float detunedMidi = static_cast<float>(prioNote) + (kUnisonDetuneCents[v] / 100.0f);
                const float detunedFreq = (std::isfinite(prioFreqHz) && prioFreqHz > 0.0f)
                    ? (prioFreqHz * std::pow(2.0f, kUnisonDetuneCents[v] / 1200.0f))
                    : 0.0f;
                mVoices[v].setTargetPitchMidi(detunedMidi, false, detunedFreq);
            }
        } else {
            for (size_t v = 0; v < 4; ++v) {
                mVoices[v].noteOff(velocity);
            }
        }
    }
}

void VoiceManager::allNotesOff() noexcept {
    mNoteStackCount = 0;
    for (auto& v : mVoices) {
        v.noteOff(0.0f);
    }
}

void VoiceManager::triggerKineticBurst(std::string_view burstType, float velocity) noexcept {
    mVoices[0].triggerKineticBurst(burstType, velocity);
}

// ============================================================================
// Audio Block Processing (Zero Heap Allocations)
// ============================================================================
void VoiceManager::processBlock(
    float* bufferL,
    float* bufferR,
    int numSamples,
    float externalPressureMod) noexcept {
    if (numSamples <= 0 || bufferL == nullptr || bufferR == nullptr) {
        return;
    }

    bool anyActive = false;
    for (const auto& v : mVoices) {
        if (v.isActive()) {
            anyActive = true;
            break;
        }
    }
    if (!anyActive) {
        mHeadroomGain = 1.0f;
        return;
    }

    const bool isUnison = (mVoicingMode == parameters::VoicingMode::StereoUnison);

    for (int i = 0; i < numSamples; ++i) {
        float sumL = 0.0f;
        float sumR = 0.0f;
        size_t activeCount = 0;
        const bool unisonEnsembleActive = isUnison &&
            (mVoices[1].isActive() || mVoices[2].isActive() || mVoices[3].isActive());

        for (size_t vIdx = 0; vIdx < kMaxVoices; ++vIdx) {
            auto& v = mVoices[vIdx];
            if (v.isActive()) {
                float vL = 0.0f, vR = 0.0f;
                v.processSample(vL, vR, externalPressureMod);
                if (unisonEnsembleActive && vIdx < 4) {
                    const float pan = kUnisonPanOffsets[vIdx];
                    const float normPan = 0.5f * (pan + 1.0f);
                    const float gainL = std::cos(dsp::kHalfPi * normPan) * dsp::kSqrt2;
                    const float gainR = std::sin(dsp::kHalfPi * normPan) * dsp::kSqrt2;
                    sumL += vL * gainL;
                    sumR += vR * gainR;
                } else {
                    sumL += vL;
                    sumR += vR;
                }
                ++activeCount;
            }
        }

        const float targetScale = (activeCount > 1)
            ? (1.0f / std::sqrt(static_cast<float>(activeCount)))
            : 1.0f;
        mHeadroomGain += 0.008f * (targetScale - mHeadroomGain);

        bufferL[i] += sumL * mHeadroomGain;
        bufferR[i] += sumR * mHeadroomGain;
    }
}

// ============================================================================
// Voice & Telemetry Queries
// ============================================================================
size_t VoiceManager::getActiveVoiceCount() const noexcept {
    size_t count = 0;
    for (const auto& v : mVoices) {
        if (v.isActive()) ++count;
    }
    return count;
}

float VoiceManager::getActiveAperture() const noexcept {
    for (const auto& v : mVoices) {
        if (v.isActive()) return v.getAperture();
    }
    return 1.0f;
}

float VoiceManager::getActiveColonicPressure() const noexcept {
    for (const auto& v : mVoices) {
        if (v.isActive()) return v.getColonicPressure();
    }
    return 0.70f;
}

float VoiceManager::getActiveAirVelocity() const noexcept {
    for (const auto& v : mVoices) {
        if (v.isActive()) return v.getAirVelocity();
    }
    return 0.0f;
}

float VoiceManager::getActiveBubbleIntensity() const noexcept {
    for (const auto& v : mVoices) {
        if (v.isActive()) return v.getBubbleActivity();
    }
    return 0.0f;
}

bool VoiceManager::wasDropletTriggered() const noexcept {
    for (const auto& v : mVoices) {
        if (v.isActive() && v.wasDropletTriggered()) return true;
    }
    return false;
}

std::array<float, 16> VoiceManager::getModalEnergies() const noexcept {
    std::array<float, 16> sumEnergies {};
    for (const auto& v : mVoices) {
        if (v.isActive()) {
            const auto& m = v.getModalAmplitudes();
            for (size_t k = 0; k < 16; ++k) {
                sumEnergies[k] = std::max(sumEnergies[k], m[k]);
            }
        }
    }
    return sumEnergies;
}

std::array<float, 16> VoiceManager::getTriadEnergyFlow() const noexcept {
    for (const auto& v : mVoices) {
        if (v.isActive()) return v.getTriadEnergyFlow();
    }
    return {};
}

float VoiceManager::getLastRadialDrift() const noexcept {
    for (const auto& v : mVoices) {
        if (v.isActive()) return v.getLastRadialDrift();
    }
    return 0.0f;
}

} // namespace pf02::engine
