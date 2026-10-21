/**
 * @file modal_extractor.js
 * @brief Client-Side Spectral Modal Extractor for Poopenfarten II (PF-02)
 *
 * Implements:
 *  1. 4096-Point Radix-2 FFT with 4-term Blackman-Harris windowing.
 *  2. Sub-bin parabolic log-magnitude peak picking (f_peak = (k + delta) * fs / N).
 *  3. Geometric manifold MSE classification across 4 theoretical models (Sigmoid, Chladni, Glottal, Porcelain).
 *  4. DarkSnail Beltrami triad coupling and Zeno flux estimation from spectral asymmetry.
 *  5. Lossless WAV binary parser (RIFF PCM) for user-dropped audio files.
 *  6. Procedural synthetic test burst generator for offline verification.
 *
 * ZERO static audio sample arrays! ZERO Base64 PCM strings!
 */

import { kManifoldRatios, kPi, kTwoPi, clamp } from './pf02_dsp_engine.js';

export class SpectralModalExtractor {
  static kFftSize = 4096;
  static kHalfFft = 2048;
  static kNumModes = 16;

  constructor(sampleRate = 48000.0) {
    this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    this.window = new Float32Array(SpectralModalExtractor.kFftSize);
    this.initBlackmanHarrisWindow();

    this.real = new Float32Array(SpectralModalExtractor.kFftSize);
    this.imag = new Float32Array(SpectralModalExtractor.kFftSize);
    this.magSpectrum = new Float32Array(SpectralModalExtractor.kHalfFft);
    this.logMagSpectrum = new Float32Array(SpectralModalExtractor.kHalfFft);
  }

  prepare(sampleRate) {
    this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
  }

  initBlackmanHarrisWindow() {
    const a0 = 0.35875;
    const a1 = 0.48829;
    const a2 = 0.14128;
    const a3 = 0.01168;
    const N = SpectralModalExtractor.kFftSize;

    for (let i = 0; i < N; ++i) {
      const phi = (kTwoPi * i) / (N - 1);
      this.window[i] = a0 - a1 * Math.cos(phi) + a2 * Math.cos(2.0 * phi) - a3 * Math.cos(3.0 * phi);
    }
  }

  runRadix2Fft(real, imag) {
    const n = SpectralModalExtractor.kFftSize;

    // 1. Bit-reversal permutation
    let j = 0;
    for (let i = 0; i < n - 1; ++i) {
      if (i < j) {
        const tempR = real[i];
        real[i] = real[j];
        real[j] = tempR;

        const tempI = imag[i];
        imag[i] = imag[j];
        imag[j] = tempI;
      }
      let k = n >> 1;
      while (k <= j) {
        j -= k;
        k >>= 1;
      }
      j += k;
    }

    // 2. Cooley-Tukey Radix-2 butterflies
    for (let len = 2; len <= n; len <<= 1) {
      const halfLen = len >> 1;
      const angle = -kTwoPi / len;
      const wStepR = Math.cos(angle);
      const wStepI = Math.sin(angle);

      for (let i = 0; i < n; i += len) {
        let wR = 1.0;
        let wI = 0.0;
        for (let step = 0; step < halfLen; ++step) {
          const idxA = i + step;
          const idxB = i + step + halfLen;

          const uR = real[idxA];
          const uI = imag[idxA];
          const vR = real[idxB] * wR - imag[idxB] * wI;
          const vI = real[idxB] * wI + imag[idxB] * wR;

          real[idxA] = uR + vR;
          imag[idxA] = uI + vI;
          real[idxB] = uR - vR;
          imag[idxB] = uI - vI;

          const nextWR = wR * wStepR - wI * wStepI;
          const nextWI = wR * wStepI + wI * wStepR;
          wR = nextWR;
          wI = nextWI;
        }
      }
    }
  }

  analyzeBuffer(audioBuffer, sampleRate = null) {
    const fs = sampleRate || this.sampleRate;
    const n = SpectralModalExtractor.kFftSize;
    const halfN = SpectralModalExtractor.kHalfFft;

    const copyLen = Math.min(audioBuffer.length, n);
    for (let i = 0; i < copyLen; ++i) {
      this.real[i] = audioBuffer[i] * this.window[i];
      this.imag[i] = 0.0;
    }
    for (let i = copyLen; i < n; ++i) {
      this.real[i] = 0.0;
      this.imag[i] = 0.0;
    }

    this.runRadix2Fft(this.real, this.imag);

    let maxMag = 0.0;
    for (let k = 0; k < halfN; ++k) {
      const mag = Math.sqrt(this.real[k] * this.real[k] + this.imag[k] * this.imag[k]);
      this.magSpectrum[k] = mag;
      this.logMagSpectrum[k] = Math.log(Math.max(1.0e-12, mag));
      if (mag > maxMag) maxMag = mag;
    }

    // Parabolic log-magnitude peak picking
    const detectedPeaks = [];
    const noiseFloor = maxMag * 0.005; // -46 dB

    for (let k = 2; k < halfN - 2 && detectedPeaks.length < 64; ++k) {
      if (this.magSpectrum[k] > this.magSpectrum[k - 1] &&
          this.magSpectrum[k] > this.magSpectrum[k + 1] &&
          this.magSpectrum[k] > noiseFloor) {

        const alpha = this.logMagSpectrum[k - 1];
        const beta  = this.logMagSpectrum[k];
        const gamma = this.logMagSpectrum[k + 1];
        const denom = alpha - 2.0 * beta + gamma;

        let delta = 0.0;
        if (Math.abs(denom) > 1.0e-12) {
          delta = 0.5 * (alpha - gamma) / denom;
          delta = clamp(delta, -0.5, 0.5);
        }

        const peakFreq = (k + delta) * (fs / n);
        const peakAmp = Math.exp(beta - 0.25 * (alpha - gamma) * delta);
        detectedPeaks.push({ frequencyHz: peakFreq, amplitude: peakAmp });
      }
    }

    detectedPeaks.sort((a, b) => a.frequencyHz - b.frequencyHz);

    let f0 = 55.0;
    if (detectedPeaks.length > 0 && detectedPeaks[0].frequencyHz >= 10.0) {
      f0 = detectedPeaks[0].frequencyHz;
    }

    const modalFrequencies = new Float32Array(16);
    const modalAmplitudes = new Float32Array(16);
    const modalQ = new Float32Array(16);

    for (let i = 0; i < 16; ++i) {
      if (i < detectedPeaks.length) {
        modalFrequencies[i] = detectedPeaks[i].frequencyHz;
        modalAmplitudes[i] = detectedPeaks[i].amplitude / Math.max(1.0e-6, maxMag);
      } else {
        const ratio = kManifoldRatios.GlottalColonic[i];
        modalFrequencies[i] = Math.min(f0 * ratio, fs * 0.48);
        modalAmplitudes[i] = 0.05;
      }
      modalQ[i] = clamp(30.0 / (1.0 + 0.08 * i), 5.0, 200.0);
    }

    // Geometric Manifold Classification via MSE on log2 ratios
    const manifoldKeys = ['SigmoidHorn', 'ChladniRing', 'GlottalColonic', 'PorcelainBowl'];
    const manifoldMse = [0, 0, 0, 0];

    for (let m = 0; m < 4; ++m) {
      const table = kManifoldRatios[manifoldKeys[m]];
      let sumSqDiff = 0.0;
      for (let i = 0; i < 16; ++i) {
        const measuredRatio = modalFrequencies[i] / Math.max(1.0, f0);
        const targetRatio = table[i];
        const diff = Math.log2(Math.max(0.1, measuredRatio)) - Math.log2(targetRatio);
        sumSqDiff += diff * diff;
      }
      manifoldMse[m] = sumSqDiff * (1.0 / 16.0);
    }

    let bestM = 0;
    let minMse = Infinity;
    for (let m = 0; m < 4; ++m) {
      if (manifoldMse[m] < minMse) {
        minMse = manifoldMse[m];
        bestM = m;
      }
    }

    const tau = 0.08;
    let sumExp = 0.0;
    for (let m = 0; m < 4; ++m) {
      sumExp += Math.exp(-manifoldMse[m] / tau);
    }
    const confidence = sumExp > 1.0e-6 ? (Math.exp(-minMse / tau) / sumExp) : 0.75;

    // DarkSnail Beltrami & Triad Estimation
    let evenSum = 0.0;
    let oddSum = 0.0;
    for (let i = 0; i < 16; ++i) {
      if (i % 2 === 0) evenSum += modalAmplitudes[i];
      else oddSum += modalAmplitudes[i];
    }
    const asym = Math.abs(evenSum - oddSum) / (evenSum + oddSum + 1.0e-6);

    return {
      fundamentalHz: f0,
      bestManifold: manifoldKeys[bestM],
      manifoldIndex: bestM,
      manifoldConfidence: confidence,
      estimatedDamping: bestM === 3 ? 'Porcelain' : 'MucosalTissue',
      modalFrequencies: modalFrequencies,
      modalAmplitudes: modalAmplitudes,
      modalQ: modalQ,
      estimatedTriadCoupling: clamp(0.5 + 1.2 * asym, 0.1, 1.8),
      estimatedZenoFlux: clamp(0.2 + 0.8 * asym, 0.0, 1.0)
    };
  }

  generateTestBurst(fundamentalHz = 55.0, manifold = 'GlottalColonic', sampleRate = 48000.0, numSamples = 4096) {
    const out = new Float32Array(numSamples);
    const ratios = kManifoldRatios[manifold] || kManifoldRatios.GlottalColonic;
    const f0 = clamp(fundamentalHz, 15.0, 400.0);

    for (let n = 0; n < numSamples; ++n) {
      const t = n / sampleRate;
      let sample = 0.0;
      for (let i = 0; i < 16; ++i) {
        const f = f0 * ratios[i];
        if (f > sampleRate * 0.48) continue;
        const decay = Math.exp(-t * (10.0 + i * 5.0));
        const amp = (1.0 / (1.0 + 0.3 * i)) * decay;
        sample += amp * Math.sin(kTwoPi * f * t);
      }
      out[n] = sample * 0.20;
    }
    return out;
  }

  static parseWavBuffer(arrayBuffer) {
    const view = new DataView(arrayBuffer);
    const riff = String.fromCharCode(view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3));
    const wave = String.fromCharCode(view.getUint8(8), view.getUint8(9), view.getUint8(10), view.getUint8(11));
    if (riff !== 'RIFF' || wave !== 'WAVE') {
      throw new Error('Invalid WAV file: missing RIFF/WAVE header');
    }

    let offset = 12;
    let fmtFound = false;
    let numChannels = 1;
    let sampleRate = 48000;
    let bitsPerSample = 16;
    let audioFormat = 1; // 1 = PCM, 3 = IEEE float
    let dataOffset = 0;
    let dataSize = 0;

    while (offset < view.byteLength - 8) {
      const chunkId = String.fromCharCode(
        view.getUint8(offset), view.getUint8(offset + 1),
        view.getUint8(offset + 2), view.getUint8(offset + 3)
      );
      const chunkSize = view.getUint32(offset + 4, true);

      if (chunkId === 'fmt ') {
        fmtFound = true;
        audioFormat = view.getUint16(offset + 8, true);
        numChannels = view.getUint16(offset + 10, true);
        sampleRate = view.getUint32(offset + 12, true);
        bitsPerSample = view.getUint16(offset + 22, true);
      } else if (chunkId === 'data') {
        dataOffset = offset + 8;
        dataSize = chunkSize;
        break;
      }
      offset += 8 + chunkSize;
    }

    if (!fmtFound || dataOffset === 0) {
      throw new Error('Corrupt WAV file: missing fmt or data sub-chunks');
    }

    const bytesPerSample = bitsPerSample / 8;
    const numFrames = Math.floor(dataSize / (numChannels * bytesPerSample));
    const monoOut = new Float32Array(numFrames);

    let samplePtr = dataOffset;
    for (let i = 0; i < numFrames; ++i) {
      let sum = 0.0;
      for (let ch = 0; ch < numChannels; ++ch) {
        let sample = 0.0;
        if (bitsPerSample === 16 && audioFormat === 1) {
          sample = view.getInt16(samplePtr, true) / 32768.0;
        } else if (bitsPerSample === 32 && audioFormat === 3) {
          sample = view.getFloat32(samplePtr, true);
        } else if (bitsPerSample === 24 && audioFormat === 1) {
          const b0 = view.getUint8(samplePtr);
          const b1 = view.getUint8(samplePtr + 1);
          const b2 = view.getInt8(samplePtr + 2);
          sample = ((b2 << 16) | (b1 << 8) | b0) / 8388608.0;
        }
        sum += sample;
        samplePtr += bytesPerSample;
      }
      monoOut[i] = sum / numChannels;
    }

    return { audio: monoOut, sampleRate: sampleRate };
  }
}
