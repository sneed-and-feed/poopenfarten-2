#pragma once

#include "DspMath.h"
#include "DarkSnailMath.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

enum class GeometricManifold : int {
    SigmoidHorn   = 0, // Poincaré negative-curvature visceral tract
    ChladniRing   = 1, // Biharmonic 2D annular sphincter plate
    GlottalColonic= 2, // Coupled vocal/visceral formant squelch cascade
    PorcelainBowl = 3  // 2D Jordan-enclosure cavity standing waves
};

enum class DampingProfile : int {
    MucosalTissue = 0, // Steep damping, heavy viscous loss
    Porcelain     = 1, // High-Q, sustained crystalline upper overtones
    WetElastomer  = 2, // Nonlinear viscoelastic squelch
    BellBronze    = 3, // Resonant metallic harmonic bloom
    StiffCartilage= 4  // Crisp percussive slapdown
};

struct ModalMatrixParams {
    float fundamentalHz       { 55.0f };   // Base resonance pitch [7.0, 420.0] Hz
    GeometricManifold manifold{ GeometricManifold::GlottalColonic };
    float manifoldMorph       { 0.0f };    // Continuous geometric morph [0.0, 1.0]
    DampingProfile damping    { DampingProfile::MucosalTissue };
    float triadCouplingGain   { 0.85f };   // Skew-symmetric Lie triad gain [0.0, 2.0]
    float so16SkewGenerator   { 0.50f };   // so(16) Lie generator morph [0.0, 1.0]
    bool  sphericalRetraction { true };    // Exact S^15 spherical norm retraction toggle
    float householderCoupling { 0.45f };   // O(N) Householder scattering depth [0.0, 1.0]
    float porcelainCavityMix  { 35.0f };   // 2D Jordan cavity resonance balance [0.0, 100.0] %
    int   porcelainBoundaryMode{ 1 };      // 0: Dry, 1: Std, 2: Water, 3: Tiled
    float stereoSpread        { 75.0f };   // Golden-Ratio stereo panning spread [0.0, 100.0] %
    int   zenoUnrollK         { 4 };       // Active unroll depth K in {1, 4, 8, 16}
};

/**
 * ModalResonatorMatrix: 16-Pole Physical Acoustic Modal Resonator Bank.
 *
 * Implements:
 * 1. 16 parallel Zavalishin Topology-Preserving Transform (TPT) State Variable Filters (SVF)
 *    with non-linear acoustic radiation resistance damping (F_rad ~ v^2).
 * 2. 4 Geometric Manifolds with continuous interpolation.
 * 3. 5 Viscoelastic Damping Profiles.
 * 4. Fast O(N) Orthogonal Householder scattering matrix with prime delay taps (13.7 ms, 19.3 ms).
 * 5. DarkSnail Skew-Symmetric Bilinear Triad Operator & SlowFastTriadCascade in so(16)
 *    with exact S^15 spherical norm retraction (zero radial drift < 1e-6).
 * 6. Continuous 201-channel 2D Jordan-enclosure cavity field projection.
 * 7. Golden-Ratio stereo modal panning (Phi = 137.507764 deg).
 */
class ModalResonatorMatrix {
public:
    static constexpr size_t kNumModes = 16;
    using ModalArray = std::array<float, kNumModes>;

    // Manifold frequency ratios calibrated to physical boundary acoustics
    static inline constexpr ModalArray kSigmoidHornRatios = {
        1.0000f, 1.1926f, 1.4866f, 1.8493f,
        2.2561f, 2.6926f, 3.1496f, 3.6210f,
        4.1024f, 4.5913f, 5.0858f, 5.5846f,
        6.0867f, 6.5914f, 7.0980f, 7.6062f
    };

    static inline constexpr ModalArray kChladniRingRatios = {
        1.0000f, 1.5882f, 2.0588f, 2.7647f,
        3.1765f, 4.0588f, 4.4118f, 5.3529f,
        5.8824f, 6.7647f, 7.4118f, 8.5294f,
        9.3529f, 10.2941f, 11.6471f, 13.0000f
    };

    static inline constexpr ModalArray kGlottalColonicRatios = {
        1.0000f, 1.4500f, 2.1000f, 2.8500f,
        3.7500f, 4.6000f, 5.5500f, 6.6000f,
        7.7500f, 9.0000f, 10.3500f, 11.8000f,
        13.4000f, 15.1000f, 17.0000f, 19.1000f
    };

    static inline constexpr ModalArray kPorcelainBowlRatios = {
        1.0000f, 1.3416f, 1.7321f, 2.2361f,
        2.6458f, 3.1623f, 3.6056f, 4.1231f,
        4.5826f, 5.0990f, 5.5678f, 6.0828f,
        6.5574f, 7.0711f, 7.5498f, 8.0623f
    };

    ModalResonatorMatrix() noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Process a single audio sample:
    // takes wideband aeroacoustic exciter input and generates stereo panned output (outL, outR)
    void processSample(
        float exciterInput,
        const ModalMatrixParams& params,
        float& outL,
        float& outR) noexcept;

    // Modulation inputs from 3D RK4 Lorenz Attractor
    void setLorenzModulation(
        const ModalArray& freqMultipliers,
        float qSpreadFactor,
        float panOffsetRad) noexcept;

    // Telemetry getters
    [[nodiscard]] const ModalArray& getModalEnergies() const noexcept { return mModalEnergies; }
    [[nodiscard]] const ModalArray& getModalFrequencies() const noexcept { return mCurrentFrequencies; }
    [[nodiscard]] const ModalArray& getTriadEnergyFlow() const noexcept { return mTriadEnergyFlow; }
    [[nodiscard]] float getLastRadialDrift() const noexcept { return mTriadCascade.getLastRadialDrift(); }
    [[nodiscard]] float getLastTangentOrthError() const noexcept { return mTriadCascade.getLastTangentOrthError(); }

private:
    void updateFilterCoefficients(const ModalMatrixParams& params) noexcept;
    void computeBaseRatios(GeometricManifold manifold, float morph, ModalArray& outRatios) const noexcept;
    void getDampingQ(DampingProfile profile, ModalArray& outQ) const noexcept;

    float mSampleRate { 48000.0f };

    // 1. 16-Pole Zavalishin TPT SVF Bandpass Matrix State
    ModalArray mG {};      // Discrete frequency warp g = tan(pi * f / fs)
    ModalArray mK {};      // Damping k = 1 / Q
    ModalArray mA1 {};     // Resolvent denominator 1 / (1 + g*(g + k))
    ModalArray mS1 {};     // Integrator 1 state
    ModalArray mS2 {};     // Integrator 2 state
    ModalArray mQNorm {};  // Energy normalization factor
    ModalArray mCurrentFrequencies {};
    ModalArray mModalEnergies {};

    // 2. DarkSnail Skew-Symmetric Triad & SlowFastTriadCascade
    SkewSymmetricTriadOperator<kNumModes, kNumModes> mTriadOp;
    SlowFastTriadCascade mTriadCascade;
    ModalArray mSlowState {};
    ModalArray mTriadEnergyFlow {};

    // 3. 2D Continuous Spectral Jordan Cavity Field
    ContinuousSpectralJordanField mJordanField;

    // 4. Acyclic Prime Delay Lines for Householder Tail Diffusion
    // D1 = 13.7 ms (~658 samples at 48k), D2 = 19.3 ms (~926 samples at 48k)
    static constexpr size_t kDelay1Cap = 2048;
    static constexpr size_t kDelay2Cap = 2048;
    FixedDelayLine<kDelay1Cap> mDelay1;
    FixedDelayLine<kDelay2Cap> mDelay2;
    OnePoleLowpass mDiffuserFilter1;
    OnePoleLowpass mDiffuserFilter2;
    float mDelay1Samples { 658.0f };
    float mDelay2Samples { 926.0f };

    // Lorenz modulation cache
    ModalArray mLorenzFreqMult { 1.0f };
    float mLorenzQSpread { 1.0f };
    float mLorenzPanOffset { 0.0f };

    // Golden Ratio Stereo Panning Coefficients (L/R gains)
    std::array<float, kNumModes> mPanL {};
    std::array<float, kNumModes> mPanR {};
};

} // namespace pf02::dsp
