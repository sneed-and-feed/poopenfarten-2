#include "SpectralModalExtractor.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace pf02::dsp {

SpectralModalExtractor::SpectralModalExtractor() noexcept {
    initBlackmanHarrisWindow();
}

void SpectralModalExtractor::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    initBlackmanHarrisWindow();
}

void SpectralModalExtractor::initBlackmanHarrisWindow() noexcept {
    constexpr float a0 = 0.35875f;
    constexpr float a1 = 0.48829f;
    constexpr float a2 = 0.14128f;
    constexpr float a3 = 0.01168f;

    for (size_t i = 0; i < kFftSize; ++i) {
        const float phi = (kTwoPi * static_cast<float>(i)) / static_cast<float>(kFftSize - 1);
        mWindow[i] = a0 - a1 * std::cos(phi) + a2 * std::cos(2.0f * phi) - a3 * std::cos(3.0f * phi);
    }
}

void SpectralModalExtractor::runRadix2Fft(std::array<std::complex<float>, kFftSize>& data) noexcept {
    // 1. Bit-reversal permutation
    size_t j = 0;
    for (size_t i = 0; i < kFftSize - 1; ++i) {
        if (i < j) {
            std::swap(data[i], data[j]);
        }
        size_t k = kFftSize >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }

    // 2. Cooley-Tukey Radix-2 butterflies
    for (size_t len = 2; len <= kFftSize; len <<= 1) {
        const float angle = -kTwoPi / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < kFftSize; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            const size_t halfLen = len >> 1;
            for (size_t step = 0; step < halfLen; ++step) {
                const std::complex<float> u = data[i + step];
                const std::complex<float> v = data[i + step + halfLen] * w;
                data[i + step] = u + v;
                data[i + step + halfLen] = u - v;
                w *= wlen;
            }
        }
    }
}

ExtractedModalParameters SpectralModalExtractor::analyzeBuffer(
    const float* audioBuffer,
    size_t numSamples) noexcept
{
    ExtractedModalParameters result {};

    if (audioBuffer == nullptr || numSamples < 64) {
        return result;
    }

    // 1. Window audio data into FFT buffer
    const size_t copyCount = std::min(numSamples, kFftSize);
    for (size_t i = 0; i < copyCount; ++i) {
        mFftBuffer[i] = std::complex<float>(audioBuffer[i] * mWindow[i], 0.0f);
    }
    for (size_t i = copyCount; i < kFftSize; ++i) {
        mFftBuffer[i] = std::complex<float>(0.0f, 0.0f);
    }

    // 2. Execute Radix-2 FFT
    runRadix2Fft(mFftBuffer);

    // 3. Compute magnitude spectrum
    float maxMag = 0.0f;
    for (size_t k = 0; k < kHalfFft; ++k) {
        const float mag = std::abs(mFftBuffer[k]);
        mMagSpectrum[k] = mag;
        mLogMagSpectrum[k] = std::log(std::max(1.0e-12f, mag));
        maxMag = std::max(maxMag, mag);
    }

    // 4. Peak picking with parabolic sub-bin interpolation
    struct Peak {
        float frequencyHz { 0.0f };
        float amplitude   { 0.0f };
        size_t bin        { 0 };
    };

    std::array<Peak, 64> detectedPeaks {};
    size_t numPeaks = 0;
    const float noiseFloor = maxMag * 0.005f; // -46 dB threshold

    for (size_t k = 2; k < kHalfFft - 2 && numPeaks < 64; ++k) {
        if (mMagSpectrum[k] > mMagSpectrum[k - 1] &&
            mMagSpectrum[k] > mMagSpectrum[k + 1] &&
            mMagSpectrum[k] > noiseFloor)
        {
            // Parabolic log-magnitude interpolation
            const float alpha = mLogMagSpectrum[k - 1];
            const float beta  = mLogMagSpectrum[k];
            const float gamma = mLogMagSpectrum[k + 1];
            const float denom = alpha - 2.0f * beta + gamma;

            float delta = 0.0f;
            if (std::abs(denom) > 1.0e-12f) {
                delta = 0.5f * (alpha - gamma) / denom;
                delta = std::clamp(delta, -0.5f, 0.5f);
            }

            const float peakFreq = (static_cast<float>(k) + delta) * (mSampleRate / static_cast<float>(kFftSize));
            const float peakAmp = std::exp(beta - 0.25f * (alpha - gamma) * delta);

            detectedPeaks[numPeaks++] = { peakFreq, peakAmp, k };
        }
    }

    // 5. Sort peaks by frequency
    std::sort(detectedPeaks.begin(), detectedPeaks.begin() + numPeaks, [](const Peak& a, const Peak& b) {
        return a.frequencyHz < b.frequencyHz;
    });

    // Extract fundamental frequency
    float f0 = 55.0f;
    if (numPeaks > 0 && detectedPeaks[0].frequencyHz >= 10.0f) {
        f0 = detectedPeaks[0].frequencyHz;
    }
    result.fundamentalHz = f0;

    // Fill 16 modal frequencies & amplitudes
    for (size_t i = 0; i < kNumModes; ++i) {
        if (i < numPeaks) {
            result.modalFrequencies[i] = detectedPeaks[i].frequencyHz;
            result.modalAmplitudes[i]  = detectedPeaks[i].amplitude / std::max(1.0e-6f, maxMag);
        } else {
            // Extrapolate remaining modes geometrically
            const float ratio = ModalResonatorMatrix::kGlottalColonicRatios[i];
            result.modalFrequencies[i] = std::min(f0 * ratio, mSampleRate * 0.48f);
            result.modalAmplitudes[i]  = 0.05f;
        }
        // Nominal Q based on frequency and modal index
        result.modalQ[i] = std::clamp(30.0f / (1.0f + 0.08f * static_cast<float>(i)), 5.0f, 200.0f);
    }

    // 6. Geometric Manifold Classification via MSE on log2 ratios
    std::array<float, 4> manifoldMse {};
    const std::array<const ModalResonatorMatrix::ModalArray*, 4> manifoldTables = {
        &ModalResonatorMatrix::kSigmoidHornRatios,
        &ModalResonatorMatrix::kChladniRingRatios,
        &ModalResonatorMatrix::kGlottalColonicRatios,
        &ModalResonatorMatrix::kPorcelainBowlRatios
    };

    for (size_t m = 0; m < 4; ++m) {
        float sumSqDiff = 0.0f;
        const auto& table = *manifoldTables[m];
        for (size_t i = 0; i < kNumModes; ++i) {
            const float measuredRatio = result.modalFrequencies[i] / std::max(1.0f, f0);
            const float targetRatio = table[i];
            const float diff = std::log2(std::max(0.1f, measuredRatio)) - std::log2(targetRatio);
            sumSqDiff += diff * diff;
        }
        manifoldMse[m] = sumSqDiff * (1.0f / static_cast<float>(kNumModes));
    }

    // Pick minimum MSE manifold
    size_t bestM = 0;
    float minMse = 1.0e10f;
    for (size_t m = 0; m < 4; ++m) {
        if (manifoldMse[m] < minMse) {
            minMse = manifoldMse[m];
            bestM = m;
        }
    }

    result.bestManifold = static_cast<GeometricManifold>(bestM);

    // Softmax confidence with calibrated temperature tau = 0.08
    constexpr float tau = 0.08f;
    float sumExp = 0.0f;
    for (size_t m = 0; m < 4; ++m) {
        sumExp += std::exp(-manifoldMse[m] / tau);
    }
    result.manifoldConfidence = (sumExp > 1.0e-6f) ? (std::exp(-minMse / tau) / sumExp) : 0.75f;

    // 7. DarkSnail Beltrami & Triad Estimation
    // Cross-spectral exterior curl across even vs odd partials
    float evenSum = 0.0f;
    float oddSum = 0.0f;
    for (size_t i = 0; i < kNumModes; ++i) {
        if (i % 2 == 0) evenSum += result.modalAmplitudes[i];
        else oddSum += result.modalAmplitudes[i];
    }
    const float tot = evenSum + oddSum + 1.0e-6f;
    const float asym = std::abs(evenSum - oddSum) / tot;

    result.estimatedTriadCoupling = std::clamp(0.5f + 1.2f * asym, 0.1f, 1.8f);
    result.estimatedZenoFlux = std::clamp(0.2f + 0.8f * asym, 0.0f, 1.0f);

    return result;
}

void SpectralModalExtractor::generateTestBurst(
    float* outBuffer,
    size_t numSamples,
    float fundamentalHz,
    GeometricManifold manifold) noexcept
{
    if (outBuffer == nullptr || numSamples == 0) return;

    ModalResonatorMatrix::ModalArray ratios {};
    switch (manifold) {
        case GeometricManifold::SigmoidHorn: ratios = ModalResonatorMatrix::kSigmoidHornRatios; break;
        case GeometricManifold::ChladniRing: ratios = ModalResonatorMatrix::kChladniRingRatios; break;
        case GeometricManifold::GlottalColonic: ratios = ModalResonatorMatrix::kGlottalColonicRatios; break;
        case GeometricManifold::PorcelainBowl: ratios = ModalResonatorMatrix::kPorcelainBowlRatios; break;
    }

    const float f0 = std::clamp(fundamentalHz, 15.0f, 400.0f);
    for (size_t n = 0; n < numSamples; ++n) {
        const float t = static_cast<float>(n) / mSampleRate;
        float sample = 0.0f;
        for (size_t i = 0; i < kNumModes; ++i) {
            const float f = f0 * ratios[i];
            if (f > mSampleRate * 0.48f) continue;
            const float decay = std::exp(-t * (10.0f + static_cast<float>(i) * 5.0f));
            const float amp = (1.0f / (1.0f + 0.3f * static_cast<float>(i))) * decay;
            sample += amp * std::sin(kTwoPi * f * t);
        }
        outBuffer[n] = sample * 0.20f;
    }
}

} // namespace pf02::dsp
