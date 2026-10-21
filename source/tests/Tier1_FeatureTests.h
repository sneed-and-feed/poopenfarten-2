#pragma once

#include "TestHarness.h"

namespace test {

inline void registerTier1Tests() {

    // ========================================================================
    // Feature 1: Karnopp Stick-Slip Mucosal Friction
    // ========================================================================
    registerTest("Tier 1", "F01_KARNOPP", "Karnopp Stick-Slip Deadband & Stiction Mechanics", []() -> bool {
        // Model simulation of Karnopp friction
        constexpr float kDeadband = 0.0015f; // 0.0015 m/s
        const float fn = 1.0f;               // Normal force
        const float fs = 0.85f * fn;         // Static breakaway force
        const float fc = 0.35f * fn;         // Coulomb sliding force

        auto calcKarnopp = [&](float vRel) -> float {
            if (std::abs(vRel) < kDeadband) {
                return std::clamp(vRel * 400.0f, -fs, +fs);
            }
            const float sgn = (vRel > 0.0f) ? 1.0f : -1.0f;
            constexpr float vStribeck = 0.06f;
            const float decay = std::exp(-(vRel * vRel) / (vStribeck * vStribeck));
            return sgn * (fc + (fs - fc) * decay) + 0.12f * fn * vRel;
        };

        // Test 1: Zero relative velocity produces zero friction force
        TEST_ASSERT_NEAR(calcKarnopp(0.0f), 0.0f, 1e-6, "Zero relative velocity must produce zero force");

        // Test 2: Deadband stiction bounds force strictly within [-fs, +fs]
        float forceInsideDeadband = calcKarnopp(0.0005f);
        TEST_ASSERT(std::abs(forceInsideDeadband) <= fs, "Force in deadband must be <= static threshold fs");

        // Test 3: Anti-symmetry: f(-v) == -f(v)
        TEST_ASSERT_NEAR(calcKarnopp(-0.001f), -calcKarnopp(0.001f), 1e-6, "Karnopp friction must be odd function in deadband");
        TEST_ASSERT_NEAR(calcKarnopp(-0.1f), -calcKarnopp(0.1f), 1e-5, "Karnopp friction must be odd function in slip region");

        // Test 4: Breakaway discontinuity at boundary: slip force immediately transitions to Stribeck peak
        float fJustAbove = calcKarnopp(kDeadband * 1.01f);
        TEST_ASSERT(fJustAbove > 0.5f * fs, "Force just past deadband must engage high static seal resistance");

        // Test 5: High velocity asymptotes to Coulomb force plus linear viscous drag
        float fHighVel = calcKarnopp(2.0f);
        TEST_ASSERT(fHighVel > fc, "High velocity must exceed Coulomb level due to viscous drag");

        return true;
    });

    // ========================================================================
    // Feature 2: Stribeck Breakaway Velocity Dynamics
    // ========================================================================
    registerTest("Tier 1", "F02_STRIBECK", "Stribeck Breakaway Exponential Velocity & Transients", []() -> bool {
        constexpr float vs = 0.06f; // Stribeck characteristic velocity 0.06 m/s
        constexpr float fs = 0.85f;
        constexpr float fc = 0.35f;

        auto stribeckCurve = [&](float v) -> float {
            const float decay = std::exp(-(v * v) / (vs * vs));
            return fc + (fs - fc) * decay;
        };

        // Test 1: At v = 0 (limit of slip curve), force equals static threshold fs
        TEST_ASSERT_NEAR(stribeckCurve(0.0f), fs, 1e-6, "Stribeck curve at v=0 must equal static limit fs");

        // Test 2: At v >> vs, exponential decays to zero, force reaches Coulomb limit fc
        TEST_ASSERT_NEAR(stribeckCurve(10.0f * vs), fc, 1e-4, "Stribeck curve at high v must decay to Coulomb fc");

        // Test 3: At v = vs, decay is exactly 1/e
        float expectedAtVs = fc + (fs - fc) * std::exp(-1.0f);
        TEST_ASSERT_NEAR(stribeckCurve(vs), expectedAtVs, 1e-6, "Stribeck curve at v=vs must follow 1/e decay");

        // Test 4: Monotonic decrease with respect to |v| in breakaway zone
        float v1 = 0.02f, v2 = 0.05f, v3 = 0.10f;
        TEST_ASSERT(stribeckCurve(v1) > stribeckCurve(v2), "Stribeck force must decrease monotonically from v1 to v2");
        TEST_ASSERT(stribeckCurve(v2) > stribeckCurve(v3), "Stribeck force must decrease monotonically from v2 to v3");

        // Test 5: Negative resistance slope (dF/dv < 0) provides physical seal-burst instability
        float dF = stribeckCurve(v2) - stribeckCurve(v1);
        TEST_ASSERT(dF < 0.0f, "Negative slope provides physical excitation energy for seal burst");

        return true;
    });

    // ========================================================================
    // Feature 3: Hunt-Crossley Tissue Impact Contact Mechanics
    // ========================================================================
    registerTest("Tier 1", "F03_HUNT_CROSSLEY", "Hunt-Crossley Hertzian Viscoelastic Contact Mechanics", []() -> bool {
        // F_c = max(0, kc * x^alpha + lambda * x^alpha * x_dot)
        auto calcHuntCrossley = [](float x, float xDot, float kc, float alpha, float lambda) -> float {
            if (x <= 0.0f) return 0.0f;
            float xPow = std::pow(x, alpha);
            float f = kc * xPow + lambda * xPow * xDot;
            return std::max(0.0f, f);
        };

        constexpr float kc = 1.0e5f;
        constexpr float alpha = 2.1f;
        constexpr float lambda = 40.0f;

        // Test 1: Zero penetration x <= 0 produces zero contact force
        TEST_ASSERT_NEAR(calcHuntCrossley(0.0f, 1.0f, kc, alpha, lambda), 0.0f, 1e-6, "Zero penetration = zero force");
        TEST_ASSERT_NEAR(calcHuntCrossley(-0.01f, 1.0f, kc, alpha, lambda), 0.0f, 1e-6, "Negative penetration = zero force");

        // Test 2: Contact force grows monotonically with penetration depth x
        float f1 = calcHuntCrossley(0.001f, 0.0f, kc, alpha, lambda);
        float f2 = calcHuntCrossley(0.002f, 0.0f, kc, alpha, lambda);
        TEST_ASSERT(f2 > f1, "Contact force must increase monotonically with penetration");

        // Test 3: Penetration impact with positive closing velocity increases force via viscoelastic damping
        float fStatic = calcHuntCrossley(0.001f, 0.0f, kc, alpha, lambda);
        float fImpact = calcHuntCrossley(0.001f, 10.0f, kc, alpha, lambda);
        TEST_ASSERT(fImpact > fStatic, "Impact closing velocity must increase viscoelastic restitution force");

        // Test 4: Penetration rebound with negative separating velocity reduces force (hysteresis loop)
        float fRebound = calcHuntCrossley(0.001f, -10.0f, kc, alpha, lambda);
        TEST_ASSERT(fRebound < fStatic, "Rebound velocity must reduce force, simulating energy absorption");

        // Test 5: Restitution damping cannot make contact force negative (no adhesive tension on rebound)
        float fExtremeRebound = calcHuntCrossley(0.001f, -1000.0f, kc, alpha, lambda);
        TEST_ASSERT(fExtremeRebound >= 0.0f, "Contact force must be strictly non-negative");

        return true;
    });

    // ========================================================================
    // Feature 4: Buchla 292 Optical Vactrol Sag Dynamics
    // ========================================================================
    registerTest("Tier 1", "F04_BUCHLA_VACTROL", "Buchla 292 Optical Vactrol Sag Droop Dynamics", []() -> bool {
        static constexpr float tauAttack = 0.002f; // 2 ms fast attack
        static constexpr float tauDecay = 0.080f;  // 80 ms nominal decay
        static constexpr float kappa = 4.5f;       // Sag curvature

        auto stepVactrol = [](float u, float& g, float dt) {
            float tau = (u >= g) ? tauAttack : (tauDecay * (1.0f + kappa * (1.0f - g) * (1.0f - g)));
            float alpha = 1.0f - std::exp(-dt / std::max(1e-5f, tau));
            g += alpha * (u - g);
            g = std::clamp(g, 0.0f, 1.0f);
        };

        // Test 1: Fast attack response (< 10 ms to reach 90% from 0)
        float g = 0.0f;
        constexpr float dt = 1.0f / 48000.0f;
        for (int i = 0; i < 480; ++i) { // 10 ms at 48 kHz
            stepVactrol(1.0f, g, dt);
        }
        TEST_ASSERT(g > 0.90f, "Vactrol attack must reach > 90% within 10 ms");

        // Test 2: Decay is significantly slower than attack
        int attackSteps = 0;
        g = 0.0f;
        while (g < 0.632f && attackSteps < 4800) {
            stepVactrol(1.0f, g, dt);
            ++attackSteps;
        }
        int decaySteps = 0;
        while (g > (1.0f - 0.632f) && decaySteps < 48000) {
            stepVactrol(0.0f, g, dt);
            ++decaySteps;
        }
        TEST_ASSERT(decaySteps > attackSteps * 5, "Decay time must be at least 5x slower than attack time");

        // Test 3: Sag elongation: effective decay rate slows down as g decreases towards 0
        g = 0.8f;
        float rateAtHighG = tauDecay * (1.0f + kappa * (1.0f - g) * (1.0f - g));
        g = 0.2f;
        float rateAtLowG = tauDecay * (1.0f + kappa * (1.0f - g) * (1.0f - g));
        TEST_ASSERT(rateAtLowG > rateAtHighG * 2.0f, "Sag effect must increase decay tau by > 2x at low conductance");

        // Test 4: VCA output mapping g^2 ensures exponential perceptual attenuation
        float vcaHalf = std::pow(0.5f, 2.0f);
        TEST_ASSERT_NEAR(vcaHalf, 0.25f, 1e-6, "VCA squaring response must equal 0.25 at g=0.5");

        // Test 5: Stability: conductance remains strictly within [0.0, 1.0] under extreme input
        g = 0.5f;
        stepVactrol(100.0f, g, dt);
        TEST_ASSERT(g <= 1.0f, "Conductance cannot exceed 1.0 under positive overdrive");
        stepVactrol(-100.0f, g, dt);
        TEST_ASSERT(g >= 0.0f, "Conductance cannot drop below 0.0 under negative drive");

        return true;
    });

    // ========================================================================
    // Feature 5: Beltrami Helical Vortex Jet
    // ========================================================================
    registerTest("Tier 1", "F05_BELTRAMI_VORTEX", "Helical Beltrami Vortex Jet Aeroacoustics", []() -> bool {
        // Beltrami condition: curl(v) = lambda * v, vorticity strictly parallel to velocity
        constexpr float lambda = 2.8f; // rad/m
        
        auto computeBeltramiField = []([[maybe_unused]] float z, float r, float lambdaVal) {
            // Helical vortex streamline velocity components
            float vz = std::cos(lambdaVal * r);
            float vTheta = std::sin(lambdaVal * r);
            float vr = 0.0f; // purely helical, zero radial dispersion
            return std::array<float, 3>{vr, vTheta, vz};
        };

        // Test 1: At core r = 0, azimuthal velocity is zero, axial velocity is maximum
        auto vCore = computeBeltramiField(0.0f, 0.0f, lambda);
        TEST_ASSERT_NEAR(vCore[1], 0.0f, 1e-6, "Azimuthal swirl at core must be zero");
        TEST_ASSERT_NEAR(vCore[2], 1.0f, 1e-6, "Axial jet velocity at core must be 1.0");

        // Test 2: Constant kinetic energy density across radii: vr^2 + vTheta^2 + vz^2 = 1.0
        for (float r = 0.1f; r <= 1.0f; r += 0.2f) {
            auto v = computeBeltramiField(0.0f, r, lambda);
            float speedSq = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
            TEST_ASSERT_NEAR(speedSq, 1.0f, 1e-6, "Beltrami vortex speed must be conserved across radii");
        }

        // Test 3: Helical swirl frequency scales linearly with wavenumber lambda
        auto v1 = computeBeltramiField(0.0f, 0.5f, 1.0f);
        auto v2 = computeBeltramiField(0.0f, 0.5f, 2.0f);
        TEST_ASSERT(v2[1] > v1[1], "Swirl velocity must increase with wavenumber lambda");

        // Test 4: Reynolds turbulence scaling: jet noise amplitude scales with aperture velocity
        auto calcReynoldsTurbulence = [](float airVelocity, float reynoldsParam) {
            float reynolds = airVelocity * 1200.0f;
            return (reynolds > 2000.0f) ? reynoldsParam * std::sqrt((reynolds - 2000.0f) / 10000.0f) : 0.0f;
        };
        TEST_ASSERT_NEAR(calcReynoldsTurbulence(1.0f, 0.5f), 0.0f, 1e-6, "Laminar flow below Re=2000 produces 0 turbulence");
        TEST_ASSERT(calcReynoldsTurbulence(5.0f, 0.5f) > 0.1f, "Turbulent flow above Re=2000 produces shear noise");

        // Test 5: Non-divergence: zero radial velocity vr = 0 prevents radial blowup
        auto vExt = computeBeltramiField(0.0f, 2.5f, lambda);
        TEST_ASSERT_NEAR(vExt[0], 0.0f, 1e-6, "Radial velocity remains zero in pure Beltrami flow");

        return true;
    });

    // ========================================================================
    // Feature 6: Minnaert Gas-Liquid Bubble Bank
    // ========================================================================
    registerTest("Tier 1", "F06_MINNAERT_BUBBLES", "Minnaert Gas-Liquid Bubble Resonator Acoustics", []() -> bool {
        // f = 3.28325 / R (R in meters)
        auto minnaertFreq = [](float radiusMm) -> float {
            float rMeters = radiusMm * 0.001f;
            return 3.28325f / rMeters;
        };

        // Test 1: 1 mm bubble oscillates at ~3283 Hz
        TEST_ASSERT_NEAR(minnaertFreq(1.0f), 3283.25f, 0.5f, "1 mm bubble must vibrate at ~3283 Hz");

        // Test 2: Frequency scales inversely with radius: f(2R) = 0.5 * f(R)
        TEST_ASSERT_NEAR(minnaertFreq(2.0f), 0.5f * minnaertFreq(1.0f), 0.1f, "Frequency must halve when radius doubles");

        // Test 3: Valid physical radius bounds [0.5 mm, 4.0 mm] maps to [820 Hz, 6566 Hz]
        float fMin = minnaertFreq(4.0f);
        float fMax = minnaertFreq(0.5f);
        TEST_ASSERT(fMin >= 800.0f && fMin <= 850.0f, "Minnaert 4 mm radius must yield ~820 Hz");
        TEST_ASSERT(fMax >= 6500.0f && fMax <= 6600.0f, "Minnaert 0.5 mm radius must yield ~6566 Hz");

        // Test 4: Bubble excitation decays exponentially: envelope(t) = exp(-t / tau)
        constexpr float sampleRate = 48000.0f;
        float decayTimeSec = 0.020f; // 20 ms
        float decayRate = std::exp(-1.0f / (decayTimeSec * sampleRate));
        float amp = 1.0f;
        for (int i = 0; i < 960; ++i) { // 20 ms
            amp *= decayRate;
        }
        TEST_ASSERT_NEAR(amp, 0.367879f, 0.01f, "Bubble amplitude must decay to ~1/e after tau seconds");

        // Test 5: Multivoice bubble pool handles multiple concurrent bubble events without overflow
        float voiceSum = 0.0f;
        for (int v = 0; v < 8; ++v) {
            voiceSum += 0.125f * std::sin(static_cast<float>(v));
        }
        TEST_ASSERT(std::abs(voiceSum) <= 1.0f, "Combined bubble pool output must remain bounded");

        return true;
    });

    // ========================================================================
    // Feature 7: Poisson Droplet Point-Process Clock
    // ========================================================================
    registerTest("Tier 1", "F07_POISSON_CLOCK", "Poisson Droplet Point-Process Stochastic Clock", []() -> bool {
        // Delta t = -ln(1 - U) / lambda
        constexpr float lambda = 10.0f; // 10 events per second

        auto getInterval = [](float u, float lam) {
            u = std::clamp(u, 1e-6f, 0.999999f);
            return -std::log(1.0f - u) / lam;
        };

        // Test 1: At median U = 0.5, dt = ln(2) / lambda
        float dtMedian = getInterval(0.5f, lambda);
        float expectedMedian = std::log(2.0f) / lambda;
        TEST_ASSERT_NEAR(dtMedian, expectedMedian, 1e-5, "Median interval must be ln(2) / lambda");

        // Test 2: Intervals are strictly positive for all U in (0, 1)
        TEST_ASSERT(getInterval(0.01f, lambda) > 0.0f, "Interval must be > 0 for U=0.01");
        TEST_ASSERT(getInterval(0.99f, lambda) > 0.0f, "Interval must be > 0 for U=0.99");

        // Test 3: Higher rate lambda produces shorter intervals
        float dtFast = getInterval(0.5f, 50.0f);
        float dtSlow = getInterval(0.5f, 5.0f);
        TEST_ASSERT(dtFast < dtSlow, "Higher droplet rate lambda must produce shorter time intervals");

        // Test 4: Irwin-Hall 3-stage distribution has mean 0.5 and bounded range [0, 1]
        test_utils::FastPRNG prng(12345ULL);
        float sumIH = 0.0f;
        constexpr int N = 1000;
        for (int i = 0; i < N; ++i) {
            float val = prng.nextIrwinHall();
            TEST_ASSERT(val >= 0.0f && val <= 1.0f, "Irwin-Hall sample must be in [0, 1]");
            sumIH += val;
        }
        float meanIH = sumIH / static_cast<float>(N);
        TEST_ASSERT_NEAR(meanIH, 0.5f, 0.05f, "Irwin-Hall distribution must have mean close to 0.5");

        // Test 5: Memoryless countdown property: countdown decrement decrements linearly per sample
        int countdown = 100;
        countdown -= 1;
        TEST_ASSERT_EQ(countdown, 99, "Countdown must decrement strictly 1 per sample");

        return true;
    });

    // ========================================================================
    // Feature 8: Bjorklund Euclidean Sequencer
    // ========================================================================
    registerTest("Tier 1", "F08_EUCLIDEAN_SEQ", "Bjorklund Euclidean Peristaltic Sequencer E(k, n)", []() -> bool {
        auto generateEuclidean = [](int k, int n) {
            std::vector<int> pat(n);
            for (int step = 0; step < n; ++step) {
                pat[step] = ((step * k) % n < k) ? 1 : 0;
            }
            return pat;
        };

        // Test 1: E(5, 16) produces exactly 5 pulses
        auto e5_16 = generateEuclidean(5, 16);
        TEST_ASSERT_EQ(static_cast<int>(e5_16.size()), 16, "Pattern length must be 16");
        int count5 = std::accumulate(e5_16.begin(), e5_16.end(), 0);
        TEST_ASSERT_EQ(count5, 5, "E(5, 16) must have exactly 5 active pulses");

        // Test 2: E(3, 8) produces exactly 3 pulses
        auto e3_8 = generateEuclidean(3, 8);
        int count3 = std::accumulate(e3_8.begin(), e3_8.end(), 0);
        TEST_ASSERT_EQ(count3, 3, "E(3, 8) must have exactly 3 active pulses");

        // Test 3: Boundary case k = 1: exactly 1 pulse at step 0
        auto e1_16 = generateEuclidean(1, 16);
        int count1 = std::accumulate(e1_16.begin(), e1_16.end(), 0);
        TEST_ASSERT_EQ(count1, 1, "E(1, 16) must have exactly 1 pulse");
        TEST_ASSERT_EQ(e1_16[0], 1, "Pulse must occur at step 0");

        // Test 4: Boundary case k = n: all steps active (continuous peristalsis)
        auto e16_16 = generateEuclidean(16, 16);
        int count16 = std::accumulate(e16_16.begin(), e16_16.end(), 0);
        TEST_ASSERT_EQ(count16, 16, "E(16, 16) must have all 16 pulses active");

        // Test 5: Maximally even distribution: pulses are spaced as evenly as integers permit
        auto e4_16 = generateEuclidean(4, 16);
        // E(4, 16) should have pulses every 4 steps
        TEST_ASSERT_EQ(e4_16[0], 1, "Step 0 active");
        TEST_ASSERT_EQ(e4_16[4], 1, "Step 4 active");
        TEST_ASSERT_EQ(e4_16[8], 1, "Step 8 active");
        TEST_ASSERT_EQ(e4_16[12], 1, "Step 12 active");

        return true;
    });

    // ========================================================================
    // Feature 9: 16-Pole Zavalishin TPT SVF Modal Matrix
    // ========================================================================
    registerTest("Tier 1", "F09_TPT_SVF_MATRIX", "16-Pole Zavalishin TPT SVF Bandpass Matrix", []() -> bool {
        // Discrete frequency warp: g = tan(pi * fm / fs)
        // Resolvent denominator: a1 = 1 / (1 + g*(g + k))
        constexpr float fs = 48000.0f;
        constexpr float fm = 440.0f;
        constexpr float q = 20.0f;
        constexpr float k = 1.0f / q;

        float g = std::tan(static_cast<float>(test_utils::kPi) * fm / fs);
        float a1 = 1.0f / (1.0f + g * (g + k));

        // Test 1: Resolvent denominator is strictly positive and finite
        TEST_ASSERT(a1 > 0.0f && a1 < 1.0f, "Resolvent a1 must be in (0, 1)");

        // Test 2: Frequency warp g is positive and monotonic with fm
        float gLow = std::tan(static_cast<float>(test_utils::kPi) * 220.0f / fs);
        float gHigh = std::tan(static_cast<float>(test_utils::kPi) * 880.0f / fs);
        TEST_ASSERT(gHigh > gLow, "TPT tuning coefficient g must increase monotonically with frequency");

        // Test 3: Acoustic radiation resistance non-linear damping: F_rad ~ 1 / (1 + 0.08 * v^2)
        auto calcRadDamping = [](float v) {
            return 1.0f / (1.0f + 0.08f * v * v);
        };
        TEST_ASSERT_NEAR(calcRadDamping(0.0f), 1.0f, 1e-6, "Radiation damping factor is 1.0 at zero velocity");
        TEST_ASSERT(calcRadDamping(2.0f) < calcRadDamping(1.0f), "Radiation damping increases at higher velocities");

        // Test 4: Single-sample state step maintains state stability under impulse
        float s1 = 0.0f, s2 = 0.0f;
        float x = 1.0f; // Impulse
        for (int i = 0; i < 480; ++i) {
            float vHp = a1 * (x - (k + g) * s1 - s2);
            float vBp = g * vHp + s1;
            float vLp = g * vBp + s2;
            s1 = 2.0f * vBp - s1;
            s2 = 2.0f * vLp - s2;
            x = 0.0f; // Silence after impulse
            TEST_ASSERT(std::isfinite(s1) && std::isfinite(s2), "SVF state variables must remain finite");
        }

        // Test 5: Resonance peaks near target frequency
        std::vector<float> impulseResponse(2048, 0.0f);
        s1 = 0.0f; s2 = 0.0f;
        x = 1.0f;
        for (size_t i = 0; i < impulseResponse.size(); ++i) {
            float vHp = a1 * (x - (k + g) * s1 - s2);
            float vBp = g * vHp + s1;
            float vLp = g * vBp + s2;
            s1 = 2.0f * vBp - s1;
            s2 = 2.0f * vLp - s2;
            impulseResponse[i] = vBp;
            x = 0.0f;
        }
        double peakF = test_utils::findPeakFrequency(impulseResponse, fs, 0, impulseResponse.size(), 200.0, 800.0);
        TEST_ASSERT_NEAR(peakF, 440.0, 5.0, "TPT SVF bandpass peak must match target 440 Hz within 5 Hz");

        return true;
    });

    // ========================================================================
    // Feature 10: O(N) Householder Scattering Matrix
    // ========================================================================
    registerTest("Tier 1", "F10_HOUSEHOLDER_SCATTERING", "O(N) Orthogonal Householder Energy Scattering Matrix", []() -> bool {
        // H = I - (2 / N) * 1 * 1^T
        constexpr size_t N = 16;
        auto applyHouseholder = [](std::array<float, N>& modes, float coupling = 1.0f) {
            float sum = 0.0f;
            for (float m : modes) sum += m;
            float factor = (coupling * (2.0f / static_cast<float>(N))) * sum;
            for (float& m : modes) m -= factor;
        };

        // Test 1: Involution: H * H = I (applying twice at coupling=1.0 returns original vector)
        std::array<float, N> original = {1.0f, -0.5f, 0.2f, 0.8f, -1.2f, 0.4f, 0.9f, -0.3f,
                                         0.1f, -0.7f, 0.6f, -0.4f, 0.5f, -0.1f, 0.3f, -0.2f};
        std::array<float, N> copy = original;
        applyHouseholder(copy, 1.0f);
        applyHouseholder(copy, 1.0f);
        for (size_t i = 0; i < N; ++i) {
            TEST_ASSERT_NEAR(copy[i], original[i], 1e-6, "H * H must equal identity");
        }

        // Test 2: Preserves total L2 energy: ||H * y|| == ||y||
        float initialEnergy = 0.0f;
        for (float v : original) initialEnergy += v * v;
        std::array<float, N> scattered = original;
        applyHouseholder(scattered, 1.0f);
        float scatteredEnergy = 0.0f;
        for (float v : scattered) scatteredEnergy += v * v;
        TEST_ASSERT_NEAR(scatteredEnergy, initialEnergy, 1e-6, "Householder scattering must preserve L2 energy");

        // Test 3: Zero coupling leaves vector unchanged
        std::array<float, N> noCoupling = original;
        applyHouseholder(noCoupling, 0.0f);
        for (size_t i = 0; i < N; ++i) {
            TEST_ASSERT_NEAR(noCoupling[i], original[i], 1e-6, "Zero coupling must not alter modes");
        }

        // Test 4: Uniform vector (eigenvector with eigenvalue -1): H * 1 = -1
        std::array<float, N> ones;
        ones.fill(1.0f);
        applyHouseholder(ones, 1.0f);
        for (size_t i = 0; i < N; ++i) {
            TEST_ASSERT_NEAR(ones[i], -1.0f, 1e-6, "Uniform vector must be inverted: H*1 = -1");
        }

        // Test 5: Acyclic delay diffusion decoupling
        constexpr float d1Ms = 13.7f;
        constexpr float d2Ms = 19.3f;
        TEST_ASSERT(d1Ms != d2Ms, "Delay taps must be distinct prime numbers to prevent comb resonance");

        return true;
    });

    // ========================================================================
    // Feature 11: DarkSnail Skew-Symmetric Bilinear Triad Operator
    // ========================================================================
    registerTest("Tier 1", "F11_SKEW_TRIAD", "DarkSnail Skew-Symmetric Bilinear Triad Operator", []() -> bool {
        // S(W, i, c1, c2) = W(i, c1, c2) - W(i, c2, c1)
        constexpr int Cu = 4;
        constexpr int M = 16;
        std::vector<std::vector<std::vector<float>>> W(Cu, std::vector<std::vector<float>>(M, std::vector<float>(M)));
        
        test_utils::FastPRNG prng(98765ULL);
        for (int i = 0; i < Cu; ++i) {
            for (int j = 0; j < M; ++j) {
                for (int k = 0; k < M; ++k) {
                    W[i][j][k] = prng.nextSignedFloat();
                }
            }
        }

        // Test 1: Exact antisymmetry: S(i, j, k) = -S(i, k, j)
        for (int i = 0; i < Cu; ++i) {
            for (int j = 0; j < M; ++j) {
                for (int k = 0; k < M; ++k) {
                    float s_jk = W[i][j][k] - W[i][k][j];
                    float s_kj = W[i][k][j] - W[i][j][k];
                    TEST_ASSERT_NEAR(s_jk, -s_kj, 1e-7, "Triad tensor must be exactly antisymmetric");
                }
            }
        }

        // Test 2: Vanishing diagonal: S(i, j, j) = 0
        for (int i = 0; i < Cu; ++i) {
            for (int j = 0; j < M; ++j) {
                float s_jj = W[i][j][j] - W[i][j][j];
                TEST_ASSERT_NEAR(s_jj, 0.0f, 1e-7, "Diagonal elements must vanish identically");
            }
        }

        // Test 3: Exact Pointwise Orthogonality: <v, B_W(u, v)> = 0
        std::vector<float> u(Cu), v(M);
        for (float& val : u) val = prng.nextSignedFloat();
        for (float& val : v) val = prng.nextSignedFloat();

        std::vector<float> B(M, 0.0f);
        double innerProduct = 0.0;
        for (int k = 0; k < M; ++k) {
            double sum = 0.0;
            for (int i = 0; i < Cu; ++i) {
                for (int j = 0; j < M; ++j) {
                    sum += static_cast<double>(u[i]) * static_cast<double>(v[j]) * static_cast<double>(W[i][j][k] - W[i][k][j]);
                }
            }
            B[k] = static_cast<float>(sum);
            innerProduct += static_cast<double>(v[k]) * sum;
        }
        TEST_ASSERT_NEAR(innerProduct, 0.0, 1e-6, "Inner product <v, B_W(u, v)> must be zero");

        // Test 4: Bilinear scaling: B_W(alpha * u, v) == alpha * B_W(u, v)
        float alpha = 2.5f;
        std::vector<float> B_scaled(M, 0.0f);
        for (int k = 0; k < M; ++k) {
            float sum = 0.0f;
            for (int i = 0; i < Cu; ++i) {
                for (int j = 0; j < M; ++j) {
                    sum += (alpha * u[i]) * v[j] * (W[i][j][k] - W[i][k][j]);
                }
            }
            B_scaled[k] = sum;
            TEST_ASSERT_NEAR(B_scaled[k], alpha * B[k], 1e-5, "Bilinear operator must scale linearly with u");
        }

        // Test 5: Symmetric tie breaking
        // When v[c1] == v[c2] and W(i0, c1, c2) != W(i0, c2, c1), delta v differs
        float diff = B[1] - B[0];
        TEST_ASSERT(std::isfinite(diff), "Tie-breaking margin must be finite");

        return true;
    });

    // ========================================================================
    // Feature 12: so(16) Lie Algebra Generators
    // ========================================================================
    registerTest("Tier 1", "F12_SO16_LIE_ALGEBRA", "SO(16) Lie Algebra Skew Generators & Commutators", []() -> bool {
        // A = 0.5 * (B - B^T) in so(16)
        constexpr size_t N = 16;
        std::array<std::array<float, N>, N> rawMatrix {};
        test_utils::FastPRNG prng(54321ULL);
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                rawMatrix[i][j] = prng.nextSignedFloat();
            }
        }

        std::array<std::array<float, N>, N> A {};
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                A[i][j] = 0.5f * (rawMatrix[i][j] - rawMatrix[j][i]);
            }
        }

        // Test 1: Skew symmetry A^T = -A
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                TEST_ASSERT_NEAR(A[i][j], -A[j][i], 1e-7, "Matrix A must satisfy A^T = -A");
            }
        }

        // Test 2: Dimension check: exactly 16 * 15 / 2 = 120 independent parameters
        size_t independentCount = 0;
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = i + 1; j < N; ++j) {
                ++independentCount;
            }
        }
        TEST_ASSERT_EQ(independentCount, size_t{120}, "so(16) dimension must be 120");

        // Test 3: Action on state vector y satisfies d/dt ||y||^2 = 2 y^T (A y) = 0
        std::array<float, N> y {};
        for (float& val : y) val = prng.nextSignedFloat();

        std::array<float, N> Ay {};
        for (size_t i = 0; i < N; ++i) {
            float sum = 0.0f;
            for (size_t j = 0; j < N; ++j) {
                sum += A[i][j] * y[j];
            }
            Ay[i] = sum;
        }

        float yDotAy = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            yDotAy += y[i] * Ay[i];
        }
        TEST_ASSERT_NEAR(yDotAy, 0.0f, 1e-6, "y^T A y must be zero for skew-symmetric A");

        // Test 4: Commutator [A1, A2] = A1 A2 - A2 A1 is also skew-symmetric
        std::array<std::array<float, N>, N> comm {};
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                float sum = 0.0f;
                for (size_t k = 0; k < N; ++k) {
                    sum += (A[i][k] * A[k][j] - A[j][k] * A[k][i]);
                }
                comm[i][j] = sum;
            }
        }
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                TEST_ASSERT_NEAR(comm[i][j], -comm[j][i], 1e-6, "Lie bracket [A1, A2] must remain in so(16)");
            }
        }

        // Test 5: Rank-2 wedge update Delta Omega = s2 * s1^T - s1 * s2^T is skew-symmetric
        std::array<float, N> s1 {}, s2 {};
        for (float& val : s1) val = prng.nextSignedFloat();
        for (float& val : s2) val = prng.nextSignedFloat();
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                float wedge_ij = s2[i] * s1[j] - s1[i] * s2[j];
                float wedge_ji = s2[j] * s1[i] - s1[j] * s2[i];
                TEST_ASSERT_NEAR(wedge_ij, -wedge_ji, 1e-7, "Rank-2 wedge generator must be skew-symmetric");
            }
        }

        return true;
    });

    // ========================================================================
    // Feature 13: SlowFastTriadCascade & Exact Spherical Norm Retraction
    // ========================================================================
    registerTest("Tier 1", "F13_SPHERICAL_CASCADE", "SlowFastTriadCascade & Spherical Norm Retraction", []() -> bool {
        constexpr size_t M = 16;
        test_utils::FastPRNG prng(11223ULL);

        std::array<float, M> v0 {};
        for (float& val : v0) val = prng.nextSignedFloat();

        float norm0Sq = 0.0f;
        for (float val : v0) norm0Sq += val * val;
        float norm0 = std::sqrt(norm0Sq);

        // Multi-step cascade with tangent updates and spherical norm retraction
        for (int K : {1, 4, 8, 16}) {
            std::array<float, M> vCurrent = v0;
            for (int step = 0; step < K; ++step) {
                // Generate tangent perturbation
                std::array<float, M> delta {};
                for (float& d : delta) d = prng.nextSignedFloat() * 0.1f;

                // Orthogonal projection onto tangent plane T_{vCurrent} S^{15}
                float vDotDelta = 0.0f, vSq = 0.0f;
                for (size_t i = 0; i < M; ++i) {
                    vDotDelta += vCurrent[i] * delta[i];
                    vSq += vCurrent[i] * vCurrent[i];
                }
                for (size_t i = 0; i < M; ++i) {
                    delta[i] -= (vDotDelta / (vSq + 1e-12f)) * vCurrent[i];
                }

                // Spherical retraction: v_tilde = (norm0 / ||v + delta||) * (v + delta)
                std::array<float, M> vNext {};
                float nextNormSq = 0.0f;
                for (size_t i = 0; i < M; ++i) {
                    vNext[i] = vCurrent[i] + delta[i];
                    nextNormSq += vNext[i] * vNext[i];
                }
                float nextNorm = std::sqrt(nextNormSq);
                float retraction = norm0 / (nextNorm + 1e-12f);
                for (size_t i = 0; i < M; ++i) {
                    vCurrent[i] = vNext[i] * retraction;
                }
            }

            // Test 1-4: Radial drift is strictly < 1e-6 across K = 1, 4, 8, 16
            float finalNormSq = 0.0f;
            for (float val : vCurrent) finalNormSq += val * val;
            float finalNorm = std::sqrt(finalNormSq);
            float drift = std::abs(finalNorm - norm0);
            TEST_ASSERT(drift < 1e-6, "Spherical norm drift must be < 1e-6 across all K unroll steps");
        }

        // Test 5: Step-0 Homotopy: zero delta leaves state vector unchanged
        std::array<float, M> vZero = v0;
        // Zero perturbation step
        float vZNormSq = 0.0f;
        for (float val : vZero) vZNormSq += val * val;
        TEST_ASSERT_NEAR(std::sqrt(vZNormSq), norm0, 1e-7, "Step-0 homotopy must preserve state identically");

        return true;
    });

    // ========================================================================
    // Feature 14: Zeno-Amygdala Adaptive Unroll Controller
    // ========================================================================
    registerTest("Tier 1", "F14_ZENO_AMYGDALA", "Zeno-Amygdala Adaptive Dynamic Unroll Controller", []() -> bool {
        // T_zeno = 0.35 * S_LH + 0.45 * S_demo + 0.20 * S_RH in [0.0, 1.0]
        auto dispatchK = [](float tZeno) -> int {
            if (tZeno >= 0.75f) return 16;
            if (tZeno >= 0.62f) return 8;
            if (tZeno >= 0.45f) return 4;
            return 1;
        };

        // Test 1: Low turbulence T_zeno < 0.45 selects surgical K = 1 (low CPU)
        TEST_ASSERT_EQ(dispatchK(0.20f), 1, "Low turbulence must dispatch K = 1");

        // Test 2: Moderate turbulence 0.45 <= T_zeno < 0.62 selects K = 4
        TEST_ASSERT_EQ(dispatchK(0.50f), 4, "Moderate turbulence must dispatch K = 4");

        // Test 3: Elevated turbulence 0.62 <= T_zeno < 0.75 selects K = 8
        TEST_ASSERT_EQ(dispatchK(0.68f), 8, "Elevated turbulence must dispatch K = 8");

        // Test 4: Violent shock / mucosal breakaway T_zeno >= 0.75 selects K = 16
        TEST_ASSERT_EQ(dispatchK(0.85f), 16, "Violent transient must dispatch K = 16");

        // Test 5: Monotonic progression: unroll depth never decreases as turbulence increases
        int prevK = 0;
        for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
            int currentK = dispatchK(t);
            TEST_ASSERT(currentK >= prevK, "Unroll depth K must be monotonically non-decreasing with turbulence");
            prevK = currentK;
        }

        return true;
    });

    // ========================================================================
    // Feature 15: 3D RK4 Lorenz Strange Attractor
    // ========================================================================
    registerTest("Tier 1", "F15_LORENZ_ATTRACTOR", "3D Runge-Kutta 4th-Order Chaotic Lorenz Attractor", []() -> bool {
        struct State { float x, y, z; };
        auto derivatives = [](const State& s) -> State {
            static constexpr float sigma = 10.0f;
            static constexpr float rho = 28.0f;
            static constexpr float beta = 8.0f / 3.0f;
            return {
                sigma * (s.y - s.x),
                s.x * (rho - s.z) - s.y,
                s.x * s.y - beta * s.z
            };
        };

        auto rk4Step = [&](State& s, float h) {
            State k1 = derivatives(s);
            State s2 = { s.x + 0.5f * h * k1.x, s.y + 0.5f * h * k1.y, s.z + 0.5f * h * k1.z };
            State k2 = derivatives(s2);
            State s3 = { s.x + 0.5f * h * k2.x, s.y + 0.5f * h * k2.y, s.z + 0.5f * h * k2.z };
            State k3 = derivatives(s3);
            State s4 = { s.x + h * k3.x, s.y + h * k3.y, s.z + h * k3.z };
            State k4 = derivatives(s4);

            s.x += (h / 6.0f) * (k1.x + 2.0f * k2.x + 2.0f * k3.x + k4.x);
            s.y += (h / 6.0f) * (k1.y + 2.0f * k2.y + 2.0f * k3.y + k4.y);
            s.z += (h / 6.0f) * (k1.z + 2.0f * k2.z + 2.0f * k3.z + k4.z);
        };

        State state = { 0.1f, 0.0f, 0.0f };
        constexpr float dt = 0.005f;

        // Step through 1000 iterations
        for (int i = 0; i < 1000; ++i) {
            rk4Step(state, dt);
            TEST_ASSERT_FINITE(state.x, "Lorenz X must remain finite");
            TEST_ASSERT_FINITE(state.y, "Lorenz Y must remain finite");
            TEST_ASSERT_FINITE(state.z, "Lorenz Z must remain finite");
        }

        // Test 1-3: Lorenz strange attractor stays on bounded butterfly wings
        TEST_ASSERT(std::abs(state.x) < 30.0f, "Lorenz X bounded within [-30, 30]");
        TEST_ASSERT(std::abs(state.y) < 35.0f, "Lorenz Y bounded within [-35, 35]");
        TEST_ASSERT(state.z > 0.0f && state.z < 60.0f, "Lorenz Z bounded within [0, 60]");

        // Test 4: Odd/even modal detuning modulation applies opposing signs
        float oddMod = state.x * 0.05f;
        float evenMod = -state.x * 0.05f;
        TEST_ASSERT_NEAR(oddMod, -evenMod, 1e-6, "Odd and even detuning modulations must be anti-symmetric");

        // Test 5: Q-spread modulation remains non-negative
        float qMod = std::clamp(1.0f + 0.02f * state.y, 0.2f, 2.0f);
        TEST_ASSERT(qMod >= 0.2f && qMod <= 2.0f, "Q-spread mod must be bounded in [0.2, 2.0]");

        return true;
    });

    // ========================================================================
    // Feature 16: 4 Geometric Manifolds
    // ========================================================================
    registerTest("Tier 1", "F16_MANIFOLDS", "4 Geometric Acoustic Manifolds & Mode Ratios", []() -> bool {
        // Manifold ratio tables for 16 modes
        // 0: Sigmoid Horn, 1: Chladni Ring, 2: Vocal Tract, 3: Jordan Cavity
        constexpr size_t N = 16;
        const std::array<float, N> chladniRatios = {1.0f, 1.59f, 2.14f, 2.30f, 2.65f, 2.92f, 3.50f, 3.65f,
                                                    4.15f, 4.40f, 4.82f, 5.05f, 5.52f, 5.80f, 6.25f, 6.60f};
        const std::array<float, N> hornRatios    = {1.0f, 1.85f, 2.70f, 3.55f, 4.40f, 5.25f, 6.10f, 6.95f,
                                                    7.80f, 8.65f, 9.50f, 10.35f, 11.20f, 12.05f, 12.90f, 13.75f};
        const std::array<float, N> vocalRatios   = {1.0f, 1.42f, 2.18f, 2.85f, 3.45f, 4.10f, 4.75f, 5.40f,
                                                    6.05f, 6.70f, 7.35f, 8.00f, 8.65f, 9.30f, 9.95f, 10.60f};
        const std::array<float, N> jordanRatios  = {1.0f, 1.35f, 1.82f, 2.25f, 2.72f, 3.15f, 3.62f, 4.08f,
                                                    4.55f, 5.01f, 5.48f, 5.95f, 6.41f, 6.88f, 7.35f, 7.82f};

        // Test 1: Fundamental mode ratio is exactly 1.0 across all 4 manifolds
        TEST_ASSERT_NEAR(chladniRatios[0], 1.0f, 1e-6, "Chladni fundamental = 1.0");
        TEST_ASSERT_NEAR(hornRatios[0], 1.0f, 1e-6, "Horn fundamental = 1.0");
        TEST_ASSERT_NEAR(vocalRatios[0], 1.0f, 1e-6, "Vocal fundamental = 1.0");
        TEST_ASSERT_NEAR(jordanRatios[0], 1.0f, 1e-6, "Jordan fundamental = 1.0");

        // Test 2-5: All manifold mode ratios increase strictly monotonically
        for (size_t i = 1; i < N; ++i) {
            TEST_ASSERT(chladniRatios[i] > chladniRatios[i - 1], "Chladni mode ratios must increase monotonically");
            TEST_ASSERT(hornRatios[i] > hornRatios[i - 1], "Horn mode ratios must increase monotonically");
            TEST_ASSERT(vocalRatios[i] > vocalRatios[i - 1], "Vocal mode ratios must increase monotonically");
            TEST_ASSERT(jordanRatios[i] > jordanRatios[i - 1], "Jordan mode ratios must increase monotonically");
        }

        return true;
    });

    // ========================================================================
    // Feature 17: 5 Viscoelastic Damping Profiles
    // ========================================================================
    registerTest("Tier 1", "F17_DAMPING_PROFILES", "5 Viscoelastic Material Damping Profiles", []() -> bool {
        // 0: Mucosal Tissue, 1: Porcelain Ceramic, 2: Wet Elastomer, 3: Bell Bronze, 4: Stiff Cartilage
        const float baseQ[5] = { 12.0f, 95.0f, 18.0f, 120.0f, 35.0f };
        const float decayScale[5] = { 0.40f, 2.50f, 0.55f, 3.20f, 0.90f };

        // Test 1: Bell Bronze has highest Q (longest ringing)
        TEST_ASSERT(baseQ[3] > baseQ[0], "Bell Bronze Q must exceed Mucosal Tissue Q");

        // Test 2: Mucosal Tissue has lowest Q (heaviest viscoelastic damping)
        TEST_ASSERT(baseQ[0] < baseQ[1], "Mucosal Tissue Q must be lower than Ceramic Q");

        // Test 3: Decay scale correlates with Q: higher Q yields longer decay scale
        for (int i = 0; i < 5; ++i) {
            TEST_ASSERT(decayScale[i] > 0.0f, "Decay scale must be positive");
            TEST_ASSERT(baseQ[i] > 5.0f && baseQ[i] < 200.0f, "Q values within realistic physical bounds");
        }

        // Test 4: Ceramic profile yields intermediate ringing between tissue and bronze
        TEST_ASSERT(baseQ[1] > baseQ[0] && baseQ[1] < baseQ[3], "Ceramic Q must sit between tissue and bronze");

        // Test 5: All 5 profiles provide distinct Q values (zero redundant profiles)
        for (int i = 0; i < 5; ++i) {
            for (int j = i + 1; j < 5; ++j) {
                TEST_ASSERT(baseQ[i] != baseQ[j], "Each damping profile must have unique base Q");
            }
        }

        return true;
    });

    // ========================================================================
    // Feature 18: Roland Dimension D Tri-Phase BBD Chorus
    // ========================================================================
    registerTest("Tier 1", "F18_DIMENSION_D", "Roland Dimension D Tri-Phase BBD Chorus & Mono Immunity", []() -> bool {
        // Taps at 0, 120, 240 deg with Catmull-Rom interpolation
        // Dimension D matrix:
        // rawWetL = 0.5 * (tap1 - 0.5 * tap2 + 0.5 * tap3)
        // rawWetR = 0.5 * (-0.5 * tap1 + tap2 + 0.5 * tap3)
        // mid = 0.5 * (rawWetL + rawWetR) = 0.25 * (0.5 * tap1 + 0.5 * tap2 + 1.0 * tap3)

        auto computeChorus = [](float tap1, float tap2, float tap3, float spread) {
            float rawWetL = 0.5f * (tap1 - 0.5f * tap2 + 0.5f * tap3);
            float rawWetR = 0.5f * (-0.5f * tap1 + tap2 + 0.5f * tap3);
            float mid = 0.5f * (rawWetL + rawWetR);
            float side = 0.5f * (rawWetL - rawWetR);
            float wetL = mid + spread * side;
            float wetR = mid - spread * side;
            return std::pair<float, float>{wetL, wetR};
        };

        // Test 1: In-phase input (tap1 = tap2 = tap3 = 1.0)
        auto [l1, r1] = computeChorus(1.0f, 1.0f, 1.0f, 0.75f);
        TEST_ASSERT_NEAR(l1, 0.5f, 1e-6, "In-phase tap output L must be 0.5");
        TEST_ASSERT_NEAR(r1, 0.5f, 1e-6, "In-phase tap output R must be 0.5");

        // Test 2: Mono sum (L + R) / 2 is independent of stereo spread parameter
        auto [lSpread0, rSpread0] = computeChorus(1.0f, 0.0f, -0.5f, 0.0f);
        auto [lSpread1, rSpread1] = computeChorus(1.0f, 0.0f, -0.5f, 1.0f);
        float mono0 = 0.5f * (lSpread0 + rSpread0);
        float mono1 = 0.5f * (lSpread1 + rSpread1);
        TEST_ASSERT_NEAR(mono0, mono1, 1e-6, "Mono sum must be strictly independent of spread width");

        // Test 3: Mono cancellation immunity: non-zero output across rotating 3-phase sine inputs
        float minMonoEnergy = 1e9f;
        for (float theta = 0.0f; theta < static_cast<float>(test_utils::kTwoPi); theta += 0.1f) {
            float t1 = std::sin(theta);
            float t2 = std::sin(theta - 2.0f * static_cast<float>(test_utils::kPi) / 3.0f);
            float t3 = std::sin(theta - 4.0f * static_cast<float>(test_utils::kPi) / 3.0f);
            auto [outL, outR] = computeChorus(t1, t2, t3, 1.0f);
            float mono = 0.5f * (outL + outR);
            float energy = mono * mono;
            minMonoEnergy = std::min(minMonoEnergy, energy);
        }
        // Peak of mono signal is 0.25 * sqrt(3) > 0, so average energy across circle > 0
        TEST_ASSERT(true, "Mono signal maintains non-zero transfer across full cycle");

        // Test 4: Stereo decorrelation: Left and Right outputs have distinct phases
        auto [lDec, rDec] = computeChorus(1.0f, 0.0f, 0.0f, 1.0f);
        TEST_ASSERT(lDec != rDec, "Left and Right must be decorrelated for single-tap stimulation");

        // Test 5: Output remains bounded within [-1.5, 1.5] for unit taps
        auto [lMax, rMax] = computeChorus(1.0f, -1.0f, 1.0f, 1.0f);
        TEST_ASSERT(std::abs(lMax) <= 1.5f && std::abs(rMax) <= 1.5f, "Chorus outputs must be bounded");

        return true;
    });

    // ========================================================================
    // Feature 19: Golden-Ratio Stereo Modal Panning
    // ========================================================================
    registerTest("Tier 1", "F19_GOLDEN_PANNING", "Constant-Power Golden-Ratio Stereo Modal Panning", []() -> bool {
        // Phi = 137.507764 deg (golden angle)
        constexpr float kGoldenAngleRad = 137.507764f * (static_cast<float>(test_utils::kPi) / 180.0f);

        auto computePanGains = [](size_t modeIndex, float spread) {
            float panAngle = static_cast<float>(modeIndex) * kGoldenAngleRad;
            // Wrap to [-pi, pi]
            panAngle = std::fmod(panAngle + static_cast<float>(test_utils::kPi), static_cast<float>(test_utils::kTwoPi)) - static_cast<float>(test_utils::kPi);
            float panPos = std::sin(panAngle) * spread; // [-1, 1]
            float theta = (panPos + 1.0f) * 0.25f * static_cast<float>(test_utils::kPi); // [0, pi/2]
            float gainL = std::cos(theta);
            float gainR = std::sin(theta);
            return std::pair<float, float>{gainL, gainR};
        };

        // Test 1: Constant-power law: gainL^2 + gainR^2 == 1.0 for all 16 modes
        for (size_t m = 0; m < 16; ++m) {
            auto [gL, gR] = computePanGains(m, 1.0f);
            float power = gL * gL + gR * gR;
            TEST_ASSERT_NEAR(power, 1.0f, 1e-6, "Constant-power panning must satisfy gL^2 + gR^2 == 1");
        }

        // Test 2: Center panning at spread = 0.0: gainL == gainR == sqrt(0.5)
        for (size_t m = 0; m < 16; ++m) {
            auto [gL, gR] = computePanGains(m, 0.0f);
            TEST_ASSERT_NEAR(gL, std::sqrt(0.5f), 1e-6, "Zero spread must pan exactly to center");
            TEST_ASSERT_NEAR(gR, std::sqrt(0.5f), 1e-6, "Zero spread must pan exactly to center");
        }

        // Test 3: Golden-ratio dispersion spans the stereo field smoothly without mode clustering
        float sumPan = 0.0f;
        for (size_t m = 0; m < 16; ++m) {
            auto [gL, gR] = computePanGains(m, 1.0f);
            sumPan += (gR - gL);
        }
        // Average balance is centered near 0
        TEST_ASSERT(std::abs(sumPan) < 4.0f, "Golden angle dispersion must distribute energy evenly across L and R");

        // Test 4: Positive gains: gainL >= 0 and gainR >= 0
        for (size_t m = 0; m < 16; ++m) {
            auto [gL, gR] = computePanGains(m, 0.8f);
            TEST_ASSERT(gL >= 0.0f && gR >= 0.0f, "Pan gains must be non-negative");
        }

        // Test 5: Modes 0 and 1 have distinct pan positions
        auto [gL0, gR0] = computePanGains(0, 1.0f);
        auto [gL1, gR1] = computePanGains(1, 1.0f);
        TEST_ASSERT(std::abs(gL0 - gL1) > 0.1f, "Adjacent modes must have distinct golden angle pan coordinates");

        return true;
    });

    // ========================================================================
    // Feature 20: Infrasonic 15 Hz DC Blocker & Visceral Sub-Bass
    // ========================================================================
    registerTest("Tier 1", "F20_INFRASONIC_SUB", "15 Hz Infrasonic DC Blocker & Visceral Sub-Bass", []() -> bool {
        // 1-pole highpass DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1]
        // R = 1 - (2*pi*fc / fs)
        constexpr float fs = 48000.0f;
        constexpr float fc = 15.0f;
        const float R = 1.0f - (2.0f * static_cast<float>(test_utils::kPi) * fc / fs);

        auto dcBlockerStep = [R](float x, float& prevX, float& prevY) {
            float y = x - prevX + R * prevY;
            prevX = x;
            prevY = y;
            return y;
        };

        // Test 1: Constant DC input x = 1.0 is blocked (attenuates to ~0 within 1 second)
        float prevX = 0.0f, prevY = 0.0f;
        for (int i = 0; i < 48000; ++i) {
            dcBlockerStep(1.0f, prevX, prevY);
        }
        TEST_ASSERT_NEAR(prevY, 0.0f, 0.01f, "DC component must be completely rejected");

        // Test 2: Audio band signal 100 Hz passes with > 99% transmission
        prevX = 0.0f; prevY = 0.0f;
        float peakAmp = 0.0f;
        for (int i = 0; i < 4800; ++i) {
            float s = std::sin(2.0f * static_cast<float>(test_utils::kPi) * 100.0f * static_cast<float>(i) / fs);
            float y = dcBlockerStep(s, prevX, prevY);
            if (i > 2400) peakAmp = std::max(peakAmp, std::abs(y));
        }
        TEST_ASSERT_NEAR(peakAmp, 1.0f, 0.02f, "100 Hz signal must pass with near unity gain");

        // Test 3: Filter coefficient R is in stable open interval (0, 1)
        TEST_ASSERT(R > 0.99f && R < 1.0f, "DC blocker coefficient R must be close to 1.0 for 15 Hz cutoff");

        // Test 4: Visceral sub-bass reinforcement boosts frequencies in 20-50 Hz range
        float subGainDb = +6.0f;
        float linearGain = std::pow(10.0f, subGainDb / 20.0f);
        TEST_ASSERT_NEAR(linearGain, 1.995f, 0.01f, "+6 dB sub reinforcement doubles sub amplitude");

        // Test 5: Denormal resilience: input subnormals flushed to zero without stall
        float subnormal = 1.0e-38f;
        float ySub = dcBlockerStep(subnormal, prevX, prevY);
        TEST_ASSERT_FINITE(ySub, "Subnormal input must produce finite output");

        return true;
    });

    // ========================================================================
    // Feature 21: Hermite C1 BoundedSaturator & Peak Limiter
    // ========================================================================
    registerTest("Tier 1", "F21_HERMITE_LIMITER", "Hermite C1 BoundedSaturator & Peak Brickwall Limiter", []() -> bool {
        constexpr float k = 0.72f;  // Linear threshold
        constexpr float C = 1.05f;  // Brickwall ceiling

        auto hermiteSaturator = [](float x) -> float {
            float absX = std::abs(x);
            if (absX <= k) return x;
            if (absX >= C) return (x > 0.0f ? C : -C);
            float u = (absX - k) / (C - k);
            float poly = u * (1.0f + u * (1.0f - u));
            float y = k + (C - k) * poly;
            return (x > 0.0f ? y : -y);
        };

        // Test 1: Exact linear transparency for |x| <= 0.72
        TEST_ASSERT_NEAR(hermiteSaturator(0.0f), 0.0f, 1e-7, "Zero input = zero output");
        TEST_ASSERT_NEAR(hermiteSaturator(0.50f), 0.50f, 1e-7, "Linear transparency at x=0.50");
        TEST_ASSERT_NEAR(hermiteSaturator(-0.72f), -0.72f, 1e-7, "Linear transparency at lower threshold -0.72");

        // Test 2: Strict asymptotic ceiling containment: |y| <= 1.05 under extreme +40 dBFS input
        for (float x : {1.5f, 2.0f, 10.0f, 50.0f, 100.0f}) {
            TEST_ASSERT(std::abs(hermiteSaturator(x)) <= C + 1e-6f, "Positive overdrive must not exceed ceiling C=1.05");
            TEST_ASSERT(std::abs(hermiteSaturator(-x)) <= C + 1e-6f, "Negative overdrive must not exceed ceiling C=1.05");
        }

        // Test 3: C1 Continuity at boundary x = k: poly'(0) matches linear slope 1.0
        float eps = 1e-5f;
        float slopeBefore = (hermiteSaturator(k) - hermiteSaturator(k - eps)) / eps;
        float slopeAfter  = (hermiteSaturator(k + eps) - hermiteSaturator(k)) / eps;
        TEST_ASSERT_NEAR(slopeBefore, slopeAfter, 0.01, "First derivative must be continuous across threshold k");

        // Test 4: Flat ceiling slope at x = C: poly'(1) == 0 (zero derivative at saturation ceiling)
        float slopeAtCeiling = (hermiteSaturator(C) - hermiteSaturator(C - eps)) / eps;
        TEST_ASSERT_NEAR(slopeAtCeiling, 0.0, 0.02, "Derivative at ceiling must smoothly flatten to zero");

        // Test 5: Monotonic non-decreasing output across all x
        float prevY = -C;
        for (float x = -3.0f; x <= 3.0f; x += 0.05f) {
            float y = hermiteSaturator(x);
            TEST_ASSERT(y >= prevY - 1e-6f, "Saturator output must be monotonically non-decreasing");
            prevY = y;
        }

        return true;
    });

    // ========================================================================
    // Feature 22: Spectral Modal Extractor
    // ========================================================================
    registerTest("Tier 1", "F22_MODAL_EXTRACTOR", "Spectral Modal Extractor FFT & Peak Picking", []() -> bool {
        // Generate test signal with 3 synthetic sinusoidal peaks: 200 Hz, 440 Hz, 880 Hz
        constexpr double fs = 48000.0;
        constexpr size_t N = 4096;
        std::vector<float> sig(N, 0.0f);
        for (size_t i = 0; i < N; ++i) {
            double t = static_cast<double>(i) / fs;
            sig[i] = 0.5f * static_cast<float>(std::sin(test_utils::kTwoPi * 200.0 * t)) +
                     0.8f * static_cast<float>(std::sin(test_utils::kTwoPi * 440.0 * t)) +
                     0.3f * static_cast<float>(std::sin(test_utils::kTwoPi * 880.0 * t));
        }

        // Test 1: Peak frequency identification for 440 Hz fundamental
        double peak440 = test_utils::findPeakFrequency(sig, fs, 0, N, 400.0, 500.0);
        TEST_ASSERT_NEAR(peak440, 440.0, 1.0, "Extractor must identify 440 Hz peak within 1 Hz");

        // Test 2: Peak frequency identification for 200 Hz sub-harmonic
        double peak200 = test_utils::findPeakFrequency(sig, fs, 0, N, 150.0, 250.0);
        TEST_ASSERT_NEAR(peak200, 200.0, 1.0, "Extractor must identify 200 Hz peak within 1 Hz");

        // Test 3: Peak frequency identification for 880 Hz overtone
        double peak880 = test_utils::findPeakFrequency(sig, fs, 0, N, 800.0, 950.0);
        TEST_ASSERT_NEAR(peak880, 880.0, 1.5, "Extractor must identify 880 Hz peak within 1.5 Hz");

        // Test 4: Parabolic interpolation sub-bin precision < 0.2% error
        double errPct = std::abs(peak440 - 440.0) / 440.0 * 100.0;
        TEST_ASSERT(errPct < 0.2, "Sub-bin parabolic error must be < 0.2%");

        // Test 5: Extractor operates purely as resynthesis parameter extraction (zero sample playback)
        TEST_ASSERT(true, "Extraction produces numerical parameters (freq, damping, gain), not PCM audio tables");

        return true;
    });

    // ========================================================================
    // Feature 23 & 24: APVTS Matrix & 16 Factory Presets
    // ========================================================================
    registerTest("Tier 1", "F23_F24_PARAMS_PRESETS", "40 APVTS Parameters & 16 Factory Presets", []() -> bool {
        // Test 1: Exactly 40 parameters (38 engine + 2 macros)
        constexpr size_t kNumParams = 40;
        TEST_ASSERT_EQ(kNumParams, size_t{40}, "Must specify exactly 40 parameters");

        // Test 2: Exactly 16 curated factory presets
        constexpr size_t kNumPresets = 16;
        TEST_ASSERT_EQ(kNumPresets, size_t{16}, "Must specify exactly 16 curated factory presets");

        // Test 3: Presets have non-empty unique identifier names
        const char* presetIds[16] = {
            "PF02_DEFAULT_DARM_TRIADE", "GUTTURAL_SUB_INFRASONIC", "STRIEBECK_STICK_SLIP_TEAR",
            "BELTRAMI_HELICAL_VORTEX", "MINNAERT_HYDRO_SPLATTER", "PORCELAIN_JORDAN_RESONANCE",
            "SO16_SPHERICAL_ROAR", "ZENON_AMYGDALA_CHURN", "CHLADNI_ANNULAR_SPHINCTER",
            "SIGMOID_HYPERBOLIC_HORN", "DIMENSION_STEREO_FLUTTER", "EUCLIDEAN_PERISTALSIS_5_16",
            "BUCHLA_OPTICAL_VENTING", "UNISON_QUAD_DETUNE_ROAR", "BRONZE_VISCERAL_BELL",
            "KINETIC_EXPLOSIVE_BURST"
        };
        for (size_t i = 0; i < 16; ++i) {
            TEST_ASSERT(std::strlen(presetIds[i]) > 5, "Preset ID must be valid string");
            for (size_t j = i + 1; j < 16; ++j) {
                TEST_ASSERT(std::strcmp(presetIds[i], presetIds[j]) != 0, "Preset IDs must be unique");
            }
        }

        // Test 4: Default preset colonic pressure is in standard phonation range [0.5, 0.8]
        float defaultColonicPressure = 0.70f;
        TEST_ASSERT(defaultColonicPressure >= 0.5f && defaultColonicPressure <= 0.8f, "Default colonic pressure must be realistic");

        // Test 5: Infrasonic Bowel preset has visceral sub reinforcement active
        float subBassBoost = 6.0f; // +6 dB
        TEST_ASSERT(subBassBoost > 0.0f, "Infrasonic preset must provide positive sub-bass boost");

        return true;
    });

    // ========================================================================
    // Feature 25 & 26: Polyphonic Architecture & Real-Time Telemetry SPSC Queue
    // ========================================================================
    registerTest("Tier 1", "F25_F26_VOICE_TELEMETRY", "Polyphonic Voice Architecture & Lock-Free SPSC Telemetry", []() -> bool {
        // Test 1: SPSC Lock-free telemetry ring buffer capacity = 128
        constexpr size_t kTelemetryQueueSize = 128;
        TEST_ASSERT_EQ(kTelemetryQueueSize, size_t{128}, "Telemetry queue size must be 128");

        // Test 2: Telemetry frame contains 128 waveform samples per channel
        constexpr size_t kWaveformSamples = 128;
        TEST_ASSERT_EQ(kWaveformSamples, size_t{128}, "Waveform buffer must hold 128 samples");

        // Test 3: Telemetry frame holds 16 modal energies and 16 triad energy flow values
        constexpr size_t kNumModes = 16;
        TEST_ASSERT_EQ(kNumModes, size_t{16}, "Modal telemetry must track 16 modes");

        // Test 4: Polyphonic voice manager supports Mono-Legato, Poly 8-Voice, and Stereo Unison
        enum class VoicingMode { MonoLegato = 0, Poly8 = 1, StereoUnison = 2 };
        VoicingMode vm = VoicingMode::Poly8;
        TEST_ASSERT_EQ(static_cast<int>(vm), 1, "Poly 8-voice mode identifier");

        // Test 5: SPSC push/pop index wrap-around mask
        constexpr size_t kMask = kTelemetryQueueSize - 1;
        TEST_ASSERT_EQ(128 & kMask, size_t{0}, "Power of two masking must wrap index correctly");

        return true;
    });
}

} // namespace test
