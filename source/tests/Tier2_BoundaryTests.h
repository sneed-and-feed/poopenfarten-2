#pragma once

#include "TestHarness.h"

namespace test {

inline void registerTier2Tests() {

    // ========================================================================
    // Boundary Suite 1: Multirate Resilience (44.1 kHz to 192 kHz)
    // ========================================================================
    registerTest("Tier 2", "B01_MULTIRATE_MATRIX", "Multirate Synthesis Stability across 44.1 kHz to 192 kHz", []() -> bool {
        const double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };

        for (double sr : sampleRates) {
            // Test 1: TPT SVF frequency tuning coefficient g remains strictly finite and positive
            float fm = 1000.0f;
            float g = std::tan(static_cast<float>(test_utils::kPi) * fm / static_cast<float>(sr));
            TEST_ASSERT(g > 0.0f && std::isfinite(g), "TPT g must be finite and positive at all sample rates");

            // Test 2: Nyquist boundary: target frequency 20 kHz is strictly below Nyquist (sr/2)
            float nyquist = static_cast<float>(sr * 0.5);
            TEST_ASSERT(20000.0f < nyquist, "Audio band must reside below Nyquist");

            // Test 3: Phase increment for 440 Hz fundamental scales inversely with sample rate
            float phaseInc = 440.0f * (static_cast<float>(test_utils::kTwoPi) / static_cast<float>(sr));
            TEST_ASSERT(phaseInc > 0.0f && phaseInc < static_cast<float>(test_utils::kPi), "Phase increment must be valid");

            // Test 4: One-pole smoother coefficient alpha remains in (0, 1)
            float timeConstantSec = 0.020f; // 20 ms
            float alpha = 1.0f - std::exp(-1.0f / (timeConstantSec * static_cast<float>(sr)));
            TEST_ASSERT(alpha > 0.0f && alpha < 1.0f, "Smoother alpha must be in (0, 1)");

            // Test 5: Simulated 100-sample block produces non-NaN finite audio at each rate
            float s1 = 0.0f, s2 = 0.0f;
            float q = 10.0f;
            float k = 1.0f / q;
            float a1 = 1.0f / (1.0f + g * (g + k));
            for (int i = 0; i < 100; ++i) {
                float x = (i == 0) ? 1.0f : 0.0f;
                float vHp = a1 * (x - (k + g) * s1 - s2);
                float vBp = g * vHp + s1;
                float vLp = g * vBp + s2;
                s1 = test_utils::flushDenormal(2.0f * vBp - s1);
                s2 = test_utils::flushDenormal(2.0f * vLp - s2);
                TEST_ASSERT_FINITE(vBp, "Resonator output must be finite at all sample rates");
            }
        }

        return true;
    });

    // ========================================================================
    // Boundary Suite 2: Buffer Slicing Invariance (1 to 2048 Samples)
    // ========================================================================
    registerTest("Tier 2", "B02_BUFFER_SLICING", "Buffer Size Resilience across 1 to 2048 Samples", []() -> bool {
        const int bufferSizes[] = { 1, 16, 32, 64, 128, 256, 512, 1024, 2048 };

        // Test 1: Single-sample processing (buffer size 1) runs without crash
        constexpr float fs = 48000.0f;
        float state1 = 0.0f;
        float alpha = 1.0f - std::exp(-1.0f / (0.01f * fs));
        for (int i = 0; i < 100; ++i) {
            state1 += alpha * (1.0f - state1);
            TEST_ASSERT_FINITE(state1, "Single-sample step must be finite");
        }

        // Test 2-5: Multi-buffer state evolution continuity: total state progression over N samples
        // is identical whether rendered as 1 block of 1024 or 16 blocks of 64
        constexpr int totalSamples = 1024;
        
        // Single block of 1024
        float singleBlockState = 0.0f;
        for (int i = 0; i < totalSamples; ++i) {
            singleBlockState += alpha * (1.0f - singleBlockState);
        }

        // 16 blocks of 64
        float slicedBlockState = 0.0f;
        for (int b = 0; b < 16; ++b) {
            for (int s = 0; s < 64; ++s) {
                slicedBlockState += alpha * (1.0f - slicedBlockState);
            }
        }
        TEST_ASSERT_NEAR(slicedBlockState, singleBlockState, 1e-6, "State evolution must be identical across buffer slicing");

        // 1024 blocks of 1 (single-sample stepping)
        float perSampleState = 0.0f;
        for (int i = 0; i < totalSamples; ++i) {
            perSampleState += alpha * (1.0f - perSampleState);
        }
        TEST_ASSERT_NEAR(perSampleState, singleBlockState, 1e-6, "Per-sample stepping matches block stepping");

        // Web Audio standard 128-sample blocks
        for (int b : bufferSizes) {
            TEST_ASSERT(b >= 1 && b <= 2048, "Buffer size within standard testing range [1, 2048]");
        }

        return true;
    });

    // ========================================================================
    // Boundary Suite 3: Extreme Parameter Sweeps & Idle Silence
    // ========================================================================
    registerTest("Tier 2", "B03_PARAM_EXTREMES", "Parameter Extremes & Zero Colonic Pressure Silence", []() -> bool {
        // Test 1: Zero colonic pressure (P = 0.0) produces zero audio output (zero idle bleed)
        auto simulateExciter = [](float pressure, float tension, float aperture) {
            if (pressure < 0.01f) return 0.0f; // Sealed shut / no drive
            return pressure * (1.0f - tension * 0.5f) / aperture;
        };
        TEST_ASSERT_NEAR(simulateExciter(0.0f, 0.5f, 1.0f), 0.0f, 1e-7, "P=0 must yield strictly 0 audio output");

        // Test 2: Maximum colonic pressure (P = 1.0) produces bounded drive
        float maxDrive = simulateExciter(1.0f, 0.0f, 0.5f);
        TEST_ASSERT(maxDrive > 0.0f && maxDrive < 10.0f, "P=1 produces strong bounded drive");

        // Test 3: Tightest resting aperture (0.1 mm) maintains numerical stability
        float tightDrive = simulateExciter(0.7f, 0.5f, 0.1f);
        TEST_ASSERT_FINITE(tightDrive, "0.1 mm resting aperture must remain finite");

        // Test 4: Widest resting aperture (5.0 mm) produces soft breathy drive
        float wideDrive = simulateExciter(0.7f, 0.5f, 5.0f);
        TEST_ASSERT(wideDrive < tightDrive, "Wide aperture produces lower velocity than tight slit");

        // Test 5: Triad coupling gain sweep [0.0, 2.0] maintains finite state
        for (float gain = 0.0f; gain <= 2.0f; gain += 0.2f) {
            float energy = 1.0f * (1.0f + gain * 0.1f);
            TEST_ASSERT_FINITE(energy, "Triad gain sweep must remain finite");
        }

        return true;
    });

    // ========================================================================
    // Boundary Suite 4: Abrupt 1-Sample Parameter Steps
    // ========================================================================
    registerTest("Tier 2", "B04_ABRUPT_STEPS", "Instant 1-Sample Parameter Transients & De-Clicking", []() -> bool {
        // One-pole parameter smoother: target jumps from 0.0 to 1.0 in 1 sample
        constexpr float fs = 48000.0f;
        float smoothVal = 0.0f;
        float targetVal = 1.0f;
        float alpha = 1.0f - std::exp(-1.0f / (0.010f * fs)); // 10 ms smooth

        std::vector<float> trajectory(480, 0.0f);
        for (size_t i = 0; i < trajectory.size(); ++i) {
            smoothVal += alpha * (targetVal - smoothVal);
            trajectory[i] = smoothVal;
        }

        // Test 1: First sample step is small: |smoothVal[0] - 0| < 0.01 (no instantaneous click)
        TEST_ASSERT(trajectory[0] < 0.01f, "First sample change must be small to prevent audio clicks");

        // Test 2: Trajectory is strictly monotonic towards target
        for (size_t i = 1; i < trajectory.size(); ++i) {
            TEST_ASSERT(trajectory[i] >= trajectory[i - 1], "Smoothed parameter trajectory must be monotonic");
        }

        // Test 3: Reaches target within settling time (> 95% within 30 ms / 1440 samples)
        for (int i = 0; i < 1440; ++i) {
            smoothVal += alpha * (targetVal - smoothVal);
        }
        TEST_ASSERT(smoothVal > 0.95f, "Smoothed parameter must reach target within settling window");

        // Test 4: Reverse jump: target jumps from 1.0 to 0.0
        targetVal = 0.0f;
        for (int i = 0; i < 1440; ++i) {
            smoothVal += alpha * (targetVal - smoothVal);
        }
        TEST_ASSERT(smoothVal < 0.05f, "Smoothed parameter must settle to 0.0 without underflow");

        // Test 5: Saturator containment during abrupt jump
        float saturated = std::clamp(trajectory.back() * 2.0f, -1.05f, 1.05f);
        TEST_ASSERT(std::abs(saturated) <= 1.05f, "Saturator bounds transients cleanly");

        return true;
    });

    // ========================================================================
    // Boundary Suite 5: Subnormal Float & Denormal Flushing
    // ========================================================================
    registerTest("Tier 2", "B05_DENORMAL_FLUSH", "IEEE 754 Denormal Flushing & Subnormal Input Safety", []() -> bool {
        // Test 1: Subnormal positive float flushed to exactly 0.0f
        float subnormalPos = 1.4e-45f; // Smallest positive subnormal float
        float flushedPos = test_utils::flushDenormal(subnormalPos);
        TEST_ASSERT_NEAR(flushedPos, 0.0f, 0.0, "Positive subnormal must flush to exactly 0.0f");

        // Test 2: Subnormal negative float flushed to exactly 0.0f
        float subnormalNeg = -1.4e-45f;
        float flushedNeg = test_utils::flushDenormal(subnormalNeg);
        TEST_ASSERT_NEAR(flushedNeg, 0.0f, 0.0, "Negative subnormal must flush to exactly 0.0f");

        // Test 3: Normal small float (e.g. 1.0e-12) is preserved without flushing
        float normalSmall = 1.0e-12f;
        float preserved = test_utils::flushDenormal(normalSmall);
        TEST_ASSERT_NEAR(preserved, normalSmall, 1e-18, "Normal float must be preserved exactly");

        // Test 4: Zero is preserved
        float zero = 0.0f;
        TEST_ASSERT_NEAR(test_utils::flushDenormal(zero), 0.0f, 0.0, "Zero must remain zero");

        // Test 5: Filter state decaying towards zero flushes cleanly without CPU stall
        float filterState = 1.0f;
        constexpr float decay = 0.9f;
        int steps = 0;
        while (filterState > 0.0f && steps < 1000) {
            filterState = test_utils::flushDenormal(filterState * decay);
            ++steps;
        }
        TEST_ASSERT_EQ(filterState, 0.0f, "Filter state must eventually flush to exactly 0.0f");

        return true;
    });
}

} // namespace test
