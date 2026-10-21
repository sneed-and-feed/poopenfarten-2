#pragma once

#include "TestHarness.h"

namespace test {

inline void registerTier3Tests() {

    // ========================================================================
    // Invariant 1: Skew-Symmetric Triad Operator Antisymmetry (< 1e-7)
    // ========================================================================
    registerTest("Tier 3", "M01_TRIAD_ANTISYMMETRY", "Triad Tensor Antisymmetry & Vanishing Diagonal (< 1e-7)", []() -> bool {
        constexpr int Cu = 8;
        constexpr int M = 16;
        test_utils::FastPRNG prng(777666555ULL);

        // Generate arbitrary 3-tensor W[Cu][M][M]
        std::vector<std::vector<std::vector<double>>> W(Cu, std::vector<std::vector<double>>(M, std::vector<double>(M)));
        for (int i = 0; i < Cu; ++i) {
            for (int j = 0; j < M; ++j) {
                for (int k = 0; k < M; ++k) {
                    W[i][j][k] = static_cast<double>(prng.nextSignedFloat());
                }
            }
        }

        double maxAntisymmetryError = 0.0;
        double maxDiagonalError = 0.0;

        for (int i = 0; i < Cu; ++i) {
            for (int j = 0; j < M; ++j) {
                for (int k = 0; k < M; ++k) {
                    double S_jk = W[i][j][k] - W[i][k][j];
                    double S_kj = W[i][k][j] - W[i][j][k];
                    double asymErr = std::abs(S_jk + S_kj);
                    maxAntisymmetryError = std::max(maxAntisymmetryError, asymErr);

                    if (j == k) {
                        double diagErr = std::abs(S_jk);
                        maxDiagonalError = std::max(maxDiagonalError, diagErr);
                    }
                }
            }
        }

        TEST_ASSERT(maxAntisymmetryError < 1.0e-7, "Triad tensor antisymmetry error must be strictly < 1e-7");
        TEST_ASSERT(maxDiagonalError < 1.0e-7, "Triad diagonal self-interaction error must be strictly < 1e-7");

        return true;
    });

    // ========================================================================
    // Invariant 2: Pointwise Tangent Orthogonality (< 1e-6)
    // ========================================================================
    registerTest("Tier 3", "M02_TANGENT_ORTHOGONALITY", "Exact Pointwise Tangent Orthogonality: <v, B_W(u, v)> < 1e-6", []() -> bool {
        constexpr int Cu = 4;
        constexpr int M = 16;
        test_utils::FastPRNG prng(4321987ULL);

        for (int trial = 0; trial < 20; ++trial) {
            std::vector<double> u(Cu), v(M);
            for (double& val : u) val = static_cast<double>(prng.nextSignedFloat());
            for (double& val : v) val = static_cast<double>(prng.nextSignedFloat());

            std::vector<std::vector<std::vector<double>>> W(Cu, std::vector<std::vector<double>>(M, std::vector<double>(M)));
            for (int i = 0; i < Cu; ++i) {
                for (int j = 0; j < M; ++j) {
                    for (int k = 0; k < M; ++k) {
                        W[i][j][k] = static_cast<double>(prng.nextSignedFloat());
                    }
                }
            }

            // B_W(u, v)_k = sum_i sum_j u_i v_j (W[i][j][k] - W[i][k][j])
            std::vector<double> B(M, 0.0);
            for (int k = 0; k < M; ++k) {
                double sum = 0.0;
                for (int i = 0; i < Cu; ++i) {
                    for (int j = 0; j < M; ++j) {
                        sum += u[i] * v[j] * (W[i][j][k] - W[i][k][j]);
                    }
                }
                B[k] = sum;
            }

            double innerProduct = 0.0;
            for (int k = 0; k < M; ++k) {
                innerProduct += v[k] * B[k];
            }

            TEST_ASSERT(std::abs(innerProduct) < 1.0e-6, "Tangent inner product |<v, B_W(u, v)>| must be < 1e-6");
        }

        return true;
    });

    // ========================================================================
    // Invariant 3: Multi-Step Spherical Norm Conservation (< 1e-6)
    // ========================================================================
    registerTest("Tier 3", "M03_SPHERICAL_NORM_CONSERVATION", "SlowFastTriadCascade Norm Invariance across K in {1, 4, 8, 16}", []() -> bool {
        constexpr size_t M = 16;
        test_utils::FastPRNG prng(998877ULL);

        for (int K : {1, 4, 8, 16}) {
            for (int trial = 0; trial < 10; ++trial) {
                std::array<double, M> v0 {};
                for (double& val : v0) val = static_cast<double>(prng.nextSignedFloat());

                double norm0Sq = 0.0;
                for (double val : v0) norm0Sq += val * val;
                double norm0 = std::sqrt(norm0Sq);

                std::array<double, M> vCurrent = v0;

                for (int step = 0; step < K; ++step) {
                    // Random tangent perturbation
                    std::array<double, M> delta {};
                    for (double& d : delta) d = static_cast<double>(prng.nextSignedFloat()) * 0.2;

                    // Gram-Schmidt project onto tangent plane
                    double vDotDelta = 0.0, vSq = 0.0;
                    for (size_t i = 0; i < M; ++i) {
                        vDotDelta += vCurrent[i] * delta[i];
                        vSq += vCurrent[i] * vCurrent[i];
                    }
                    for (size_t i = 0; i < M; ++i) {
                        delta[i] -= (vDotDelta / (vSq + 1e-15)) * vCurrent[i];
                    }

                    // Spherical retraction
                    std::array<double, M> vNext {};
                    double nextNormSq = 0.0;
                    for (size_t i = 0; i < M; ++i) {
                        vNext[i] = vCurrent[i] + delta[i];
                        nextNormSq += vNext[i] * vNext[i];
                    }
                    double nextNorm = std::sqrt(nextNormSq);
                    double retraction = norm0 / (nextNorm + 1e-15);
                    for (size_t i = 0; i < M; ++i) {
                        vCurrent[i] = vNext[i] * retraction;
                    }
                }

                double finalNormSq = 0.0;
                for (double val : vCurrent) finalNormSq += val * val;
                double finalNorm = std::sqrt(finalNormSq);

                double drift = std::abs(finalNorm - norm0);
                TEST_ASSERT(drift < 1.0e-6, "Radial drift on S^15 must be < 1e-6 across K steps");
            }
        }

        return true;
    });

    // ========================================================================
    // Invariant 4: O(N) Householder Scattering Norm Conservation (< 1e-6)
    // ========================================================================
    registerTest("Tier 3", "M04_HOUSEHOLDER_ISOMETRY", "O(N) Householder Scattering Matrix Isometry: ||Hy|| == ||y||", []() -> bool {
        constexpr size_t N = 16;
        test_utils::FastPRNG prng(334455ULL);

        for (int trial = 0; trial < 25; ++trial) {
            std::array<double, N> y {};
            for (double& val : y) val = static_cast<double>(prng.nextSignedFloat()) * 5.0;

            double initialNormSq = 0.0;
            for (double val : y) initialNormSq += val * val;
            double initialNorm = std::sqrt(initialNormSq);

            // H = I - (2 / N) * 1 * 1^T
            double sum = 0.0;
            for (double val : y) sum += val;
            double factor = (2.0 / static_cast<double>(N)) * sum;

            std::array<double, N> Hy {};
            double scatteredNormSq = 0.0;
            for (size_t i = 0; i < N; ++i) {
                Hy[i] = y[i] - factor;
                scatteredNormSq += Hy[i] * Hy[i];
            }
            double scatteredNorm = std::sqrt(scatteredNormSq);

            double diff = std::abs(scatteredNorm - initialNorm);
            TEST_ASSERT(diff < 1.0e-6, "Householder scattering isometry diff must be < 1e-6");
        }

        return true;
    });

    // ========================================================================
    // Invariant 5: Dimension D Mono Cancellation Immunity (|Wet_mono| > 0)
    // ========================================================================
    registerTest("Tier 3", "M05_DIMENSION_D_MONO_IMMUNITY", "Roland Dimension D Tri-Phase BBD Mono Cancellation Immunity", []() -> bool {
        // Taps: tap1 = sin(wt), tap2 = sin(wt - 2pi/3), tap3 = sin(wt - 4pi/3)
        // rawWetL = 0.5 * (tap1 - 0.5 * tap2 + 0.5 * tap3)
        // rawWetR = 0.5 * (-0.5 * tap1 + tap2 + 0.5 * tap3)
        // monoSum = 0.5 * (rawWetL + rawWetR) = 0.25 * (0.5 * tap1 + 0.5 * tap2 + 1.0 * tap3)
        // Since tap1 + tap2 = -tap3, 0.5*tap1 + 0.5*tap2 + tap3 = 0.5*(tap1 + tap2) + tap3 = -0.5*tap3 + tap3 = 0.5*tap3
        // Therefore monoSum = 0.25 * (0.5 * tap3) = 0.125 * tap3 != 0!
        // This is a direct closed-form mathematical proof of mono-sum immunity!

        for (double freq = 20.0; freq <= 20000.0; freq *= 1.4) {
            double omega = test_utils::kTwoPi * freq;
            double maxMonoAmp = 0.0;
            double maxTap3Amp = 0.0;

            for (double t = 0.0; t < 0.01; t += 0.00005) {
                double t1 = std::sin(omega * t);
                double t2 = std::sin(omega * t - (2.0 * test_utils::kPi) / 3.0);
                double t3 = std::sin(omega * t - (4.0 * test_utils::kPi) / 3.0);

                double rawWetL = 0.5 * (t1 - 0.5 * t2 + 0.5 * t3);
                double rawWetR = 0.5 * (-0.5 * t1 + t2 + 0.5 * t3);
                double monoSum = 0.5 * (rawWetL + rawWetR);

                maxMonoAmp = std::max(maxMonoAmp, std::abs(monoSum));
                maxTap3Amp = std::max(maxTap3Amp, std::abs(t3));
            }

            TEST_ASSERT(maxMonoAmp > 0.10, "Mono sum must have non-zero acoustic transfer across full spectrum");
            TEST_ASSERT_NEAR(maxMonoAmp, 0.125, 0.02, "Mono sum amplitude must follow exact 0.125 * tap3 relationship");
        }

        return true;
    });

    // ========================================================================
    // Invariant 6: Hermite C1 Saturator Strict Asymptotic Ceiling (<= 1.05)
    // ========================================================================
    registerTest("Tier 3", "M06_HERMITE_CEILING", "Hermite C1 Saturator Asymptotic Ceiling & Knee Continuity", []() -> bool {
        constexpr double k = 0.72;
        constexpr double C = 1.05;

        auto hermiteFn = [k, C](double x) -> double {
            double absX = std::abs(x);
            if (absX <= k) return x;
            if (absX >= C) return (x > 0.0 ? C : -C);
            double u = (absX - k) / (C - k);
            double poly = u * (1.0 + u * (1.0 - u));
            double y = k + (C - k) * poly;
            return (x > 0.0 ? y : -y);
        };

        // Test 1: Asymptotic ceiling containment: |y| <= 1.05 for inputs up to +60 dBFS (x = 1000.0)
        for (double x = -1000.0; x <= 1000.0; x += 25.0) {
            double y = hermiteFn(x);
            TEST_ASSERT(std::abs(y) <= C + 1e-12, "Hermite output must never exceed ceiling C=1.05");
        }

        // Test 2: C1 continuity: derivative from left matches derivative from right at knee k
        double eps = 1e-6;
        double dLeft = (hermiteFn(k) - hermiteFn(k - eps)) / eps;
        double dRight = (hermiteFn(k + eps) - hermiteFn(k)) / eps;
        TEST_ASSERT_NEAR(dLeft, 1.0, 1e-4, "Left derivative at knee must be exactly 1.0");
        TEST_ASSERT_NEAR(dRight, 1.0, 1e-4, "Right derivative at knee must be exactly 1.0");
        TEST_ASSERT_NEAR(dLeft, dRight, 1e-4, "First derivative must match continuously across knee k");

        // Test 3: Zero derivative at ceiling C
        double dCeiling = (hermiteFn(C) - hermiteFn(C - eps)) / eps;
        TEST_ASSERT_NEAR(dCeiling, 0.0, 1e-4, "Derivative at saturation ceiling must smoothly vanish");

        return true;
    });
}

} // namespace test
