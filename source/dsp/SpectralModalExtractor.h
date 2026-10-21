#pragma once

#include "DspMath.h"
#include "ModalResonatorMatrix.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <complex>

namespace pf02::dsp {

struct ExtractedModalParameters {
    float fundamentalHz { 55.0f };
    GeometricManifold bestManifold { GeometricManifold::GlottalColonic };
    float manifoldConfidence { 0.85f };
    DampingProfile estimatedDamping { DampingProfile::MucosalTissue };
    std::array<float, 16> modalFrequencies {};
    std::array<float, 16> modalQ {};
    std::array<float, 16> modalAmplitudes {};
    float estimatedTriadCoupling { 0.85f };
    float estimatedZenoFlux { 0.35f };
};

/**
 * SpectralModalExtractor: High-precision modal frequency and Beltrami triad extractor.
 *
 * Implements:
 * 1. 4096-point Radix-2 FFT with 4-term Blackman-Harris windowing.
 * 2. Sub-bin parabolic log-magnitude peak picking (f_peak = (k + delta) * fs / N).
 * 3. Multi-slice temporal exponential decay regression for modal Q estimation.
 * 4. Manifold MSE geometric classification across all 4 theoretical tables.
 * 5. DarkSnail Beltrami triad coupling and Zeno flux estimation.
 * 6. 100% zero heap allocations on stack/preallocated buffers.
 */
class SpectralModalExtractor {
public:
    static constexpr size_t kFftSize = 4096;
    static constexpr size_t kHalfFft = kFftSize / 2;
    static constexpr size_t kNumModes = 16;

    SpectralModalExtractor() noexcept;

    void prepare(double sampleRate) noexcept;

    // Analyzes a test burst or audio recording buffer and extracts modal parameters
    [[nodiscard]] ExtractedModalParameters analyzeBuffer(
        const float* audioBuffer,
        size_t numSamples) noexcept;

    // Generates a synthetic test burst for self-testing / calibration
    void generateTestBurst(
        float* outBuffer,
        size_t numSamples,
        float fundamentalHz,
        GeometricManifold manifold) noexcept;

private:
    void initBlackmanHarrisWindow() noexcept;
    void runRadix2Fft(std::array<std::complex<float>, kFftSize>& data) noexcept;

    float mSampleRate { 48000.0f };
    std::array<float, kFftSize> mWindow {};

    // Static buffers for FFT processing (zero dynamic allocation)
    std::array<std::complex<float>, kFftSize> mFftBuffer {};
    std::array<float, kHalfFft> mMagSpectrum {};
    std::array<float, kHalfFft> mLogMagSpectrum {};
};

} // namespace pf02::dsp
