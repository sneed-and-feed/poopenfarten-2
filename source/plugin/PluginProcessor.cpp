#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>

// ============================================================================
// Constructor & Destructor
// ============================================================================
Pf02AudioProcessor::Pf02AudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), false)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      mApvts(*this, nullptr, "Parameters", pf02::parameters::createParameterLayout())
{
    initializeParameterPointers();

    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size(); ++i) {
        const juce::String paramIdStr(defs[i].idString.data(), defs[i].idString.size());
        mApvts.addParameterListener(paramIdStr, this);
    }
}

Pf02AudioProcessor::~Pf02AudioProcessor()
{
    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size(); ++i) {
        const juce::String paramIdStr(defs[i].idString.data(), defs[i].idString.size());
        mApvts.removeParameterListener(paramIdStr, this);
    }
}

void Pf02AudioProcessor::initializeParameterPointers() noexcept
{
    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size() && i < pf02::parameters::kNumParameters; ++i) {
        const juce::String paramIdStr(defs[i].idString.data(), defs[i].idString.size());
        mParamPointers[i] = mApvts.getRawParameterValue(paramIdStr);
        if (mParamPointers[i] != nullptr) {
            const float val = mParamPointers[i]->load(std::memory_order_relaxed);
            mLastParamValues[i] = val;
            mAudioEngine.setParameter(defs[i].id, val);
        } else {
            mLastParamValues[i] = defs[i].defaultValue;
        }
        mParamDirty[i].store(false, std::memory_order_relaxed);
    }
}

// ============================================================================
// Hard Real-Time Lifecycle
// ============================================================================
void Pf02AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mAudioEngine.prepare(sampleRate, samplesPerBlock);

    // Initial snapshot sync of all parameters to engine
    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size() && i < pf02::parameters::kNumParameters; ++i) {
        if (mParamPointers[i] != nullptr) {
            const float val = mParamPointers[i]->load(std::memory_order_relaxed);
            mLastParamValues[i] = val;
            mAudioEngine.setParameter(defs[i].id, val);
        }
    }
}

void Pf02AudioProcessor::releaseResources()
{
    mAudioEngine.reset();
}

void Pf02AudioProcessor::reset()
{
    mAudioEngine.reset();
}

bool Pf02AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput  = layouts.getMainInputChannelSet();

    // Synth instrument: Output must be Mono (1) or Stereo (2)
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo()) {
        return false;
    }

    // Input may be disabled or stereo/mono sidechain/direct
    if (mainInput.isDisabled() || mainInput == juce::AudioChannelSet::stereo() || mainInput == juce::AudioChannelSet::mono()) {
        return true;
    }

    return false;
}

// ============================================================================
// Real-Time Audio Callback (Guaranteed ZERO Allocations)
// ============================================================================
void Pf02AudioProcessor::syncDirtyParametersToEngine() noexcept
{
    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < pf02::parameters::kNumParameters; ++i) {
        if (mParamPointers[i] != nullptr) {
            const float currentVal = mParamPointers[i]->load(std::memory_order_relaxed);
            if (mParamDirty[i].exchange(false, std::memory_order_acquire) || currentVal != mLastParamValues[i]) {
                mLastParamValues[i] = currentVal;
                mAudioEngine.setParameter(defs[i].id, currentVal);
            }
        }
    }
}

void Pf02AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0) {
        return;
    }

    // 1. Synchronize dirty parameters into the physical modeling engine
    syncDirtyParametersToEngine();

    // 2. Drain lock-free SPSC performance commands from UI thread
    drainUiCommands();

    // 3. Synthesize audio block and parse MIDI via Pf02AudioEngine
    mAudioEngine.processBlock(buffer, midiMessages);
}

// ============================================================================
// Metadata Queries
// ============================================================================
const juce::String Pf02AudioProcessor::getName() const
{
    return "PF-02 Triaden-Synthesizer";
}

bool Pf02AudioProcessor::acceptsMidi() const
{
    return true;
}

bool Pf02AudioProcessor::producesMidi() const
{
    return false;
}

bool Pf02AudioProcessor::isMidiEffect() const
{
    return false;
}

double Pf02AudioProcessor::getTailLengthSeconds() const
{
    return 0.5;
}

// ============================================================================
// Preset Management (16 Factory Presets)
// ============================================================================
int Pf02AudioProcessor::getNumPrograms()
{
    return static_cast<int>(pf02::parameters::PresetManager::kNumFactoryPresets);
}

int Pf02AudioProcessor::getCurrentProgram()
{
    return mCurrentProgram;
}

void Pf02AudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms()) {
        return;
    }

    mCurrentProgram = index;
    const auto* preset = pf02::parameters::PresetManager::getPresetByIndex(static_cast<size_t>(index));
    if (preset == nullptr) {
        return;
    }

    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size(); ++i) {
        const juce::String paramIdStr(defs[i].idString.data(), defs[i].idString.size());
        if (auto* param = mApvts.getParameter(paramIdStr)) {
            const float targetVal = preset->getParam(defs[i].id);
            const float norm = (defs[i].type == pf02::parameters::ParameterType::Choice)
                                 ? param->convertTo0to1(std::round(targetVal))
                                 : param->convertTo0to1(targetVal);
            param->setValueNotifyingHost(std::clamp(norm, 0.0f, 1.0f));
        }
    }
}

const juce::String Pf02AudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < getNumPrograms()) {
        const auto* preset = pf02::parameters::PresetManager::getPresetByIndex(static_cast<size_t>(index));
        if (preset != nullptr) {
            return juce::String(preset->name);
        }
    }
    return "Standard-Patrone";
}

void Pf02AudioProcessor::changeProgramName(int, const juce::String&)
{
}

// ============================================================================
// State Save / Restore (XML / ValueTree)
// ============================================================================
void Pf02AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = mApvts.copyState();
    state.setProperty("currentProgram", mCurrentProgram, nullptr);
    state.setProperty("isPowerOn", mIsPowerOn.load(std::memory_order_relaxed), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Pf02AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(mApvts.state.getType())) {
        auto vt = juce::ValueTree::fromXml(*xmlState);
        if (vt.hasProperty("currentProgram")) {
            mCurrentProgram = std::clamp(static_cast<int>(vt.getProperty("currentProgram")), 0, getNumPrograms() - 1);
        }
        if (vt.hasProperty("isPowerOn")) {
            setPower(static_cast<bool>(vt.getProperty("isPowerOn")));
        } else {
            setPower(true);
        }
        mApvts.replaceState(vt);
    }
}

// ============================================================================
// APVTS Listener
// ============================================================================
void Pf02AudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    const auto& defs = pf02::parameters::getAllParameterDefs();
    for (size_t i = 0; i < defs.size() && i < pf02::parameters::kNumParameters; ++i) {
        if (parameterID == juce::String(defs[i].idString.data(), defs[i].idString.size())) {
            mParamDirty[i].store(true, std::memory_order_release);
            if (defs[i].id == pf02::parameters::ParameterId::PowerActive) {
                mIsPowerOn.store(newValue >= 0.5f, std::memory_order_relaxed);
            }
            break;
        }
    }
}

// ============================================================================
// Lock-Free SPSC Performance Triggers (UI Thread -> Audio Thread)
// ============================================================================
void Pf02AudioProcessor::enqueueUiCommand(const UiCommand& cmd) noexcept
{
    const size_t writeIdx = mCommandWriteIndex.load(std::memory_order_relaxed);
    const size_t readIdx = mCommandReadIndex.load(std::memory_order_acquire);
    if ((writeIdx - readIdx) >= kCommandQueueSize) {
        return; // Queue full
    }
    mCommandQueue[writeIdx & (kCommandQueueSize - 1)] = cmd;
    mCommandWriteIndex.store(writeIdx + 1, std::memory_order_release);
}

void Pf02AudioProcessor::drainUiCommands() noexcept
{
    size_t readIdx = mCommandReadIndex.load(std::memory_order_relaxed);
    const size_t writeIdx = mCommandWriteIndex.load(std::memory_order_acquire);

    while (readIdx != writeIdx) {
        const UiCommand cmd = mCommandQueue[readIdx & (kCommandQueueSize - 1)];
        switch (cmd.type) {
            case UiCommandType::NoteOn:
                mAudioEngine.noteOn(cmd.midiNote, cmd.velocity, cmd.freqHz);
                break;
            case UiCommandType::NoteOff:
                mAudioEngine.noteOff(cmd.midiNote, cmd.velocity);
                break;
            case UiCommandType::PitchBend:
                mAudioEngine.pitchBend(cmd.semitones);
                break;
            case UiCommandType::AllNotesOff:
                mAudioEngine.allNotesOff();
                break;
            case UiCommandType::KineticBurst: {
                switch (cmd.burstKind) {
                    case 1: mAudioEngine.triggerKineticBurst("breakaway", cmd.velocity); break;
                    case 2: mAudioEngine.triggerKineticBurst("splatter", cmd.velocity); break;
                    case 3: mAudioEngine.triggerKineticBurst("cavity", cmd.velocity); break;
                    default: mAudioEngine.triggerKineticBurst("impulse", cmd.velocity); break;
                }
                break;
            }
        }
        ++readIdx;
    }

    mCommandReadIndex.store(readIdx, std::memory_order_release);
}

void Pf02AudioProcessor::noteOn(int midiNote, float velocity, float freqHz) noexcept
{
    UiCommand cmd;
    cmd.type = UiCommandType::NoteOn;
    cmd.midiNote = midiNote;
    cmd.velocity = velocity;
    cmd.freqHz = freqHz;
    enqueueUiCommand(cmd);
}

void Pf02AudioProcessor::noteOff(int midiNote, float velocity) noexcept
{
    UiCommand cmd;
    cmd.type = UiCommandType::NoteOff;
    cmd.midiNote = midiNote;
    cmd.velocity = velocity;
    enqueueUiCommand(cmd);
}

void Pf02AudioProcessor::pitchBend(float semitones) noexcept
{
    UiCommand cmd;
    cmd.type = UiCommandType::PitchBend;
    cmd.semitones = semitones;
    enqueueUiCommand(cmd);
}

void Pf02AudioProcessor::allNotesOff() noexcept
{
    UiCommand cmd;
    cmd.type = UiCommandType::AllNotesOff;
    enqueueUiCommand(cmd);
}

void Pf02AudioProcessor::triggerKineticBurst(const juce::String& burstType, float velocity) noexcept
{
    UiCommand cmd;
    cmd.type = UiCommandType::KineticBurst;
    cmd.velocity = velocity;
    if (burstType.equalsIgnoreCase("breakaway")) {
        cmd.burstKind = 1;
    } else if (burstType.equalsIgnoreCase("splatter")) {
        cmd.burstKind = 2;
    } else if (burstType.equalsIgnoreCase("cavity")) {
        cmd.burstKind = 3;
    } else {
        cmd.burstKind = 0;
    }
    enqueueUiCommand(cmd);
}

void Pf02AudioProcessor::setMacro(const juce::String& macroId, float value) noexcept
{
    const float clampedVal = std::clamp(value, 0.0f, 1.0f);
    if (macroId == "macro_gut_squeeze" || macroId == "macroGutSqueeze") {
        if (auto* p = mApvts.getParameter(pf02::parameters::ParamIDs::macro_gut_squeeze.data())) {
            p->setValueNotifyingHost(clampedVal);
        }
    } else if (macroId == "macro_fluid_moisture" || macroId == "macroFluidMoisture") {
        if (auto* p = mApvts.getParameter(pf02::parameters::ParamIDs::macro_fluid_moisture.data())) {
            p->setValueNotifyingHost(clampedVal);
        }
    }
}

void Pf02AudioProcessor::setPower(bool powerOn) noexcept
{
    mIsPowerOn.store(powerOn, std::memory_order_relaxed);
    if (auto* p = mApvts.getParameter(pf02::parameters::ParamIDs::power_active.data())) {
        p->setValueNotifyingHost(powerOn ? 1.0f : 0.0f);
    }
}

bool Pf02AudioProcessor::isPower() const noexcept
{
    return mIsPowerOn.load(std::memory_order_relaxed);
}

// ============================================================================
// Editor Creation
// ============================================================================
bool Pf02AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* Pf02AudioProcessor::createEditor()
{
    return new Pf02AudioProcessorEditor(*this);
}

// ============================================================================
// JUCE Plugin Entry Point Export
// ============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Pf02AudioProcessor();
}
