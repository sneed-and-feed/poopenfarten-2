#include "source/dsp/DspMath.h"
#include "source/dsp/DarkSnailMath.h"
#include "source/dsp/MasteringChain.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <array>
#include <cmath>
#include <random>
#include <chrono>
#include <cassert>
#include <numeric>
#include <algorithm>

using namespace pf02::dsp;

#define CHALLENGER_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "\n[ADVERSARIAL FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            std::exit(1); \
        } \
    } while (0)

// ============================================================================
// Adversarial PRNG for reproducible adversarial probing
// ============================================================================
class AdversarialRNG {
public:
    explicit AdversarialRNG(uint64_t seed = 0xDEADBEEFCAFEULL) : mState(seed) {}

    uint64_t nextU64() noexcept {
        mState ^= (mState >> 12);
        mState ^= (mState << 25);
        mState ^= (mState >> 27);
        return mState * 0x2545F4914F6CDD1DULL;
    }

    float nextFloat() noexcept {
        return static_cast<float>(nextU64() >> 40) * (1.0f / 16777216.0f);
    }

    float nextSignedFloat() noexcept {
        return nextFloat() * 2.0f - 1.0f;
    }

    float nextExtremeFloat() noexcept {
        float base = nextSignedFloat();
        int exp = static_cast<int>(nextU64() % 13) - 6; // -6 to +6
        return base * std::pow(10.0f, static_cast<float>(exp));
    }

private:
    uint64_t mState;
};

// ============================================================================
// Probe 1: Triad Antisymmetry & Pointwise Orthogonality Across 1,000 Vectors
// ============================================================================
void probeTriadAntisymmetryAndOrthogonality() {
    std::cout << "\n=======================================================\n";
    std::cout << "[PROBE 1] Triad Antisymmetry & Orthogonality Stress-Test\n";
    std::cout << "=======================================================\n";

    AdversarialRNG rng(0x1337C0D3ULL);
    SkewSymmetricTriadOperator<16, 16> triadOp;

    // 1. Stress test with randomized 3-tensor weights
    double maxAntisymError = 0.0;
    double maxDiagError = 0.0;

    for (size_t trial = 0; trial < 10; ++trial) {
        // Randomize weights with extreme dynamic range
        for (size_t i = 0; i < 16; ++i) {
            for (size_t j = 0; j < 16; ++j) {
                for (size_t k = 0; k < 16; ++k) {
                    triadOp.setWeight(i, j, k, rng.nextExtremeFloat());
                }
            }
        }

        // Check S(W, i, c1, c2) = -S(W, i, c2, c1) and S(W, i, c, c) == 0
        for (size_t i = 0; i < 16; ++i) {
            for (size_t c1 = 0; c1 < 16; ++c1) {
                for (size_t c2 = 0; c2 < 16; ++c2) {
                    float s1 = triadOp.skewElement(i, c1, c2);
                    float s2 = triadOp.skewElement(i, c2, c1);
                    double err = std::abs(static_cast<double>(s1) + static_cast<double>(s2));
                    maxAntisymError = std::max(maxAntisymError, err);

                    if (c1 == c2) {
                        maxDiagError = std::max(maxDiagError, std::abs(static_cast<double>(s1)));
                    }
                }
            }
        }
    }

    std::cout << "  -> Evaluated 10 randomized tensors (40,960 elements each)\n";
    std::cout << "  -> Peak Antisymmetry Error: " << std::scientific << maxAntisymError << "\n";
    std::cout << "  -> Peak Vanishing Diagonal Error: " << std::scientific << maxDiagError << "\n";

    CHALLENGER_ASSERT(maxAntisymError < 1.0e-7, "Antisymmetry error must be strictly < 1e-7");
    CHALLENGER_ASSERT(maxDiagError < 1.0e-7, "Diagonal error must be strictly < 1e-7");

    // 2. Probe 1,000 randomized state vectors u, v
    // Testing inner product <v, B_W(u, v)> < 1e-7
    double maxInnerProduct = 0.0;
    double sumInnerProduct = 0.0;
    std::array<double, 1000> innerProducts {};

    // Re-initialize standard scaled weights for realistic DSP state
    triadOp.initDefaultWeights();

    for (size_t vecIdx = 0; vecIdx < 1000; ++vecIdx) {
        std::array<float, 16> u {};
        std::array<float, 16> v {};

        // Test across 1,000 state vectors on S^15 (the actual manifold of DarkSnail)
        float uNorm = 0.0f, vNorm = 0.0f;
        for (size_t i = 0; i < 16; ++i) {
            u[i] = rng.nextSignedFloat();
            v[i] = rng.nextSignedFloat();
            uNorm += u[i] * u[i];
            vNorm += v[i] * v[i];
        }
        uNorm = std::sqrt(std::max(1.0e-12f, uNorm));
        vNorm = std::sqrt(std::max(1.0e-12f, vNorm));
        for (size_t i = 0; i < 16; ++i) {
            u[i] /= uNorm;
            v[i] /= vNorm;
        }

        // Evaluate B_W(u, v)
        std::array<float, 16> b = triadOp.evaluate(u, v);

        // Compute inner product <v, B_W(u, v)>
        double inner = 0.0;
        for (size_t i = 0; i < 16; ++i) {
            inner += static_cast<double>(v[i]) * static_cast<double>(b[i]);
        }
        double absInner = std::abs(inner);
        innerProducts[vecIdx] = absInner;
        maxInnerProduct = std::max(maxInnerProduct, absInner);
        sumInnerProduct += absInner;

        CHALLENGER_ASSERT(absInner < 1.0e-7, "Pointwise orthogonality <v, B_W(u, v)> exceeded 1e-7 threshold on S^15!");
    }



    double meanInnerProduct = sumInnerProduct / 1000.0;
    std::cout << "  -> Evaluated 1,000 randomized state vectors u, v\n";
    std::cout << "  -> Mean |<v, B_W(u, v)>|: " << std::scientific << meanInnerProduct << "\n";
    std::cout << "  -> Peak |<v, B_W(u, v)>|: " << std::scientific << maxInnerProduct << "\n";

    size_t countExceeding1e7 = 0;
    size_t countExceeding1e6 = 0;
    for (size_t i = 0; i < 1000; ++i) {
        if (innerProducts[i] >= 1.0e-7) countExceeding1e7++;
        if (innerProducts[i] >= 1.0e-6) countExceeding1e6++;
    }
    std::cout << "  -> Vectors with |<v, B>| >= 1e-7: " << countExceeding1e7 << " / 1000\n";
    std::cout << "  -> Vectors with |<v, B>| >= 1e-6: " << countExceeding1e6 << " / 1000\n";

    std::cout << "  -> Evaluated 1,000 randomized state vectors u, v\n";
    std::cout << "  -> Mean |<v, B_W(u, v)>|: " << std::scientific << meanInnerProduct << "\n";
    std::cout << "  -> Peak |<v, B_W(u, v)>|: " << std::scientific << maxInnerProduct << " (Threshold: 1.000e-07)\n";
    std::cout << "  [PASS] Probe 1 strictly satisfies all antisymmetry and orthogonality invariants.\n";
}

// ============================================================================
// Probe 2: Spherical Norm Invariance Across K in {1, 4, 8, 16}
// ============================================================================
void probeSphericalNormInvariance() {
    std::cout << "\n=======================================================\n";
    std::cout << "[PROBE 2] Spherical Norm Invariance Stress-Test\n";
    std::cout << "=======================================================\n";

    AdversarialRNG rng(0x8472910ULL);
    SlowFastTriadCascade cascade;
    SkewSymmetricTriadOperator<16, 16> triadOp;

    const std::vector<int> testKs = { 1, 4, 8, 16 };
    const std::vector<float> testGains = { 0.1f, 0.5f, 0.85f, 1.5f, 3.0f };

    double globalMaxRadialDrift = 0.0;
    double globalMaxTangentOrth = 0.0;

    for (int K : testKs) {
        cascade.setUnrollSteps(K);
        double maxDriftForK = 0.0;
        double maxOrthForK = 0.0;

        for (float gain : testGains) {
            cascade.setCascadeGain(gain);

            for (size_t trial = 0; trial < 100; ++trial) {
                // Randomize interaction tensor
                for (size_t i = 0; i < 16; ++i) {
                    for (size_t j = 0; j < 16; ++j) {
                        for (size_t k = 0; k < 16; ++k) {
                            if (j != k) {
                                triadOp.setWeight(i, j, k, rng.nextSignedFloat() * 0.25f);
                            }
                        }
                    }
                }

                // Initial fast and slow vectors
                std::array<float, 16> uSlow {};
                std::array<float, 16> vFast {};

                // Normalize vFast onto S^15 unit sphere
                float vNorm = 0.0f;
                for (size_t i = 0; i < 16; ++i) {
                    uSlow[i] = rng.nextSignedFloat() * 0.25f;
                    vFast[i] = rng.nextSignedFloat();
                    vNorm += vFast[i] * vFast[i];
                }
                vNorm = std::sqrt(std::max(1.0e-12f, vNorm));
                for (size_t i = 0; i < 16; ++i) vFast[i] /= vNorm;


                // Measure exact initial L2 norm squared
                double norm0Sq = 0.0;
                for (size_t i = 0; i < 16; ++i) {
                    norm0Sq += static_cast<double>(vFast[i]) * static_cast<double>(vFast[i]);
                }

                // Execute cascade
                std::array<float, 16> vOut = cascade.processCascade(uSlow, vFast, triadOp, K);

                // Measure exact final L2 norm squared
                double normFinalSq = 0.0;
                for (size_t i = 0; i < 16; ++i) {
                    normFinalSq += static_cast<double>(vOut[i]) * static_cast<double>(vOut[i]);
                }

                double radialDrift = std::abs(normFinalSq - norm0Sq);
                double tangentOrth = static_cast<double>(cascade.getLastTangentOrthError());

                maxDriftForK = std::max(maxDriftForK, radialDrift);
                maxOrthForK = std::max(maxOrthForK, tangentOrth);
                globalMaxRadialDrift = std::max(globalMaxRadialDrift, radialDrift);
                globalMaxTangentOrth = std::max(globalMaxTangentOrth, tangentOrth);

                if (radialDrift >= 1.0e-6) {
                    std::cout << "[DIAGNOSTIC] K=" << K << " gain=" << gain << " drift=" << std::scientific << radialDrift 
                              << " norm0Sq=" << norm0Sq << " normFinalSq=" << normFinalSq 
                              << " floatDrift=" << cascade.getLastRadialDrift() << "\n";
                }

                CHALLENGER_ASSERT(radialDrift < 1.0e-6, "Radial drift exceeded 1e-6 threshold!");
                CHALLENGER_ASSERT(tangentOrth < 1.0e-6, "Tangent orthogonality exceeded 1e-6 threshold!");

            }
        }

        std::cout << "  -> K=" << std::setw(2) << K 
                  << " | Max Radial Drift: " << std::scientific << maxDriftForK 
                  << " | Max Tangent Orth Err: " << std::scientific << maxOrthForK << " (PASSED)\n";
    }

    std::cout << "  -> Global Worst-Case Radial Drift: " << std::scientific << globalMaxRadialDrift << " (Threshold: 1.000e-06)\n";
    std::cout << "  -> Global Worst-Case Tangent Orth: " << std::scientific << globalMaxTangentOrth << " (Threshold: 1.000e-06)\n";
    std::cout << "  [PASS] Probe 2 strictly satisfies spherical norm invariance across all K in {1, 4, 8, 16}.\n";
}

// ============================================================================
// Probe 3: Householder Scattering Matrix Norm Conservation
// ============================================================================
void probeHouseholderScatteringMatrix() {
    std::cout << "\n=======================================================\n";
    std::cout << "[PROBE 3] Householder Scattering Matrix Norm Conservation\n";
    std::cout << "=======================================================\n";

    AdversarialRNG rng(0x987654321ULL);
    double maxL2Diff = 0.0;
    double maxL2DiffSq = 0.0;

    // 1. Check special eigenvectors
    // Uniform vector [1, 1, ..., 1] should be reflected to [-1, -1, ..., -1]
    {
        std::array<float, 16> uniformVec;
        uniformVec.fill(2.5f);
        double normBefore = 0.0;
        for (float x : uniformVec) normBefore += static_cast<double>(x) * static_cast<double>(x);
        normBefore = std::sqrt(normBefore);

        std::array<float, 16> reflected = householderReflect16(uniformVec, 1.0f);
        double normAfter = 0.0;
        for (float x : reflected) normAfter += static_cast<double>(x) * static_cast<double>(x);
        normAfter = std::sqrt(normAfter);

        double diff = std::abs(normBefore - normAfter);
        maxL2Diff = std::max(maxL2Diff, diff);
        CHALLENGER_ASSERT(diff < 1.0e-6, "Uniform vector reflection norm difference < 1e-6");

        // Verify sign flip: reflected[i] should be -2.5f
        for (size_t i = 0; i < 16; ++i) {
            CHALLENGER_ASSERT(std::abs(reflected[i] - (-2.5f)) < 1.0e-6f, "Uniform vector sign flip verified");
        }
    }

    // 2. Vector orthogonal to [1, ..., 1] should be unaffected (eigenvalue +1)
    {
        std::array<float, 16> orthoVec {};
        for (size_t i = 0; i < 8; ++i) orthoVec[i] = 1.0f;
        for (size_t i = 8; i < 16; ++i) orthoVec[i] = -1.0f; // sum == 0

        std::array<float, 16> reflected = householderReflect16(orthoVec, 1.0f);
        for (size_t i = 0; i < 16; ++i) {
            CHALLENGER_ASSERT(std::abs(reflected[i] - orthoVec[i]) < 1.0e-6f, "Orthogonal vector invariant");
        }
    }

    // 3. 10,000 randomized 16-element vectors across diverse audio distributions
    for (size_t trial = 0; trial < 10000; ++trial) {
        std::array<float, 16> y {};
        if (trial < 3000) {
            // Standard uniform in [-1, 1]
            for (size_t i = 0; i < 16; ++i) y[i] = rng.nextSignedFloat();
        } else if (trial < 6000) {
            // Unit-sphere normalized (||y|| = 1.0)
            float norm = 0.0f;
            for (size_t i = 0; i < 16; ++i) {
                y[i] = rng.nextSignedFloat();
                norm += y[i] * y[i];
            }
            norm = std::sqrt(std::max(1.0e-12f, norm));
            for (size_t i = 0; i < 16; ++i) y[i] /= norm;
        } else if (trial < 8000) {
            // Scaled audio modes in [-0.5, 0.5]
            for (size_t i = 0; i < 16; ++i) y[i] = rng.nextSignedFloat() * 0.5f;
        } else {
            // Decaying partial amplitudes: 1/k
            for (size_t i = 0; i < 16; ++i) y[i] = rng.nextSignedFloat() / static_cast<float>(i + 1);
        }

        double normSqBefore = 0.0;
        for (size_t i = 0; i < 16; ++i) {
            normSqBefore += static_cast<double>(y[i]) * static_cast<double>(y[i]);
        }
        double normBefore = std::sqrt(normSqBefore);

        applyHouseholderScattering16(y, 1.0f);

        double normSqAfter = 0.0;
        for (size_t i = 0; i < 16; ++i) {
            normSqAfter += static_cast<double>(y[i]) * static_cast<double>(y[i]);
        }
        double normAfter = std::sqrt(normSqAfter);

        double diffSq = std::abs(normSqBefore - normSqAfter);
        double diff = std::abs(normBefore - normAfter);

        maxL2DiffSq = std::max(maxL2DiffSq, diffSq);
        maxL2Diff = std::max(maxL2Diff, diff);

        CHALLENGER_ASSERT(diff < 1.0e-6, "Householder L2 norm difference exceeded 1e-6 threshold!");
    }


    std::cout << "  -> Evaluated 10,000 randomized 16-element vectors across extreme ranges\n";
    std::cout << "  -> Peak ||Hy||_2 - ||y||_2 Difference: " << std::scientific << maxL2Diff << " (Threshold: 1.000e-06)\n";
    std::cout << "  -> Peak ||Hy||_2^2 - ||y||_2^2 Difference: " << std::scientific << maxL2DiffSq << "\n";
    std::cout << "  [PASS] Probe 3 strictly confirms Householder scattering matrix isometry.\n";
}

// ============================================================================
// Probe 4: Roland Dimension D Mono Immunity Across Fine Frequency Sweep
// ============================================================================
void probeDimensionDMonoImmunity() {
    std::cout << "\n=======================================================\n";
    std::cout << "[PROBE 4] Roland Dimension D Mono Immunity Fine Frequency Sweep\n";
    std::cout << "=======================================================\n";
    std::cout << "  -> Sweeping 10 Hz to 20,000 Hz in fine 1 Hz steps (19,991 frequencies)...\n";

    constexpr double kSampleRate = 48000.0;
    MasteringChain chain;
    chain.prepare(kSampleRate);

    MasteringChainParams params;
    params.dimensionDEnable = true;
    params.dimensionDMix = 100.0f; // 100% wet
    params.dimensionDSpread = 1.0f; // Standard stereo spread
    params.colonicSaturationDrive = 0.0f;
    params.visceralSubReinforceDb = -36.0f; // Inactive
    params.masterVolumeDb = 0.0f;

    double globalMinPeakMono = 1e9;
    double globalMinRmsMono = 1e9;
    double globalMaxPeakMono = 0.0;
    int worstFreqPeak = 0;
    int worstFreqRms = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Test across all 4 modes
    for (int modeInt = 0; modeInt < 4; ++modeInt) {
        params.dimensionDMode = static_cast<DimensionChorusMode>(modeInt);
        chain.reset();

        double minPeakForMode = 1e9;
        double minRmsForMode = 1e9;
        int worstFreqForMode = 0;

        // Sweep 10 Hz to 20,000 Hz at 1 Hz steps
        for (int freq = 10; freq <= 20000; ++freq) {
            const double f = static_cast<double>(freq);
            const double omega = kTwoPi * (f / kSampleRate);

            // Test 1024 samples per frequency (warmup + evaluation)
            constexpr int kWarmup = 256;
            constexpr int kEval = 1024;
            double sumSq = 0.0;
            double maxAbs = 0.0;

            for (int n = 0; n < (kWarmup + kEval); ++n) {
                float sample = static_cast<float>(std::sin(omega * static_cast<double>(n)));
                float outL = 0.0f, outR = 0.0f;
                chain.process(sample, sample, params, outL, outR);

                if (n >= kWarmup) {
                    float wetMono = 0.5f * (outL + outR);
                    double dVal = static_cast<double>(wetMono);
                    sumSq += dVal * dVal;
                    maxAbs = std::max(maxAbs, std::abs(dVal));
                }
            }

            double rms = std::sqrt(sumSq / static_cast<double>(kEval));

            if (maxAbs < minPeakForMode) {
                minPeakForMode = maxAbs;
                worstFreqForMode = freq;
            }
            minRmsForMode = std::min(minRmsForMode, rms);

            if (maxAbs < globalMinPeakMono) {
                globalMinPeakMono = maxAbs;
                worstFreqPeak = freq;
            }
            if (rms < globalMinRmsMono) {
                globalMinRmsMono = rms;
                worstFreqRms = freq;
            }
            globalMaxPeakMono = std::max(globalMaxPeakMono, maxAbs);

            // CRITICAL ADVERSARIAL ASSERTION:
            // Zero complete phase cancellation dips: |Wet_mono| MUST BE STRICTLY > 0!
            CHALLENGER_ASSERT(maxAbs > 1.0e-5, "Complete phase cancellation notch detected! Peak wet mono vanished to 0.");
            CHALLENGER_ASSERT(rms > 1.0e-6, "RMS wet mono vanished to 0! Destructive interference null detected.");
        }

        std::cout << "  -> Mode " << (modeInt + 1) 
                  << " | Min Peak Mono: " << std::fixed << std::setprecision(5) << minPeakForMode 
                  << " at " << worstFreqForMode << " Hz | Min RMS Mono: " << minRmsForMode << " (IMMUNE)\n";
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::cout << "  -> Total Frequencies Tested: 19,991 frequencies x 4 modes = 79,964 sweep blocks in " 
              << elapsedMs << " ms\n";
    std::cout << "  -> Global Minimum Peak Wet Mono: " << std::fixed << std::setprecision(6) << globalMinPeakMono 
              << " at " << worstFreqPeak << " Hz\n";
    std::cout << "  -> Global Minimum RMS Wet Mono:  " << std::fixed << std::setprecision(6) << globalMinRmsMono 
              << " at " << worstFreqRms << " Hz\n";
    std::cout << "  -> Global Maximum Peak Wet Mono: " << std::fixed << std::setprecision(6) << globalMaxPeakMono << "\n";
    std::cout << "  [PASS] Probe 4 strictly confirms zero phase cancellation dips across 10 Hz - 20,000 Hz.\n";
}

// ============================================================================
// Main Execution Entry Point
// ============================================================================
int main() {
    std::cout << "================================================================================\n";
    std::cout << "   CHALLENGER M1-2: ADVERSARIAL PROBE OF MATHEMATICAL INVARIANTS & MONO IMMUNITY\n";
    std::cout << "   PF-02 · Darm-Akustischer & Ultrametrischer Triaden-Synthesizer                \n";
    std::cout << "================================================================================\n";

    ScopedNoDenormals denormalGuard;

    auto totalStart = std::chrono::high_resolution_clock::now();

    probeTriadAntisymmetryAndOrthogonality();
    probeSphericalNormInvariance();
    probeHouseholderScatteringMatrix();
    probeDimensionDMonoImmunity();

    auto totalEnd = std::chrono::high_resolution_clock::now();
    auto totalElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - totalStart).count();

    std::cout << "\n================================================================================\n";
    std::cout << "   ALL 4 ADVERSARIAL MATHEMATICAL PROBES PASSED WITH 100% EMPIRICAL SUCCESS     \n";
    std::cout << "   Total Execution Time: " << totalElapsedMs << " ms                             \n";
    std::cout << "   VERDICT: APPROVE                                                             \n";
    std::cout << "================================================================================\n";

    return 0;
}
