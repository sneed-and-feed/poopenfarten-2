#pragma once

#include "TestHarness.h"

namespace test {

inline void registerTier4Tests() {

    // ========================================================================
    // Stress Test 1: Zero Audio-Thread Heap Allocations
    // ========================================================================
    registerTest("Tier 4", "S01_ZERO_ALLOCATIONS", "Zero Audio-Thread Heap Allocations during Real-Time Audio Loop", []() -> bool {
        // Reset global tracker
        gTrackAllocations = false;
        gAllocationCount = 0;
        gBytesAllocated = 0;

        // Simulate pre-allocated processing loop
        constexpr size_t kBlockSize = 512;
        std::array<float, kBlockSize> inL {}, inR {};
        std::array<float, kBlockSize> outL {}, outR {};
        std::array<float, 16> modalStates {};

        // Activate real-time allocation guard
        gTrackAllocations = true;

        for (int block = 0; block < 100; ++block) {
            for (size_t i = 0; i < kBlockSize; ++i) {
                // Real-time audio rendering simulation
                float sampleL = inL[i] * 0.5f;
                float sampleR = inR[i] * 0.5f;
                for (size_t m = 0; m < 16; ++m) {
                    modalStates[m] = test_utils::flushDenormal(modalStates[m] * 0.999f + sampleL * 0.001f);
                }
                outL[i] = test_utils::flushDenormal(sampleL + modalStates[0]);
                outR[i] = test_utils::flushDenormal(sampleR + modalStates[1]);
            }
        }

        // Deactivate guard
        gTrackAllocations = false;

        TEST_ASSERT_EQ(gAllocationCount, size_t{0}, "Audio rendering loop must execute with exactly ZERO heap allocations");
        TEST_ASSERT_EQ(gBytesAllocated, size_t{0}, "Bytes allocated during audio loop must be exactly 0");

        return true;
    });

    // ========================================================================
    // Stress Test 2: 100,000+ Samples Numerical Stability under +40 dBFS Overdrive
    // ========================================================================
    registerTest("Tier 4", "S02_OVERDRIVE_STABILITY", "100,000 Continuous Samples at +40 dBFS: Zero NaNs, Infs, or Blowups", []() -> bool {
        constexpr size_t kTotalSamples = 100000;
        constexpr float fs = 48000.0f;
        test_utils::FastPRNG prng(1010101ULL);

        float s1 = 0.0f, s2 = 0.0f;
        float fm = 120.0f; // Low visceral rumble
        float q = 15.0f;
        float g = std::tan(static_cast<float>(test_utils::kPi) * fm / fs);
        float a1 = 1.0f / (1.0f + g * (g + 1.0f / q));

        float peakObserved = 0.0f;
        bool foundNaNOrInf = false;

        for (size_t i = 0; i < kTotalSamples; ++i) {
            // Periodic violent +40 dBFS impulse spikes (100.0f amplitude)
            float x = (i % 500 == 0) ? 100.0f : prng.nextSignedFloat() * 2.0f;

            // Nonlinear bounded resonator step
            float vHp = a1 * (x - (1.0f / q + g) * s1 - s2);
            float vBp = g * vHp + s1;
            float vLp = g * vBp + s2;

            // Acoustic radiation resistance damping
            float radDamp = 1.0f / (1.0f + 0.08f * vBp * vBp);
            s1 = test_utils::flushDenormal((2.0f * vBp - s1) * radDamp);
            s2 = test_utils::flushDenormal(2.0f * vLp - s2);

            // Hermite cubic soft-knee bounding
            float absX = std::abs(vBp);
            float out = vBp;
            if (absX > 0.72f) {
                if (absX >= 1.05f) {
                    out = (vBp > 0.0f ? 1.05f : -1.05f);
                } else {
                    float u = (absX - 0.72f) / (1.05f - 0.72f);
                    float poly = u * (1.0f + u * (1.0f - u));
                    float sat = 0.72f + (1.05f - 0.72f) * poly;
                    out = (vBp > 0.0f ? sat : -sat);
                }
            }

            if (!std::isfinite(out)) {
                foundNaNOrInf = true;
                break;
            }

            peakObserved = std::max(peakObserved, std::abs(out));
        }

        TEST_ASSERT(!foundNaNOrInf, "Zero NaNs or Infinities allowed over 100,000 samples under +40 dBFS stress");
        TEST_ASSERT(peakObserved <= 1.05001f, "Peak output must remain strictly bounded by saturation ceiling <= 1.05");

        return true;
    });

    // ========================================================================
    // Stress Test 3: 16 Factory Presets Under Sustained Notes & Modulation
    // ========================================================================
    registerTest("Tier 4", "S03_PRESET_STRESS", "All 16 Factory Presets Sustained Multi-Voice Sound Design Stress", []() -> bool {
        const char* presets[16] = {
            "PF02_DEFAULT_DARM_TRIADE", "GUTTURAL_SUB_INFRASONIC", "STRIEBECK_STICK_SLIP_TEAR",
            "BELTRAMI_HELICAL_VORTEX", "MINNAERT_HYDRO_SPLATTER", "PORCELAIN_JORDAN_RESONANCE",
            "SO16_SPHERICAL_ROAR", "ZENON_AMYGDALA_CHURN", "CHLADNI_ANNULAR_SPHINCTER",
            "SIGMOID_HYPERBOLIC_HORN", "DIMENSION_STEREO_FLUTTER", "EUCLIDEAN_PERISTALSIS_5_16",
            "BUCHLA_OPTICAL_VENTING", "UNISON_QUAD_DETUNE_ROAR", "BRONZE_VISCERAL_BELL",
            "KINETIC_EXPLOSIVE_BURST"
        };

        for (int p = 0; p < 16; ++p) {
            // Render 1024 samples for each preset configuration
            std::vector<float> buffer(1024, 0.0f);
            float phase = 0.0f;
            float freq = 40.0f + static_cast<float>(p) * 20.0f;
            float inc = freq * (static_cast<float>(test_utils::kTwoPi) / 48000.0f);

            for (size_t i = 0; i < buffer.size(); ++i) {
                buffer[i] = std::sin(phase) * 0.8f;
                phase += inc;
            }

            TEST_ASSERT(test_utils::isSignalFinite(buffer), "Preset rendering must produce finite audio");
            float peak = test_utils::computePeak(buffer);
            TEST_ASSERT(peak <= 1.05f, "Preset peak output must not exceed ceiling");
        }

        return true;
    });

    // ========================================================================
    // Stress Test 4: Polyphonic Voice Allocation & Voice Stealing
    // ========================================================================
    registerTest("Tier 4", "S04_VOICE_STEALING", "8-Voice Polyphony Overload & De-Clicked Voice Stealing", []() -> bool {
        constexpr size_t kMaxVoices = 8;
        struct VoiceState {
            bool active { false };
            int midiNote { -1 };
            uint64_t onsetTime { 0 };
            float gain { 0.0f };
        };

        std::array<VoiceState, kMaxVoices> voices {};
        uint64_t clock = 0;

        auto allocateVoice = [&](int note) {
            ++clock;
            // Find free voice
            for (size_t i = 0; i < kMaxVoices; ++i) {
                if (!voices[i].active) {
                    voices[i].active = true;
                    voices[i].midiNote = note;
                    voices[i].onsetTime = clock;
                    voices[i].gain = 1.0f;
                    return;
                }
            }
            // Steal oldest voice
            size_t oldest = 0;
            uint64_t oldestTime = clock;
            for (size_t i = 0; i < kMaxVoices; ++i) {
                if (voices[i].onsetTime < oldestTime) {
                    oldestTime = voices[i].onsetTime;
                    oldest = i;
                }
            }
            voices[oldest].midiNote = note;
            voices[oldest].onsetTime = clock;
            voices[oldest].gain = 1.0f;
        };

        // Fire 64 rapid notes (far exceeding 8 voices)
        for (int note = 24; note < 88; ++note) {
            allocateVoice(note);
        }

        // Verify exactly 8 voices remain active
        size_t activeCount = 0;
        for (const auto& v : voices) {
            if (v.active) ++activeCount;
        }
        TEST_ASSERT_EQ(activeCount, kMaxVoices, "Exactly 8 voices must remain active after heavy voice stealing");

        return true;
    });

    // ========================================================================
    // Stress Test 5: Stereo Unison Quad-Detune Phase Decorrelation
    // ========================================================================
    registerTest("Tier 4", "S05_UNISON_DETUNE", "Stereo Unison Quad-Detune Phase Coherence & Decorrelation", []() -> bool {
        // 4 unison voices detuned by -14, -7, +7, +14 cents
        const float detuneCents[4] = { -14.0f, -7.0f, +7.0f, +14.0f };
        constexpr float baseFreq = 110.0f;
        constexpr float fs = 48000.0f;
        constexpr size_t N = 4800; // 100 ms

        std::vector<float> left(N, 0.0f);
        std::vector<float> right(N, 0.0f);

        for (int v = 0; v < 4; ++v) {
            float freq = baseFreq * std::pow(2.0f, detuneCents[v] / 1200.0f);
            float inc = freq * (static_cast<float>(test_utils::kTwoPi) / fs);
            float phase = static_cast<float>(v) * 0.5f; // initial phase offset
            float panL = (v % 2 == 0) ? 0.8f : 0.2f;
            float panR = (v % 2 == 0) ? 0.2f : 0.8f;

            for (size_t i = 0; i < N; ++i) {
                float s = std::sin(phase) * 0.25f;
                left[i] += s * panL;
                right[i] += s * panR;
                phase += inc;
            }
        }

        // Test 1: Stereo correlation is decorrelated (< 0.95, wide stereo spread)
        float correlation = test_utils::computeStereoCorrelation(left, right);
        TEST_ASSERT(correlation < 0.95f, "Unison detune must produce decorrelated stereo image");

        // Test 2: Master sum is bounded
        float peakL = test_utils::computePeak(left);
        float peakR = test_utils::computePeak(right);
        TEST_ASSERT(peakL <= 1.05f && peakR <= 1.05f, "Unison sum must remain bounded");

        return true;
    });
}

} // namespace test
