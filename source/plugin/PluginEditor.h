#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_data_structures/juce_data_structures.h>
#include "PluginProcessor.h"

#include <atomic>
#include <array>
#include <memory>
#include <optional>

/**
 * Pf02AudioProcessorEditor: JUCE 8 WebBrowserComponent GUI Editor for Poopenfarten II
 * (PF-02 · Darm-Akustischer & Ultrametrischer Triaden-Synthesizer).
 *
 * Hosts Edge WebView2 on Windows (WebKit on macOS), serving embedded zip binary data
 * from web_assets_pf02.zip via juce::ResourceProvider.
 *
 * Provides bidirectional JSON message bridge:
 * - paramChange, kineticBurst, noteOn, noteOff, pitchBend, macroChange, presetChange
 * - 60 Hz decimation telemetry push for 4-mode vector CRT oscilloscope
 */
class Pf02AudioProcessorEditor : public juce::AudioProcessorEditor,
                                 public juce::AudioProcessorValueTreeState::Listener,
                                 private juce::Timer
{
public:
    explicit Pf02AudioProcessorEditor(Pf02AudioProcessor&);
    ~Pf02AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // APVTS Listener callback (detects host automation and notifies web UI)
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Web integration & message handlers
    void handleParamChangeFromWeb(const juce::var& data);
    void handleKineticBurstFromWeb(const juce::var& data);
    void handleNoteOnFromWeb(const juce::var& data);
    void handleNoteOffFromWeb(const juce::var& data);
    void handlePitchBendFromWeb(const juce::var& data);
    void handleMacroChangeFromWeb(const juce::var& data);
    void handlePresetChangeFromWeb(const juce::var& data);

    void sendParameterUpdateToWeb(const juce::String& paramId, float newValue);
    void syncAllParametersToWeb();

    // Resource provider for embedded zip / local files
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

private:
    void timerCallback() override;

    static juce::WebBrowserComponent::Options createWebOptions(Pf02AudioProcessorEditor& editor);
    void sendTelemetryToWeb();

    Pf02AudioProcessor& processorRef;
    std::unique_ptr<juce::WebBrowserComponent> webComponent;

    // Coalescing array for APVTS parameter changes heading to web frontend
    std::array<std::atomic<float>, pf02::parameters::kNumParameters> pendingParamValues {};
    std::array<std::atomic<bool>, pf02::parameters::kNumParameters> paramDirty {};

    // Telemetry frame store
    pf02::engine::VisualizerTelemetryFrame latestTelemetryFrame {};
    int silentFrameCounter { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Pf02AudioProcessorEditor)
};
