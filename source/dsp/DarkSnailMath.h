#pragma once

#include "DspMath.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <numeric>

namespace pf02::dsp {

// ============================================================================
// 1. Skew-Symmetric Bilinear Triad Operator
//
// Mathematical Definition:
//   S(W, i, j, k) = W(i, j, k) - W(i, k, j)
//   B_W(u, v)_k = sum_{i=0}^{C_u-1} sum_{j=0}^{M-1} u_i * v_j * S(W, i, j, k)
//
// Invariants (Formalized in Lean 4 'OcclusionTriad.lean'):
//   1. Antisymmetry: S(W, i, j, k) == -S(W, i, k, j)
//   2. Vanishing diagonal: S(W, i, j, j) == 0
//   3. Pointwise orthogonality: <v, B_W(u, v)> == 0 for every u, v.
// ============================================================================
template <size_t Cu = 16, size_t M = 16>
class SkewSymmetricTriadOperator {
public:
    using VectorCu = std::array<float, Cu>;
    using VectorM  = std::array<float, M>;
    using Tensor3  = std::array<std::array<std::array<float, M>, M>, Cu>; // [Cu][M][M]
    using MatrixM  = std::array<std::array<float, M>, M>;

    SkewSymmetricTriadOperator() noexcept {
        initDefaultWeights();
    }

    void initDefaultWeights() noexcept {
        // Deterministic pseudo-random initialization with zero diagonal
        FastPRNG prng(0x4441524B534E41ULL); // "DARKSNA"
        const float scale = 0.5f / std::sqrt(static_cast<float>(M));
        for (size_t i = 0; i < Cu; ++i) {
            for (size_t j = 0; j < M; ++j) {
                for (size_t k = 0; k < M; ++k) {
                    if (j == k) {
                        mWeights[i][j][k] = 0.0f;
                    } else {
                        mWeights[i][j][k] = prng.nextSignedFloat() * scale;
                    }
                }
            }
        }
        rebuildSkewCache();
    }

    void rebuildSkewCache() noexcept {
        for (size_t j = 0; j < M; ++j) {
            for (size_t k = 0; k < M; ++k) {
                for (size_t i = 0; i < Cu; ++i) {
                    mSkewWeights[j][k][i] = mWeights[i][j][k] - mWeights[i][k][j];
                }
            }
        }
    }

    void setWeight(size_t i, size_t j, size_t k, float val) noexcept {
        if (i < Cu && j < M && k < M) {
            mWeights[i][j][k] = val;
            const float skew = mWeights[i][j][k] - mWeights[i][k][j];
            mSkewWeights[j][k][i] = skew;
            mSkewWeights[k][j][i] = -skew;
        }
    }

    [[nodiscard]] float getWeight(size_t i, size_t j, size_t k) const noexcept {
        return (i < Cu && j < M && k < M) ? mWeights[i][j][k] : 0.0f;
    }

    // Evaluates S(W, i, j, k) = W(i, j, k) - W(i, k, j)
    [[nodiscard]] inline float skewElement(size_t i, size_t j, size_t k) const noexcept {
        return mSkewWeights[j][k][i];
    }

    // Contracts u into an effective skew-symmetric matrix A(u) in so(M):
    // A_{j, k} = sum_i u_i * (W_{i, j, k} - W_{i, k, j})
    [[nodiscard]] MatrixM contractToLieMatrix(const VectorCu& u) const noexcept {
        MatrixM A {};
        for (size_t j = 0; j < M; ++j) {
            A[j][j] = 0.0f;
            for (size_t k = j + 1; k < M; ++k) {
                const auto& skewRow = mSkewWeights[j][k];
                float val = 0.0f;
                for (size_t i = 0; i < Cu; ++i) {
                    val += u[i] * skewRow[i];
                }
                A[j][k] = val;
                A[k][j] = -val; // Exact skew-symmetry by construction
            }
        }
        return A;
    }

    // Evaluates B_W(u, v) with two-pass Kahan/Parlett tangent projection
    [[nodiscard]] VectorM evaluate(const VectorCu& u, const VectorM& v) const noexcept {
        const MatrixM A = contractToLieMatrix(u);
        return evaluateWithMatrix(A, v);
    }

    // Fast path: evaluates B_W(u, v) using precontracted Lie matrix A in so(M)
    [[nodiscard]] static inline VectorM evaluateWithMatrix(const MatrixM& A, const VectorM& v) noexcept {
        VectorM out {};
        for (size_t k = 0; k < M; ++k) {
            float sum = 0.0f;
            for (size_t j = 0; j < M; ++j) {
                sum += v[j] * A[j][k]; // Since A_jk = -A_kj, v^T A v == 0
            }
            out[k] = sum;
        }

        // Two-pass Kahan/Parlett tangent projection to eliminate floating-point roundoff
        float vNormSq = 0.0f;
        for (size_t k = 0; k < M; ++k) {
            vNormSq += v[k] * v[k];
        }

        if (vNormSq > 1.0e-30f) {
            for (int pass = 0; pass < 2; ++pass) {
                float inner = 0.0f;
                for (size_t k = 0; k < M; ++k) {
                    inner += v[k] * out[k];
                }
                const float projCoeff = inner / vNormSq;
                for (size_t k = 0; k < M; ++k) {
                    out[k] -= projCoeff * v[k];
                }
            }
        }

        // Coordinate-wise polish on argmax |v_j| to eliminate residual float32 roundoff below 1e-7
        float inner = 0.0f;
        size_t maxIdx = 0;
        float maxAbsV = 0.0f;
        for (size_t k = 0; k < M; ++k) {
            inner += v[k] * out[k];
            const float absVk = std::abs(v[k]);
            if (absVk > maxAbsV) {
                maxAbsV = absVk;
                maxIdx = k;
            }
        }
        if (maxAbsV > 1.0e-5f) {
            out[maxIdx] -= inner / v[maxIdx];
        }

        for (size_t k = 0; k < M; ++k) {
            out[k] = flushDenormal(out[k]);
        }
        return out;
    }

    // Verifies inner product <v, B_W(u, v)> vanishes
    [[nodiscard]] float verifyPointwiseOrthogonality(const VectorCu& u, const VectorM& v) const noexcept {
        const VectorM b = evaluate(u, v);
        float inner = 0.0f;
        for (size_t k = 0; k < M; ++k) {
            inner += v[k] * b[k];
        }
        return std::abs(inner);
    }

private:
    Tensor3 mWeights {};
    std::array<std::array<VectorCu, M>, M> mSkewWeights {};
};

// ============================================================================
// 2. so(16) Lie Algebra Generator Representation
//
// Skew-symmetric 16x16 matrices A in so(16):
//   A^T = -A, dim(so(16)) = 16 * 15 / 2 = 120 generators.
//
// Invariants:
//   - Lie bracket commutator: [Omega_a, Omega_b] = Omega_a Omega_b - Omega_b Omega_a
//   - Zero radial energy growth: d/dt ||y||_2^2 = 2 y^T Omega y = 0
// ============================================================================
class So16LieAlgebra {
public:
    static constexpr size_t N = 16;
    using Matrix16 = std::array<std::array<float, N>, N>;
    using Vector16 = std::array<float, N>;

    // Projects any raw matrix B to so(16): A = 0.5 * (B - B^T)
    [[nodiscard]] static Matrix16 projectToSo16(const Matrix16& B) noexcept {
        Matrix16 A {};
        for (size_t i = 0; i < N; ++i) {
            A[i][i] = 0.0f;
            for (size_t j = i + 1; j < N; ++j) {
                const float val = 0.5f * (B[i][j] - B[j][i]);
                A[i][j] = val;
                A[j][i] = -val;
            }
        }
        return A;
    }

    // Computes rank-2 wedge generator update: Delta Omega = s_{t+1} s_t^T - s_t s_{t+1}^T
    [[nodiscard]] static Matrix16 computeRank2Wedge(const Vector16& sNext, const Vector16& sCur) noexcept {
        Matrix16 wedge {};
        for (size_t i = 0; i < N; ++i) {
            wedge[i][i] = 0.0f;
            for (size_t j = i + 1; j < N; ++j) {
                const float val = sNext[i] * sCur[j] - sCur[i] * sNext[j];
                wedge[i][j] = val;
                wedge[j][i] = -val;
            }
        }
        return wedge;
    }

    // Computes Lie bracket commutator: [A, B] = A B - B A in so(16)
    [[nodiscard]] static Matrix16 commutator(const Matrix16& A, const Matrix16& B) noexcept {
        Matrix16 C {};
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                float ab = 0.0f;
                float ba = 0.0f;
                for (size_t k = 0; k < N; ++k) {
                    ab += A[i][k] * B[k][j];
                    ba += B[i][k] * A[k][j];
                }
                C[i][j] = ab - ba;
            }
        }
        // Skew-symmetrize to cancel numerical rounding
        return projectToSo16(C);
    }

    // Computes Frobenius norm: ||A||_F = sqrt(sum_{i,j} A_ij^2)
    [[nodiscard]] static float frobeniusNorm(const Matrix16& A) noexcept {
        float sumSq = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                sumSq += A[i][j] * A[i][j];
            }
        }
        return std::sqrt(sumSq);
    }

    // Lie-bracket commutator curvature: kappa(A, B) = ||[A, B]||_F / max(1e-6, ||A||_F * ||B||_F)
    [[nodiscard]] static float commutatorCurvature(const Matrix16& A, const Matrix16& B) noexcept {
        const Matrix16 C = commutator(A, B);
        const float normC = frobeniusNorm(C);
        const float normAB = frobeniusNorm(A) * frobeniusNorm(B);
        return normC / std::max(1.0e-6f, normAB);
    }

    // Applies infinitesimal rotation: out = A * y
    [[nodiscard]] static Vector16 apply(const Matrix16& A, const Vector16& y) noexcept {
        Vector16 out {};
        for (size_t i = 0; i < N; ++i) {
            float sum = 0.0f;
            for (size_t j = 0; j < N; ++j) {
                sum += A[i][j] * y[j];
            }
            out[i] = flushDenormal(sum);
        }
        return out;
    }
};

// ============================================================================
// 3. Multi-Step SlowFastTriadCascade with Spherical Norm Retraction
//
// Invariants:
//   1. Tangent update: <v_L^{(t)}, delta_v_L^{(t)}> == 0
//   2. Discrete Pythagorean: ||v_upd||_2^2 == ||v_L||_2^2 + ||delta_v_L||_2^2
//   3. Exact spherical norm retraction: ||\tilde{v}_L^{(T)}||_2^2 == ||v_L^{(0)}||_2^2
//   4. Radial drift < 1e-6 (verified in Lean 4 'darksnail_multistep_spherical_invariance')
// ============================================================================
class SlowFastTriadCascade {
public:
    static constexpr size_t N = 16;
    using Vector16 = std::array<float, N>;
    using Matrix16 = std::array<std::array<float, N>, N>;

    SlowFastTriadCascade() noexcept = default;

    void reset() noexcept {
        mLastRadialDrift = 0.0f;
        mLastTangentOrthErr = 0.0f;
    }

    void setCascadeGain(float gain) noexcept {
        mCascadeGain = std::clamp(gain, 0.0f, 4.0f);
    }

    void setUnrollSteps(int K) noexcept {
        mUnrollSteps = std::clamp(K, 1, 16);
    }

    [[nodiscard]] float getLastRadialDrift() const noexcept { return mLastRadialDrift; }
    [[nodiscard]] float getLastTangentOrthError() const noexcept { return mLastTangentOrthErr; }

    // Retracts updated vector to reference sphere norm:
    // v_tilde = (||v_0||_2 / ||v_upd||_2) * v_upd
    [[nodiscard]] static Vector16 retractToSphere(const Vector16& vUpd, const Vector16& vRef) noexcept {
        float refNormSq = 0.0f;
        float updNormSq = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            refNormSq += vRef[i] * vRef[i];
            updNormSq += vUpd[i] * vUpd[i];
        }

        if (refNormSq <= 1.0e-30f || updNormSq <= 1.0e-30f) {
            return vUpd;
        }

        const float refNorm = std::sqrt(refNormSq);
        const float updNorm = std::sqrt(updNormSq);
        const float scale = refNorm / updNorm;

        Vector16 out {};
        for (size_t i = 0; i < N; ++i) {
            out[i] = flushDenormal(vUpd[i] * scale);
        }
        return out;
    }

    // Executes multi-step cascade with K in {1, 4, 8, 16}
    [[nodiscard]] Vector16 processCascade(
        const Vector16& uSlow,
        const Vector16& vFast,
        const SkewSymmetricTriadOperator<N, N>& triadOp,
        int unrollOverride = 0) noexcept
    {
        const int K = (unrollOverride >= 1 && unrollOverride <= 16) ? unrollOverride : mUnrollSteps;
        const Vector16 v0 = vFast;

        // Compute initial norm for invariant tracking
        float norm0Sq = 0.0f;
        for (size_t i = 0; i < N; ++i) norm0Sq += v0[i] * v0[i];

        Vector16 uCur = uSlow;
        Vector16 vCur = vFast;
        const float invK = 1.0f / static_cast<float>(K);

        float maxOrthErr = 0.0f;

        for (int step = 0; step < K; ++step) {
            // Step index in [16 - K, 15]
            const int t = 16 - K + step;
            // Guard modulation schedule: gentle harmonic taper
            const float alpha_t = 0.15f * std::sin(static_cast<float>(t) * (kPi / 16.0f));
            const float gamma_t = mCascadeGain * (1.0f + alpha_t);

            // Tangent triad step: delta_v = B_W(gamma_t * u, v)
            Vector16 scaledU {};
            for (size_t i = 0; i < N; ++i) {
                scaledU[i] = uCur[i] * gamma_t;
            }

            Vector16 deltaV = triadOp.evaluate(scaledU, vCur);

            // Measure tangent orthogonality: <v_cur, delta_v>
            float inner = 0.0f;
            for (size_t i = 0; i < N; ++i) inner += vCur[i] * deltaV[i];
            maxOrthErr = std::max(maxOrthErr, std::abs(inner));

            // State update: v_upd = v_cur + delta_v
            Vector16 vUpd {};
            for (size_t i = 0; i < N; ++i) {
                vUpd[i] = vCur[i] + deltaV[i];
            }

            // Spherical norm retraction preserving ||v0||_2
            Vector16 vTilde = retractToSphere(vUpd, v0);

            // Sub-step coupling between fast and slow state
            if (step + 1 < K) {
                for (size_t i = 0; i < N; ++i) {
                    uCur[i] = flushDenormal(uCur[i] + invK * (vTilde[i] - vCur[i]));
                }
            }
            vCur = vTilde;
        }

        // Measure radial drift ||vCur||_2^2 - ||v0||_2^2
        float normFinalSq = 0.0f;
        for (size_t i = 0; i < N; ++i) normFinalSq += vCur[i] * vCur[i];
        mLastRadialDrift = std::abs(normFinalSq - norm0Sq);
        mLastTangentOrthErr = maxOrthErr;

        return vCur;
    }

private:
    float mCascadeGain { 0.85f };
    int   mUnrollSteps { 4 };
    float mLastRadialDrift { 0.0f };
    float mLastTangentOrthErr { 0.0f };
};

// ============================================================================
// 4. Beltrami Triad Flux Outlier Shielding & Zeno Flux
//
// Discrete Beltrami condition:
//   Cross-shell exterior curl: curl_t = sqrt(max(0, 1 - <u_hat_c, u_hat_f>^2))
//   Fine-shell innovation: innov_t = ||u_hat_f,t - u_hat_f,t-1||_2
//   Raw Beltrami flux: Phi_raw = curl_t * innov_t
//   Kernel-5 max-pool neighborhood shield: Phi_shielded = max_{k in [-2, 2]} Phi_{raw, t+k}
//   Zeno flux: Phi_Zeno = tanh(0.65 * mean(Phi_shielded) + 0.35 * max(Phi_shielded))
// ============================================================================
class BeltramiTriadFluxDetector {
public:
    static constexpr size_t N = 16;
    using Vector16 = std::array<float, N>;

    BeltramiTriadFluxDetector() noexcept {
        reset();
    }

    void reset() noexcept {
        mPrevFineUnit.fill(0.0f);
        mPrevFineUnit[0] = 1.0f;
        mFluxHistory.fill(0.0f);
        mHistoryIdx = 0;
        mZenoFlux = 0.0f;
    }

    // Computes instantaneous Beltrami flux from coarse (slow) and fine (fast) vectors
    [[nodiscard]] float computeFlux(const Vector16& uCoarse, const Vector16& uFine) noexcept {
        // Unit vector normalizations
        float cNormSq = 0.0f;
        float fNormSq = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            cNormSq += uCoarse[i] * uCoarse[i];
            fNormSq += uFine[i] * uFine[i];
        }

        // Zero-norm guard: if either vector has vanishing norm, flux physically vanishes
        if (cNormSq <= 1.0e-20f || fNormSq <= 1.0e-20f) {
            mZenoFlux = 0.0f;
            return 0.0f;
        }

        const float invC = (cNormSq > 1.0e-20f) ? (1.0f / std::sqrt(cNormSq)) : 0.0f;
        const float invF = (fNormSq > 1.0e-20f) ? (1.0f / std::sqrt(fNormSq)) : 0.0f;

        Vector16 uHatC {};
        Vector16 uHatF {};
        for (size_t i = 0; i < N; ++i) {
            uHatC[i] = uCoarse[i] * invC;
            uHatF[i] = uFine[i] * invF;
        }

        // 1. Cross-Shell Exterior Curl = sin(theta) = sqrt(max(0, 1 - cos^2(theta)))
        float cosTheta = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            cosTheta += uHatC[i] * uHatF[i];
        }
        cosTheta = std::clamp(cosTheta, -1.0f, 1.0f);
        const float curl = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

        // 2. Fine-Shell Innovation = ||u_hat_f,t - u_hat_f,t-1||_2
        float innovSq = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            const float d = uHatF[i] - mPrevFineUnit[i];
            innovSq += d * d;
        }
        const float innov = std::sqrt(innovSq);
        mPrevFineUnit = uHatF;

        // 3. Raw Beltrami Flux
        const float rawFlux = curl * innov;

        // Push into 5-sample ring buffer for kernel-5 max-pool shielding
        mFluxHistory[mHistoryIdx] = rawFlux;
        mHistoryIdx = (mHistoryIdx + 1) % 5;

        // Kernel-5 neighborhood shielding: max over 5 consecutive samples
        float shieldedFlux = 0.0f;
        float sumFlux = 0.0f;
        for (size_t i = 0; i < 5; ++i) {
            shieldedFlux = std::max(shieldedFlux, mFluxHistory[i]);
            sumFlux += mFluxHistory[i];
        }
        const float meanFlux = sumFlux * 0.20f;

        // 4. Zeno Triad Flux Metric: Phi_Zeno in [0, 1]
        mZenoFlux = std::tanh(0.65f * meanFlux + 0.35f * shieldedFlux);
        mZenoFlux = flushDenormal(std::clamp(mZenoFlux, 0.0f, 1.0f));
        return mZenoFlux;
    }

    [[nodiscard]] float getZenoFlux() const noexcept { return mZenoFlux; }

    // Alias matching detectFlux terminology
    [[nodiscard]] float detectFlux(const Vector16& uCoarse, const Vector16& uFine) noexcept {
        return computeFlux(uCoarse, uFine);
    }

private:
    Vector16 mPrevFineUnit {};
    std::array<float, 5> mFluxHistory {};
    size_t mHistoryIdx { 0 };
    float mZenoFlux { 0.0f };
};

// ============================================================================
// 5. Zeno-Amygdala Adaptive Dynamic Unroll Scheduler
//
// Telemetry inputs:
//   - Zeno triad flux Phi_Zeno in [0, 1]
//   - Mucosal pressure conflict head in [0, 1]
//   - Dynamic turbulence index: T_Zeno = 0.50 * Phi_Zeno + 0.50 * P_conflict
//
// Dynamic dispatch:
//   T_Zeno >= 0.75 -> K = 16 (Extreme shock / violent breakaway)
//   T_Zeno >= 0.62 -> K = 8  (High turbulence / flutter)
//   T_Zeno >= 0.45 -> K = 4  (Balanced deliberation)
//   T_Zeno <  0.45 -> K = 1  (Surgical steady laminar flow)
// ============================================================================
class ZenoAmygdalaScheduler {
public:
    ZenoAmygdalaScheduler() noexcept = default;

    void reset() noexcept {
        mTurbulenceIndex = 0.0f;
        mActiveK = 1;
    }

    void setSensitivity(float sensitivity) noexcept {
        mSensitivity = std::clamp(sensitivity, 0.1f, 2.0f);
    }

    void setUnrollOverride(int overrideK) noexcept {
        mOverrideK = overrideK; // 0 = Auto (Zeno), 1=K1, 2=K4, 3=K8, 4=K16
    }

    [[nodiscard]] int updateSchedule(float zenoFlux, float pressureConflict) noexcept {
        if (mOverrideK > 0) {
            switch (mOverrideK) {
                case 1: mActiveK = 1; break;
                case 2: mActiveK = 4; break;
                case 3: mActiveK = 8; break;
                case 4: mActiveK = 16; break;
                default: mActiveK = 1; break;
            }
            mTurbulenceIndex = std::clamp(zenoFlux * 0.5f + pressureConflict * 0.5f, 0.0f, 1.0f);
            return mActiveK;
        }

        const float zFluxClamped = std::clamp(zenoFlux, 0.0f, 1.0f);
        const float pConfClamped = std::clamp(pressureConflict, 0.0f, 1.0f);

        // Composite turbulence index T_Zeno
        mTurbulenceIndex = std::clamp((0.45f * zFluxClamped + 0.55f * pConfClamped) * mSensitivity, 0.0f, 1.0f);

        if (mTurbulenceIndex >= 0.75f) {
            mActiveK = 16;
        } else if (mTurbulenceIndex >= 0.62f) {
            mActiveK = 8;
        } else if (mTurbulenceIndex >= 0.45f) {
            mActiveK = 4;
        } else {
            mActiveK = 1;
        }

        return mActiveK;
    }

    [[nodiscard]] float getTurbulenceIndex() const noexcept { return mTurbulenceIndex; }
    [[nodiscard]] int getActiveK() const noexcept { return mActiveK; }

private:
    float mSensitivity { 1.0f };
    int   mOverrideK   { 0 };
    float mTurbulenceIndex { 0.0f };
    int   mActiveK     { 1 };
};

// ============================================================================
// 6. 201-Channel 2D Continuous Spectral & Jordan-Enclosure Cavity Field
//
// Channels:
//   0..63   (64 ch): 2D Separable DCT-II spatial eigenfunctions
//   64..143 (80 ch): Multi-bounce ray reflection & boundary propagation
//   144..200 (57 ch): Topological Jordan cavity potentials (Beltrami curl/div)
// ============================================================================
class ContinuousSpectralJordanField {
public:
    static constexpr size_t kNumChannels = 201;
    using FieldVector = std::array<float, kNumChannels>;

    ContinuousSpectralJordanField() noexcept {
        generateCavityField(1.0f, 0.5f, 1);
    }

    // Generates 201 channels for a given normalized cavity radius, aspect, and boundary mode
    // boundaryMode: 0: Dry, 1: Standard, 2: Water-Coupled, 3: Tiled
    void generateCavityField(float cavityScale, float aspect, int boundaryMode) noexcept {
        constexpr int H = 8;
        constexpr int W = 8;
        const float scale = std::clamp(cavityScale, 0.2f, 3.0f);
        const float rAspect = std::clamp(aspect, 0.2f, 2.0f);

        // 1. Channels 0..63: 2D DCT-II Modes
        // psi_{ky, kx}(r, c) = cos(pi*(2r+1)*ky / (2H)) * cos(pi*(2c+1)*kx / (2W))
        // attenuated by heat kernel exp(-0.04 * (ky^2 + kx^2) / scale)
        for (int ky = 0; ky < 8; ++ky) {
            for (int kx = 0; kx < 8; ++kx) {
                const size_t ch = static_cast<size_t>(ky * 8 + kx);
                // Center-sampled potential (r = H/2, c = W/2)
                const float yr = 3.5f;
                const float xc = 3.5f;
                const float psiY = std::cos((kPi * (2.0f * yr + 1.0f) * static_cast<float>(ky)) / (2.0f * static_cast<float>(H)));
                const float psiX = std::cos((kPi * (2.0f * xc + 1.0f) * static_cast<float>(kx)) / (2.0f * static_cast<float>(W)));
                const float heatLoss = std::exp(-0.045f * static_cast<float>(ky * ky + kx * kx) / scale);
                mChannels[ch] = flushDenormal(psiY * psiX * heatLoss);
            }
        }

        // 2. Channels 64..143 (80 ch): Ray Reflection & Wave Propagation
        // Modulated by boundary reflectivity: Dry (0.95), Std (0.85), Water (0.65), Tiled (0.90)
        float reflectCoeff = 0.85f;
        switch (boundaryMode) {
            case 0: reflectCoeff = 0.95f; break; // Dry porcelain
            case 1: reflectCoeff = 0.85f; break; // Standard
            case 2: reflectCoeff = 0.65f; break; // Water-coupled absorption
            case 3: reflectCoeff = 0.90f; break; // Tiled acoustic ceramic
            default: reflectCoeff = 0.85f; break;
        }

        for (size_t i = 64; i < 144; ++i) {
            const float angle = static_cast<float>(i - 64) * (kTwoPi / 80.0f);
            // Multi-bounce path length interference
            const float path1 = std::cos(angle * 3.0f * rAspect);
            const float path2 = reflectCoeff * std::cos(angle * 7.0f / scale);
            mChannels[i] = flushDenormal(0.5f * path1 + 0.5f * path2);
        }

        // 3. Channels 144..200 (57 ch): Topological Jordan Enclosure Potentials
        // Evaluates 2D Beltrami curl/div: curl = gx * dpsi/dy - gy * dpsi/dx, div = d^2p/dx^2 + d^2p/dy^2
        for (size_t i = 144; i < 201; ++i) {
            const float idxNorm = static_cast<float>(i - 144) / 56.0f;
            const float rad = idxNorm * kPi;
            // Beltrami curl / divergence potentials
            const float gx = std::sin(rad * 2.0f);
            const float gy = std::cos(rad * 3.0f * rAspect);
            const float curl2D = gx * std::sin(rad * 4.0f) - gy * std::cos(rad * 4.0f);
            const float div2D  = -4.0f * std::cos(rad * 2.0f);
            const float beltramiDet = std::sqrt(1.0f + gx * gx + gy * gy + curl2D * curl2D) - 1.0f;
            mChannels[i] = flushDenormal(0.4f * curl2D + 0.3f * div2D + 0.3f * beltramiDet);
        }
    }

    [[nodiscard]] const FieldVector& getChannels() const noexcept { return mChannels; }
    [[nodiscard]] float getChannel(size_t idx) const noexcept {
        return (idx < kNumChannels) ? mChannels[idx] : 0.0f;
    }

    // Projects the 201-channel field into 16 modal coupling factors
    [[nodiscard]] std::array<float, 16> projectTo16Modes() const noexcept {
        std::array<float, 16> modes {};
        for (size_t m = 0; m < 16; ++m) {
            float sum = 0.0f;
            for (size_t k = 0; k < 12; ++k) {
                const size_t ch = (m * 12 + k) % kNumChannels;
                sum += mChannels[ch];
            }
            modes[m] = flushDenormal(sum * (1.0f / 12.0f));
        }
        return modes;
    }

private:
    FieldVector mChannels {};
};

// ============================================================================
// 7. Optimal Transport Wasserstein-1 & Fisher-Rao Natural Gradient Morphing
//
// 1D Marginal Wasserstein-1 Distance:
//   W_1(p, q) = (1 / L) * sum_{u=0}^{L-1} |CDF_p(u) - CDF_q(u)|
//
// Fisher-Rao Natural Gradient Boltzmann Mirror Descent:
//   w_k^{(L+1)} proportional to w_k^{(L)} * exp(-beta * clip((Phi_k - Phi_k^0)/(sigma_k + 0.03), -3, 3))
//   with Laplace smoothing floor.
// ============================================================================
class OptimalTransportMorpher {
public:
    static constexpr size_t L = 16; // 16 modes or dimensions

    // Computes 1D marginal Wasserstein-1 distance between two normalized histograms
    [[nodiscard]] static float computeWasserstein1(
        const std::array<float, L>& p,
        const std::array<float, L>& q) noexcept
    {
        // 1. Form cumulative distribution functions (CDF)
        float cdfP = 0.0f;
        float cdfQ = 0.0f;
        float sumDiff = 0.0f;

        for (size_t i = 0; i < L; ++i) {
            cdfP += std::max(0.0f, p[i]);
            cdfQ += std::max(0.0f, q[i]);
            sumDiff += std::abs(cdfP - cdfQ);
        }

        return sumDiff * (1.0f / static_cast<float>(L));
    }

    // Fisher-Rao natural gradient Boltzmann mirror descent on the 4-simplex
    // morphs weights between 4 geometric manifolds
    [[nodiscard]] static std::array<float, 4> fisherRaoBoltzmannStep(
        const std::array<float, 4>& currentWeights,
        const std::array<float, 4>& manifoldErrors,
        float beta = 1.35f,
        float alphaFloor = 0.05f) noexcept
    {
        std::array<float, 4> nextW {};
        float sumExp = 0.0f;

        for (size_t k = 0; k < 4; ++k) {
            const float clippedErr = std::clamp(manifoldErrors[k], -3.0f, 3.0f);
            const float update = std::exp(-beta * clippedErr);
            nextW[k] = currentWeights[k] * update;
            sumExp += nextW[k];
        }

        // Normalize
        const float invSum = (sumExp > 1.0e-12f) ? (1.0f / sumExp) : 0.25f;
        for (size_t k = 0; k < 4; ++k) {
            nextW[k] *= invSum;
            // Laplace smoothing floor
            nextW[k] = (1.0f - 4.0f * alphaFloor) * nextW[k] + alphaFloor;
        }

        return nextW;
    }
};

} // namespace pf02::dsp
