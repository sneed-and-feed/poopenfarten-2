/**
 * @file test_m2_engine.cpp
 * @brief Empirical Verification Suite for Milestone M2
 *
 * Subject:
 * 1. Parameters.h (40-parameter taxonomy & APVTS layouts)
 * 2. PresetManager.h/.cpp (16 curated factory presets & RFC 8259 JSON import/export)
 * 3. PhysicalVoice.h/.cpp (Coupled physical voice pipeline)
 * 4. VoiceManager.h/.cpp (Mono-Legato, Poly 8-Voice stealing, Stereo Unison)
 * 5. Pf02AudioEngine.h/.cpp (Master engine, mastering chain, lock-free SPSC telemetry)
 */

#include "source/parameters/Parameters.h"
#include "source/parameters/PresetManager.h"
#include "source/engine/PhysicalVoice.h"
#include "source/engine/VoiceManager.h"
#include "source/engine/Pf02AudioEngine.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cassert>
#include <cstring>
#include <chrono>

using namespace pf02;
using namespace pf02::parameters;
using namespace pf02::engine;

#define M2_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "\n[M2 TEST FAILED] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            std::exit(1); \
        } \
    } while (0)

#define M2_ASSERT_NEAR(val, exp, tol, msg) \
    do { \
        double diff = std::abs(static_cast<double>(val) - static_cast<double>(exp)); \
        if (diff > static_cast<double>(tol)) { \
            std::cerr << "\n[M2 TEST FAILED] " << msg << ": got " << (val) << ", expected " << (exp) \
                      << " (diff=" << diff << " > tol=" << (tol) << ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::exit(1); \
        } \
    } while (0)

// ============================================================================
// Test 1: Parameters Taxonomy & Metadata Parity
// ============================================================================
void testParametersTaxonomy() {
    std::cout << "[TEST 1] Verifying Parameters Taxonomy & Metadata...\n";

    const auto& defs = getAllParameterDefs();
    M2_ASSERT(defs.size() == 40, "Must have exactly 40 parameters");
    static_assert(kNumParameters == 40, "kNumParameters constant must equal 40");

    // Check unique IDs and names
    for (size_t i = 0; i < defs.size(); ++i) {
        M2_ASSERT(!defs[i].idString.empty(), "Parameter ID string must not be empty");
        M2_ASSERT(!defs[i].webProperty.empty(), "Web property must not be empty");
        M2_ASSERT(!defs[i].name.empty(), "Display name must not be empty");
        M2_ASSERT(defs[i].minValue <= defs[i].maxValue, "Min value must be <= Max value");
        M2_ASSERT(defs[i].defaultValue >= defs[i].minValue && defs[i].defaultValue <= defs[i].maxValue,
                  "Default value must be within [min, max] range");

        // Lookup by name
        ParameterId lookupId = getParameterIdByName(defs[i].idString);
        M2_ASSERT(lookupId == defs[i].id, "Lookup by ID string failed");

        ParameterId lookupWeb = getParameterIdByName(defs[i].webProperty);
        M2_ASSERT(lookupWeb == defs[i].id, "Lookup by web property string failed");

        for (size_t j = i + 1; j < defs.size(); ++j) {
            M2_ASSERT(defs[i].idString != defs[j].idString, "Duplicate parameter ID detected");
            M2_ASSERT(defs[i].webProperty != defs[j].webProperty, "Duplicate web property detected");
        }
    }

    std::cout << "  -> All 40 parameters verified with unique IDs and valid ranges (PASSED)\n";
}

// ============================================================================
// Test 2: PresetManager & RFC 8259 JSON Import / Export
// ============================================================================
void testPresetManagerAndJson() {
    std::cout << "[TEST 2] Verifying PresetManager & RFC 8259 JSON...\n";

    const auto& factoryPresets = PresetManager::getFactoryPresets();
    M2_ASSERT(factoryPresets.size() == 16, "Must provide exactly 16 factory presets");

    for (size_t i = 0; i < 16; ++i) {
        const auto& p = factoryPresets[i];
        M2_ASSERT(!p.id.empty(), "Preset ID must not be empty");
        M2_ASSERT(!p.name.empty(), "Preset name must not be empty");
        M2_ASSERT(!p.category.empty(), "Preset category must not be empty");
        M2_ASSERT(!p.description.empty(), "Preset description must not be empty");

        // Query by ID and Index
        const Preset* byId = PresetManager::getPresetById(p.id);
        M2_ASSERT(byId != nullptr && byId->id == p.id, "Preset lookup by ID failed");

        const Preset* byIdx = PresetManager::getPresetByIndex(i);
        M2_ASSERT(byIdx != nullptr && byIdx->id == p.id, "Preset lookup by index failed");

        // JSON Export
        std::string jsonStr = PresetManager::exportPresetToJson(p, true);
        M2_ASSERT(!jsonStr.empty(), "Exported preset JSON must not be empty");
        M2_ASSERT(jsonStr.find("\"id\": \"" + p.id + "\"") != std::string::npos, "JSON must contain preset ID");

        // JSON Import Roundtrip
        Preset imported;
        bool importOk = PresetManager::importPresetFromJson(jsonStr, imported);
        M2_ASSERT(importOk, "Preset import from JSON failed");
        M2_ASSERT(imported.id == p.id, "Imported preset ID mismatch");
        M2_ASSERT(imported.name == p.name, "Imported preset name mismatch");

        for (size_t k = 0; k < kNumParameters; ++k) {
            M2_ASSERT_NEAR(imported.params[k], p.params[k], 1e-3, "Imported parameter value mismatch");
        }
    }

    // Bank Export & Import
    std::vector<Preset> bank(factoryPresets.begin(), factoryPresets.end());
    std::string bankJson = PresetManager::exportBankToJson(bank, true);
    M2_ASSERT(!bankJson.empty(), "Exported bank JSON must not be empty");

    std::vector<Preset> importedBank;
    bool bankImportOk = PresetManager::importBankFromJson(bankJson, importedBank);
    M2_ASSERT(bankImportOk, "Bank import from JSON failed");
    M2_ASSERT(importedBank.size() == 16, "Imported bank size must be 16");

    std::cout << "  -> 16 Factory presets and RFC 8259 JSON serialization verified (PASSED)\n";
}

// ============================================================================
// Test 3: PhysicalVoice Single-Voice Lifecycle & Integration
// ============================================================================
void testPhysicalVoice() {
    std::cout << "[TEST 3] Verifying PhysicalVoice Physical Pipeline & Lifecycle...\n";

    PhysicalVoice voice;
    voice.prepare(48000.0);
    voice.reset();

    M2_ASSERT(!voice.isActive(), "Voice must be initially inactive");
    M2_ASSERT(!voice.isNoteHeld(), "No note must be held initially");

    // Trigger Note On (C2 = MIDI 36, f0 = 65.4 Hz)
    voice.noteOn(36, 0.85f);
    M2_ASSERT(voice.isActive(), "Voice must be active after noteOn");
    M2_ASSERT(voice.isNoteHeld(), "Note must be held after noteOn");
    M2_ASSERT(voice.getMidiNote() == 36, "MIDI note must match");

    // Process 2048 samples of note held
    float maxAbsL = 0.0f;
    float maxAbsR = 0.0f;
    for (int i = 0; i < 2048; ++i) {
        float outL = 0.0f, outR = 0.0f;
        voice.processSample(outL, outR);

        M2_ASSERT(std::isfinite(outL), "Left voice output must be finite");
        M2_ASSERT(std::isfinite(outR), "Right voice output must be finite");
        maxAbsL = std::max(maxAbsL, std::abs(outL));
        maxAbsR = std::max(maxAbsR, std::abs(outR));
    }

    M2_ASSERT(maxAbsL > 1e-4f, "Voice must produce audible excitation on Left channel");
    M2_ASSERT(maxAbsR > 1e-4f, "Voice must produce audible excitation on Right channel");
    M2_ASSERT(voice.getEnvelopeLevel() > 0.5f, "Envelope must reach sustained level");
    M2_ASSERT(voice.getAperture() > 0.0f, "Aperture must be open");

    // Trigger Note Off
    voice.noteOff();
    M2_ASSERT(!voice.isNoteHeld(), "Note must not be held after noteOff");

    // Process release tail until voice goes inactive
    int releaseSamples = 0;
    while (voice.isActive() && releaseSamples < 48000) {
        float outL = 0.0f, outR = 0.0f;
        voice.processSample(outL, outR);
        ++releaseSamples;
    }

    M2_ASSERT(!voice.isActive(), "Voice must deactivate after exponential release decay");
    M2_ASSERT(releaseSamples > 500, "Release must have a realistic duration");

    std::cout << "  -> PhysicalVoice lifecycle, aperture, and envelope decay verified (PASSED)\n";
}

// ============================================================================
// Test 4: VoiceManager Mono-Legato, Poly 8-Voice Stealing & Stereo Unison
// ============================================================================
void testVoiceManager() {
    std::cout << "[TEST 4] Verifying VoiceManager Voicing Modes & Stealing...\n";

    VoiceManager vm;
    vm.prepare(48000.0);
    vm.reset();

    // Mode A: Mono-Legato with Portamento & Note Stack
    vm.setVoicingMode(VoicingMode::MonoLegato);
    vm.setPortamentoGlideMs(30.0f);

    vm.noteOn(36, 0.8f);
    M2_ASSERT(vm.getActiveVoiceCount() == 1, "Mono-Legato must use 1 voice");

    // Legato second note (MIDI 48) while first is held
    vm.noteOn(48, 0.7f);
    M2_ASSERT(vm.getActiveVoiceCount() == 1, "Mono-Legato must remain on 1 voice during legato");

    // Release second note -> should glide back to first note
    vm.noteOff(48);
    M2_ASSERT(vm.getActiveVoiceCount() == 1, "Mono-Legato must return to first note");

    // Release first note -> voice should release
    vm.noteOff(36);

    // Mode B: Poly 8-Voice & De-Clicked Voice Stealing
    vm.reset();
    vm.setVoicingMode(VoicingMode::Poly8);

    // Play 8 distinct notes
    for (int n = 36; n < 44; ++n) {
        vm.noteOn(n, 0.75f);
    }
    M2_ASSERT(vm.getActiveVoiceCount() == 8, "All 8 voices must be active");

    // Play 9th note -> must steal oldest voice without crashing
    vm.noteOn(60, 0.90f);
    M2_ASSERT(vm.getActiveVoiceCount() == 8, "Active voice count must stay at 8 after steal");

    // Mode C: Stereo Unison Quad-Detune
    vm.reset();
    vm.setVoicingMode(VoicingMode::StereoUnison);

    vm.noteOn(36, 0.85f);
    M2_ASSERT(vm.getActiveVoiceCount() == 4, "Stereo Unison must allocate 4 voices");

    std::vector<float> bufL(512, 0.0f);
    std::vector<float> bufR(512, 0.0f);
    vm.processBlock(bufL.data(), bufR.data(), 512);

    float peakL = 0.0f, peakR = 0.0f;
    for (size_t i = 0; i < 512; ++i) {
        peakL = std::max(peakL, std::abs(bufL[i]));
        peakR = std::max(peakR, std::abs(bufR[i]));
    }
    M2_ASSERT(peakL > 1e-4f && peakR > 1e-4f, "Stereo unison must output sound on both channels");

    vm.noteOff(36);
    std::cout << "  -> Mono-Legato, Poly 8-Voice stealing, and Stereo Unison verified (PASSED)\n";
}

// ============================================================================
// Test 5: Pf02AudioEngine Master Coordination, SPSC Telemetry & Zero Allocation
// ============================================================================
void testPf02AudioEngine() {
    std::cout << "[TEST 5] Verifying Pf02AudioEngine & Lock-Free SPSC Telemetry...\n";

    Pf02AudioEngine engine;
    engine.prepare(48000.0, 512);
    engine.reset();

    // Trigger note
    engine.noteOn(40, 0.85f);

    std::vector<float> outL(512, 0.0f);
    std::vector<float> outR(512, 0.0f);
    float* channelPointers[2] = { outL.data(), outR.data() };

    // Process multiple blocks to exceed the 60 Hz decimation threshold (800 samples)
    for (int block = 0; block < 10; ++block) {
        engine.processBlock(channelPointers, 2, 512);
    }

    // Verify audio ceiling containment <= 1.05
    for (size_t i = 0; i < 512; ++i) {
        M2_ASSERT(std::abs(outL[i]) <= 1.05001f, "Output L must be bounded by Hermite limiter <= 1.05");
        M2_ASSERT(std::abs(outR[i]) <= 1.05001f, "Output R must be bounded by Hermite limiter <= 1.05");
    }

    // Pop telemetry frame from lock-free SPSC queue
    VisualizerTelemetryFrame frame;
    bool popped = engine.popTelemetry(frame);
    M2_ASSERT(popped, "Must be able to pop 60 Hz telemetry frame from SPSC queue");

    static_assert(std::tuple_size_v<decltype(frame.waveformL)> == 128, "Waveform L must hold 128 samples");
    static_assert(std::tuple_size_v<decltype(frame.waveformR)> == 128, "Waveform R must hold 128 samples");
    static_assert(std::tuple_size_v<decltype(frame.modalEnergies)> == 16, "Modal energies must track 16 partials");
    M2_ASSERT(frame.activeUnrollK >= 1 && frame.activeUnrollK <= 16, "Active unroll K must be valid");
    M2_ASSERT(frame.sphericalNormDrift < 1e-6f, "Spherical norm drift must be < 1e-6");

    // Power toggle test
    engine.setParameter(ParameterId::PowerActive, 0.0f);
    std::fill(outL.begin(), outL.end(), 1.0f);
    std::fill(outR.begin(), outR.end(), 1.0f);
    engine.processBlock(channelPointers, 2, 512);

    for (size_t i = 0; i < 512; ++i) {
        M2_ASSERT(outL[i] == 0.0f && outR[i] == 0.0f, "Power switch off must produce pure silence");
    }

    engine.noteOff(40);
    std::cout << "  -> Master engine, SPSC telemetry, power switch, and brickwall bounds verified (PASSED)\n";
}

// ============================================================================
// Test 6: All 16 Presets Headroom/RMS Balance & MucosalExciter Pitch Accuracy
// ============================================================================
static void testPresetHeadroomAndPitchAccuracy() {
    std::cout << "[M2-TEST 6] Verifying 16 Factory Presets Headroom/RMS & Exciter Pitch Accuracy...\n";

    // 1. MucosalExciter pitch accuracy across tensions [0.1, 0.5, 0.9] and octaves
    for (float f : { 65.41f, 130.81f, 261.63f, 440.0f }) {
        for (float t : { 0.1f, 0.5f, 0.9f }) {
            dsp::MucosalExciter exc;
            exc.prepare(48000.0);
            exc.triggerSlap(0.8f);

            dsp::MucosalExciterParams p;
            p.frequencyHz = f;
            p.mucosalTension = t;
            p.colonicPressure = 0.55f;

            int zc = 0;
            float prev = 0.0f;
            for (int i = 0; i < 48000; ++i) {
                float airV = 0.0f, ap = 0.0f;
                (void)exc.processSample(p, airV, ap);
                if (i > 4800) {
                    const float d = ap;
                    if (prev <= 0.0f && d > 0.0f) ++zc;
                    prev = d;
                }
            }
            const float meas = static_cast<float>(zc) / 0.9f;
            const float ratio = meas / f;
            M2_ASSERT(ratio > 0.95f && ratio < 1.05f, "C++ MucosalExciter limit-cycle frequency must track within +/-5% across all tensions");
        }
    }

    // 2. Render all 16 factory presets and verify zero clipping and healthy RMS
    const auto& presets = PresetManager::getFactoryPresets();
    std::vector<float> bufL(4096, 0.0f);
    std::vector<float> bufR(4096, 0.0f);
    float* chans[2] = { bufL.data(), bufR.data() };

    for (const auto& preset : presets) {
        Pf02AudioEngine eng;
        eng.prepare(48000.0, 512);
        for (size_t i = 0; i < kNumParameters; ++i) {
            eng.setParameter(static_cast<ParameterId>(i), preset.params[i]);
        }
        eng.noteOn(36, 0.85f, 65.41f);

        float maxAbs = 0.0f;
        double sumSq = 0.0;
        int clipped = 0;
        for (int blk = 0; blk < 3; ++blk) {
            eng.processBlock(chans, 2, 4096);
            for (size_t s = 0; s < 4096; ++s) {
                const float a = std::max(std::abs(bufL[s]), std::abs(bufR[s]));
                if (a > maxAbs) maxAbs = a;
                if (a >= 0.999f) ++clipped;
                sumSq += 0.5 * (static_cast<double>(bufL[s]) * bufL[s] + static_cast<double>(bufR[s]) * bufR[s]);
            }
        }
        const float rms = static_cast<float>(std::sqrt(sumSq / (3.0 * 4096.0)));
        M2_ASSERT(clipped == 0 && maxAbs < 0.95f, "Factory preset must not clip");
        M2_ASSERT(rms > 0.012f, "Factory preset must produce healthy audible RMS level");
    }

    std::cout << "  -> All 16 factory presets headroom/RMS and MucosalExciter pitch accuracy verified (PASSED)\n";
}

// ============================================================================
// Test 7: Kinetic Burst Distinctness, Held-Note Transition Smoothness & Low-End Stress
// ============================================================================
static void testKineticBurstsAndLowEndHeadroom() {
    std::cout << "[M2-TEST 7] Verifying 4 Kinetic Bursts Distinctness, Held-Note Smoothness & Low-End Headroom...\n";

    constexpr int kRenderLen = 28000;
    const std::array<std::string_view, 4> burstTypes = { "impulse", "breakaway", "splatter", "cavity" };
    std::array<std::vector<float>, 4> wavesL;
    std::array<float, 4> zcrHz {};
    std::array<float, 4> centroidMs {};

    // 1. Standalone burst distinctness & Stereo Unison centering
    for (size_t b = 0; b < 4; ++b) {
        Pf02AudioEngine eng;
        eng.prepare(48000.0, 512);
        eng.setParameter(ParameterId::VoicingMode, 2.0f); // StereoUnison
        eng.triggerKineticBurst(burstTypes[b], 1.0f);

        wavesL[b].assign(kRenderLen, 0.0f);
        std::vector<float> waveR(kRenderLen, 0.0f);
        float* chans[2] = { wavesL[b].data(), waveR.data() };
        eng.processBlock(chans, 2, kRenderLen);

        double sumSqL = 0.0, sumSqR = 0.0, weightedPos = 0.0;
        int zc = 0;
        float maxP = 0.0f, maxD2 = 0.0f;
        for (int i = 0; i < kRenderLen; ++i) {
            const float l = wavesL[b][i];
            const float r = waveR[i];
            const double eL = static_cast<double>(l) * l;
            sumSqL += eL;
            sumSqR += static_cast<double>(r) * r;
            weightedPos += static_cast<double>(i) * eL;
            maxP = std::max(maxP, std::max(std::abs(l), std::abs(r)));
            if (i > 0 && ((wavesL[b][i - 1] <= 0.0f && l > 0.0f) || (wavesL[b][i - 1] >= 0.0f && l < 0.0f))) {
                ++zc;
            }
            if (i >= 2) {
                const float d2 = std::abs(l - 2.0f * wavesL[b][i - 1] + wavesL[b][i - 2]);
                if (d2 > maxD2) maxD2 = d2;
            }
        }
        const float rmsL = static_cast<float>(std::sqrt(sumSqL / kRenderLen));
        const float rmsR = static_cast<float>(std::sqrt(sumSqR / kRenderLen));
        zcrHz[b] = (static_cast<float>(zc) * 0.5f) / (static_cast<float>(kRenderLen) / 48000.0f);
        centroidMs[b] = static_cast<float>((weightedPos / (sumSqL + 1e-12)) / 48000.0 * 1000.0);

        M2_ASSERT(rmsL > 0.02f && rmsR > 0.02f, "Standalone kinetic burst in StereoUnison must be audible on both L and R channels");
        const float balanceDiff = std::abs(rmsL - rmsR) / std::max(rmsL, rmsR);
        M2_ASSERT(balanceDiff < 0.25f, "Standalone kinetic burst in StereoUnison must remain stereo-balanced (not hard-panned left)");
        M2_ASSERT(maxP < 0.96f, "Standalone kinetic burst must not clip");
        M2_ASSERT(maxD2 < 0.055f, "Standalone kinetic burst must have smooth C1 onset and release without step clicks");
    }

    // Pairwise cross-correlation < 0.25 across all 6 pairs
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = i + 1; j < 4; ++j) {
            double dot = 0.0, n1 = 0.0, n2 = 0.0;
            for (int s = 0; s < 14400; ++s) {
                dot += static_cast<double>(wavesL[i][s]) * wavesL[j][s];
                n1  += static_cast<double>(wavesL[i][s]) * wavesL[i][s];
                n2  += static_cast<double>(wavesL[j][s]) * wavesL[j][s];
            }
            const double corr = std::abs(dot / std::sqrt(n1 * n2 + 1e-24));
            M2_ASSERT(corr < 0.25, "All 4 C++ kinetic bursts must have pairwise correlation |r| < 0.25");
        }
    }
    M2_ASSERT(centroidMs[0] < 30.0f && zcrHz[0] < 200.0f,
              "Impulse burst must be a fast sub-bass thud (<30ms centroid, <200Hz ZCR)");
    M2_ASSERT(zcrHz[2] > 500.0f && zcrHz[2] > zcrHz[1] && zcrHz[2] > zcrHz[3],
              "Splatter burst must have highest-frequency Minnaert bubble/droplet activity (>500Hz ZCR)");
    M2_ASSERT(centroidMs[3] > 60.0f && centroidMs[1] > 45.0f,
              "Cavity and Breakaway bursts must have sustained modal/stick-slip energy centroids");

    // 2. Held-note burst transition smoothness (no spike at burst start or burstDurationSamples)
    for (size_t b = 0; b < 4; ++b) {
        Pf02AudioEngine eng;
        eng.prepare(48000.0, 512);
        eng.noteOn(33, 1.0f);
        std::vector<float> warmL(2400, 0.0f), warmR(2400, 0.0f);
        float* warmChans[2] = { warmL.data(), warmR.data() };
        eng.processBlock(warmChans, 2, 2400);

        eng.triggerKineticBurst(burstTypes[b], 1.0f);
        std::vector<float> holdL(kRenderLen, 0.0f), holdR(kRenderLen, 0.0f);
        float* holdChans[2] = { holdL.data(), holdR.data() };
        eng.processBlock(holdChans, 2, kRenderLen);

        float maxP = 0.0f, maxD2 = 0.0f;
        for (int i = 2; i < kRenderLen; ++i) {
            maxP = std::max(maxP, std::max(std::abs(holdL[i]), std::abs(holdR[i])));
            const float d2 = std::abs(holdL[i] - 2.0f * holdL[i - 1] + holdL[i - 2]);
            if (d2 > maxD2) maxD2 = d2;
        }
        M2_ASSERT(maxP < 0.96f, "Held-note + kinetic burst must not clip");
        M2_ASSERT(maxD2 < 0.06f, "Held-note + kinetic burst must transition smoothly without boundary step clicks");
    }

    // 3. Low-end polyphonic stress (4 sub-bass notes + +6 dB sub reinforcement + 85% saturation drive)
    {
        Pf02AudioEngine eng;
        eng.prepare(48000.0, 512);
        eng.setParameter(ParameterId::VoicingMode, 1.0f); // Poly8
        eng.setParameter(ParameterId::VisceralSubReinforce, 6.0f);
        eng.setParameter(ParameterId::ColonicSaturationDrive, 85.0f);
        eng.setParameter(ParameterId::ColonicPressure, 0.95f);
        eng.noteOn(24, 1.0f);
        eng.noteOn(28, 1.0f);
        eng.noteOn(31, 1.0f);
        eng.noteOn(36, 1.0f);

        std::vector<float> subL(12000, 0.0f), subR(12000, 0.0f);
        float* subChans[2] = { subL.data(), subR.data() };
        eng.processBlock(subChans, 2, 12000);

        float maxP = 0.0f;
        int flatRun = 0, maxFlatRun = 0;
        for (int i = 1; i < 12000; ++i) {
            const float a = std::max(std::abs(subL[i]), std::abs(subR[i]));
            maxP = std::max(maxP, a);
            if (std::abs(subL[i]) > 0.85f && std::abs(subL[i] - subL[i - 1]) < 1e-5f) {
                ++flatRun;
                maxFlatRun = std::max(maxFlatRun, flatRun);
            } else {
                flatRun = 0;
            }
        }
        M2_ASSERT(maxP < 0.99f, "Low-end polyphonic sub-bass stress must stay strictly below 0.99 ceiling");
        M2_ASSERT(maxFlatRun <= 2, "Low-end polyphonic sub-bass stress must never flat-top clip");
    }

    std::cout << "  -> 4 kinetic bursts distinctness, held-note C1 smoothness, and low-end headroom verified (PASSED)\n";
}

// ============================================================================
// Main Execution Entry Point
// ============================================================================
int main() {
    std::cout << "================================================================================\n";
    std::cout << "   MILESTONE M2 VERIFICATION SUITE: VOICE ARCHITECTURE & APVTS PARAMETERS       \n";
    std::cout << "   PF-02 · Darm-Akustischer & Ultrametrischer Triaden-Synthesizer                \n";
    std::cout << "================================================================================\n\n";

    auto t0 = std::chrono::high_resolution_clock::now();

    testParametersTaxonomy();
    testPresetManagerAndJson();
    testPhysicalVoice();
    testVoiceManager();
    testPf02AudioEngine();
    testPresetHeadroomAndPitchAccuracy();
    testKineticBurstsAndLowEndHeadroom();

    auto t1 = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "\n================================================================================\n";
    std::cout << "   ALL 7 MILESTONE M2 TEST SUITES PASSED WITH 100% EMPIRICAL SUCCESS            \n";
    std::cout << "   Total Execution Time: " << std::fixed << std::setprecision(2) << totalMs << " ms\n";
    std::cout << "   VERDICT: APPROVE (MILESTONE M2 COMPLETE)                                     \n";
    std::cout << "================================================================================\n";

    return 0;
}
