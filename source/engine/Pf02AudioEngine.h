#pragma once

#include "VoiceManager.h"
#include "../dsp/DspMath.h"
#include "../dsp/MasteringChain.h"
#include "../dsp/LorenzAttractor.h"
#include "../parameters/Parameters.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <string_view>
#include <cmath>

#if __has_include(<juce_audio_basics/juce_audio_basics.h>)
    #include <juce_audio_basics/juce_audio_basics.h>
#endif


namespace pf02::engine {

/**
 * VisualizerTelemetryFrame: 60 Hz Decimated Telemetry Record for UI Visualizers.
 * Matches PROJECT.md § Interface Contracts #2 and web/verify.mjs schema.
 */
struct VisualizerTelemetryFrame {
    std::array<float, 128> waveformL {};
    std::array<float, 128> waveformR {};
    float aperture           { 1.0f };
    float colonicPressure    { 0.70f };
    float airVelocity        { 0.0f };
    float bubbleIntensity    { 0.0f };
    bool  dropletTriggered   { false };
    float zenoFlux           { 0.0f };
    int   activeUnrollK      { 4 }; // 1, 4, 8, 16
    float lorenzX            { 0.0f };
    float lorenzY            { 0.0f };
    float lorenzZ            { 0.0f };
    std::array<float, 16> modalEnergies {};
    std::array<float, 16> triadEnergyFlow {};
    float sphericalNormDrift { 0.0f }; // Guaranteed < 1e-6
    float peakLevelDb        { -100.0f };
    float rmsLevelDb         { -100.0f };
};

/**
 * Pf02AudioEngine: Master audio engine coordinating voices, effects, and telemetry.
 *
 * Guarantees:
 * - Exactly zero audio-thread heap allocations.
 * - Lock-free Single-Producer Single-Consumer (SPSC) telemetry queue.
 * - Hardware denormal protection via ScopedNoDenormals.
 */
class Pf02AudioEngine {
public:
    static constexpr size_t kTelemetryQueueSize = 128;
    static constexpr size_t kMaxInternalBlockSize = 2048;

    Pf02AudioEngine() noexcept;
    ~Pf02AudioEngine() = default;

    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;

    // MIDI Lifecycle
    void noteOn(int midiNote, float velocity, float freqHz = 0.0f) noexcept;
    void noteOff(int midiNote, float velocity = 0.0f) noexcept;
    void pitchBend(float semitones) noexcept;
    void allNotesOff() noexcept;
    void triggerKineticBurst(std::string_view burstType, float velocity = 1.0f) noexcept;

    // Parameter distribution
    void setParameter(parameters::ParameterId id, float value) noexcept;
    void setParameter(std::string_view idStr, float value) noexcept;
    [[nodiscard]] float getParameter(parameters::ParameterId id) const noexcept;

    // Real-Time Audio Block Processing
    void processBlock(float* const* channelData, int numChannels, int numSamples) noexcept;

#if defined(PF02_HAS_JUCE) && PF02_HAS_JUCE
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) noexcept;
#endif

    // Lock-Free SPSC Telemetry Queue (Producer: audio thread, Consumer: UI thread)
    bool popTelemetry(VisualizerTelemetryFrame& outFrame) noexcept;

    // Status queries
    [[nodiscard]] bool isPowerActive() const noexcept { return mPowerActive; }
    [[nodiscard]] size_t getActiveVoiceCount() const noexcept { return mVoiceManager.getActiveVoiceCount(); }

private:
    void captureTelemetrySample(float sL, float sR) noexcept;
    void dispatchTelemetryFrame() noexcept;
    void applyMacroMappings() noexcept;

    float mSampleRate { 48000.0f };
    int   mMaxBlockSize { 512 };
    bool  mPowerActive { true };

    // Performance Macros
    float mMacroGutSqueeze { 0.50f };
    float mMacroFluidMoisture { 0.50f };

    // Subsystems
    VoiceManager           mVoiceManager;
    dsp::LorenzAttractor   mLorenzAttractor;
    dsp::MasteringChain    mMasteringChain;

    // Parameters storage
    std::array<float, parameters::kNumParameters> mParameters {};
    dsp::MasteringChainParams mMasteringParams {};

    // Internal Scratch Buffers (Zero heap allocation during processBlock)
    std::array<float, kMaxInternalBlockSize> mVoiceBufferL {};
    std::array<float, kMaxInternalBlockSize> mVoiceBufferR {};

    // SPSC Lock-Free Telemetry Ring Buffer
    std::array<VisualizerTelemetryFrame, kTelemetryQueueSize> mTelemetryQueue {};
    std::atomic<size_t> mTelemetryWriteIndex { 0 };
    std::atomic<size_t> mTelemetryReadIndex { 0 };

    // Decimation timer for 60 Hz telemetry
    int mTelemetrySampleCounter { 0 };
    int mTelemetryIntervalSamples { 800 }; // 48000 / 60

    // Rolling 128-sample waveform capture for oscilloscope
    std::array<float, 128> mWaveformWindowL {};
    std::array<float, 128> mWaveformWindowR {};
    size_t mWaveformWindowPos { 0 };
};

} // namespace pf02::engine
