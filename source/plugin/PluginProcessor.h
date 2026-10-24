#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../engine/Pf02AudioEngine.h"
#include "../parameters/Parameters.h"
#include "../parameters/PresetManager.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>

/**
 * Pf02AudioProcessor: Full JUCE 8 AudioProcessor for Poopenfarten II
 * (PF-02 · Darm-Akustischer & Ultrametrischer Triaden-Synthesizer).
 *
 * Capabilities:
 * - Wraps real-time pf02::engine::Pf02AudioEngine.
 * - Thread-safe APVTS parameter sync and parameterChanged listener.
 * - MIDI processing in processBlock (Note On, Note Off, Pitch Bend, All Notes Off).
 * - PresetManager integration (16 curated factory presets, RFC 8259 JSON import/export).
 * - State save/restore via XML/ValueTree.
 * - Lock-free SPSC telemetry pop for 60 FPS CRT visualizer.
 * - Hard real-time audio thread: Exactly ZERO heap allocations in processBlock.
 */
class Pf02AudioProcessor : public juce::AudioProcessor,
                           public juce::AudioProcessorValueTreeState::Listener
{
public:
    Pf02AudioProcessor();
    ~Pf02AudioProcessor() override;

    // Hard real-time lifecycle
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // Audio callback: Guaranteed 0 heap allocations, 0 locks, block-level ScopedNoDenormals
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // Program / Preset Management
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    // State Serialization
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // APVTS Listener callback
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Accessors
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return mApvts; }
    pf02::engine::Pf02AudioEngine& getEngine() noexcept { return mAudioEngine; }

    // Direct performance triggers
    void noteOn(int midiNote, float velocity, float freqHz = 0.0f) noexcept;
    void noteOff(int midiNote, float velocity = 0.0f) noexcept;
    void pitchBend(float semitones) noexcept;
    void allNotesOff() noexcept;
    void triggerKineticBurst(const juce::String& burstType, float velocity = 1.0f) noexcept;

    // Performance Macros
    void setMacro(const juce::String& macroId, float value) noexcept;

    // Power switch
    void setPower(bool powerOn) noexcept;
    [[nodiscard]] bool isPower() const noexcept;

    // Lock-Free SPSC Telemetry Access for 60 FPS CRT Visualizer
    bool popTelemetry(pf02::engine::VisualizerTelemetryFrame& frame) noexcept {
        return mAudioEngine.popTelemetry(frame);
    }

private:
    enum class UiCommandType : uint8_t {
        NoteOn = 0,
        NoteOff,
        PitchBend,
        AllNotesOff,
        KineticBurst
    };

    struct UiCommand {
        UiCommandType type { UiCommandType::NoteOn };
        int           midiNote { 60 };
        float         velocity { 0.8f };
        float         freqHz { 0.0f };
        float         semitones { 0.0f };
        uint8_t       burstKind { 0 }; // 0: impulse, 1: breakaway, 2: splatter, 3: cavity
    };

    static constexpr size_t kCommandQueueSize = 256;

    void enqueueUiCommand(const UiCommand& cmd) noexcept;
    void drainUiCommands() noexcept;
    void syncDirtyParametersToEngine() noexcept;
    void initializeParameterPointers() noexcept;

    juce::AudioProcessorValueTreeState mApvts;
    pf02::engine::Pf02AudioEngine mAudioEngine;

    // Lock-free SPSC UI -> Audio performance command queue
    std::array<UiCommand, kCommandQueueSize> mCommandQueue {};
    std::atomic<size_t> mCommandWriteIndex { 0 };
    std::atomic<size_t> mCommandReadIndex { 0 };

    // Raw atomic parameter value pointers for lock-free audio thread reads
    std::array<std::atomic<float>*, pf02::parameters::kNumParameters> mParamPointers {};
    std::array<float, pf02::parameters::kNumParameters> mLastParamValues {};
    std::array<std::atomic<bool>, pf02::parameters::kNumParameters> mParamDirty {};

    std::atomic<bool> mIsPowerOn { true };
    int mCurrentProgram { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Pf02AudioProcessor)
};
