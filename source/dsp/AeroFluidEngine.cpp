#include "AeroFluidEngine.h"
#include <cmath>
#include <algorithm>

namespace pf02::dsp {

AeroFluidEngine::AeroFluidEngine() noexcept {
    reset();
}

void AeroFluidEngine::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mPrng.setSeed(mBaseSeed);
    mViscosityFilter.reset();
    mGurgleFilter.reset();
    mGurgleFilter.setCutoff(mSampleRate, 1450.0f);
    reset();
}

void AeroFluidEngine::reset() noexcept {
    mPinkB0 = 0.0f;
    mPinkB1 = 0.0f;
    mPinkB2 = 0.0f;
    mVortexPhase = 0.0f;
    mReynoldsNumber = 0.0f;
    mVortexWhistleLevel = 0.0f;

    for (auto& voice : mBubbleVoices) {
        voice.active = false;
        voice.phase = 0.0f;
        voice.phaseInc = 0.0f;
        voice.chirp = 1.00016f;
        voice.envelope = 0.0f;
        voice.attackGain = 0.0f;
        voice.decayRate = 0.98f;
        voice.frequencyHz = 1500.0f;
    }
    mBubbleSpawnCountdown = 200;
    mBubbleActivity = 0.0f;

    mPoissonCountdown = 200;
    mDropletTriggered = false;
    mLastDropletAmp = 0.0f;
    for (auto& filter : mDropletFilters) {
        filter.reset();
    }
    mDropletImpulses.fill(0.0f);
    mDropletFilterIdx = 0;

    mEuclideanPhase = 0.0f;
    mEuclideanCurrentStep = 0;
    mEuclideanStepTriggered = false;
    mLastViscosity = -1.0f;
}

void AeroFluidEngine::triggerBubble(float radiusMm, float intensity, float chirpRate) noexcept {
    // Minnaert formula: f = 3.28325 / R (R in meters)
    const float rClamped = std::clamp(radiusMm, 0.5f, 6.5f) * 0.001f; // in meters
    const float freq = std::clamp(3.28325f / rClamped, 380.0f, 6566.0f);
    const float nyquist = mSampleRate * 0.48f;
    const float safeFreq = std::min(freq, nyquist);

    // Find best bubble voice: idle first, or voice with lowest envelope
    size_t bestIdx = 0;
    float lowestEnv = 1.0e10f;
    for (size_t i = 0; i < kMaxBubbleVoices; ++i) {
        if (!mBubbleVoices[i].active) {
            bestIdx = i;
            break;
        }
        if (mBubbleVoices[i].envelope < lowestEnv) {
            lowestEnv = mBubbleVoices[i].envelope;
            bestIdx = i;
        }
    }

    auto& v = mBubbleVoices[bestIdx];
    const bool wasActive = v.active;
    v.active = true;
    v.frequencyHz = safeFreq;
    if (!wasActive) {
        v.phase = 0.0f;
        v.attackGain = 0.0f;
    } else {
        v.attackGain = std::min(v.attackGain, 0.25f);
    }
    v.phaseInc = safeFreq * (kTwoPi / mSampleRate);
    v.chirp = std::clamp(chirpRate, 1.0f, 1.001f);
    v.envelope = std::clamp(intensity, 0.01f, 1.0f);

    // Viscous acoustic bubble damping ratio
    const float decayTimeSec = 0.014f + 0.022f * (rClamped * 1000.0f / 4.0f);
    v.decayRate = std::exp(-1.0f / (decayTimeSec * mSampleRate));
}

void AeroFluidEngine::triggerDroplet(float intensity) noexcept {
    const float mass = mPrng.nextIrwinHall(); // Irwin-Hall 3-stage distribution
    const float popFreq = 1200.0f + mass * 3600.0f;
    const float popQ    = 7.0f + mass * 12.0f;

    auto& filter = mDropletFilters[mDropletFilterIdx];
    filter.setBandpass(mSampleRate, popFreq, popQ);
    const float safeInt = std::clamp(intensity, 0.01f, 1.0f);
    mDropletImpulses[mDropletFilterIdx] = safeInt;
    mLastDropletAmp = safeInt;
    mDropletTriggered = true;

    mDropletFilterIdx = (mDropletFilterIdx + 1) % kMaxDropletFilters;
}

void AeroFluidEngine::triggerPeristalticPulse(float intensity) noexcept {
    mEuclideanStepTriggered = true;
    // Nucleate a micro-cluster of bubbles and droplet splatter
    triggerBubble(0.8f + mPrng.nextFloat() * 1.5f, intensity * 0.65f);
    triggerDroplet(intensity * 0.85f);
}

void AeroFluidEngine::triggerFlapBurst(float openingIntensity, float moisture, float dropletRate) noexcept {
    if (moisture < 1.0e-4f) return;

    // Explosive cluster of droplet clicks directly on mucosal opening transient
    const int numDrops = std::clamp(1 + static_cast<int>(moisture * (1.0f + dropletRate * 0.15f)), 1, static_cast<int>(kMaxDropletFilters));
    for (int i = 0; i < numDrops; ++i) {
        const float intensity = openingIntensity * moisture * (0.35f + 0.65f * mPrng.nextFloat()) * 0.5f;
        triggerDroplet(intensity);
    }

    // Minnaert bubble chirps on opening burst
    const int numBubbles = (mPrng.nextFloat() < (moisture * 0.85f)) ? 2 : 1;
    for (int i = 0; i < numBubbles; ++i) {
        const float rMm = 0.6f + mPrng.nextFloat() * 2.5f;
        const float bInt = moisture * (0.3f + 0.7f * mPrng.nextFloat()) * (0.3f + 0.3f * openingIntensity);
        triggerBubble(rMm, bInt);
    }
}

void AeroFluidEngine::updatePoissonClock(float rateHz) noexcept {
    if (rateHz <= 0.01f) {
        mPoissonCountdown = 48000;
        return;
    }
    // Poisson stochastic interval: Delta t = -ln(1 - U) / lambda
    const float u = mPrng.nextFloat01Strict();
    const float dt = -std::log(1.0f - u) / rateHz;
    mPoissonCountdown = std::max(1, static_cast<int>(dt * mSampleRate));
}

void AeroFluidEngine::updateEuclideanStep(const AeroFluidParams& p) noexcept {
    const int k = std::clamp(p.euclideanPeristalsisK, 1, 16);
    const int n = std::clamp(p.euclideanPeristalsisN, 1, 16);

    // Advance phase according to 16th-note tempo
    const float beatsPerSec = p.tempoBpm / 60.0f;
    const float stepRate = beatsPerSec * 4.0f; // 16th notes
    mEuclideanPhase += stepRate / mSampleRate;

    if (mEuclideanPhase >= 1.0f) {
        mEuclideanPhase -= 1.0f;
        mEuclideanCurrentStep = (mEuclideanCurrentStep + 1) % n;

        // Bjorklund Euclidean hit test: (step * k) % n < k
        if ((mEuclideanCurrentStep * k) % n < k) {
            triggerPeristalticPulse(0.70f);
        }
    }
}

float AeroFluidEngine::processSample(
    float airVelocity,
    float aperture,
    float zenoFlux,
    const AeroFluidParams& params) noexcept
{
    mDropletTriggered = false;
    mEuclideanStepTriggered = false;

    // Advance Euclidean peristaltic sequencer
    updateEuclideanStep(params);

    // 1. Helical Beltrami Vortex Jet Dynamics (curl v = lambda * v)
    const float posAirVel = std::max(0.0f, airVelocity);
    const float posAperture = std::max(0.0f, aperture);

    // Orifice Reynolds number: Re ~ v_air * aperture * 1.3e5
    mReynoldsNumber = posAirVel * posAperture * 1.3e5f;
    mReynoldsNumber = flushDenormal(mReynoldsNumber);

    float jetSignal = 0.0f;
    if (posAirVel > 0.01f && posAperture > 1.0e-5f) {
        // Paul Kellet pinking filter for turbulent shear spectrum
        const float white = mPrng.nextSignedFloat();
        mPinkB0 = 0.99886f * mPinkB0 + white * 0.0555179f;
        mPinkB1 = 0.99332f * mPinkB1 + white * 0.0750759f;
        mPinkB2 = 0.96900f * mPinkB2 + white * 0.1538520f;
        const float pink = (mPinkB0 + mPinkB1 + mPinkB2 + white * 0.5362f) * 0.11f;

        // Reynolds turbulence intensity modulated by instantaneous mucosal aperture
        const float turbLevel = std::clamp(params.reynoldsTurbulence, 0.0f, 1.0f);
        const float apertureMod = std::clamp(posAperture / 0.0012f, 0.0f, 1.5f);
        const float velNorm = std::clamp(posAirVel / 32.0f, 0.0f, 1.8f);
        const float turbGain = turbLevel * std::pow(velNorm, 1.35f) * apertureMod * (1.0f + 0.45f * zenoFlux);
        const float shearNoise = pink * turbGain * 0.32f;

        // Helical Beltrami vortex formant modulation (kept in organic acoustic range)
        const float lambda = std::clamp(params.beltramiVortexPitch, 0.1f, 10.0f);
        const float vortexFreq = std::clamp((lambda * (45.0f + posAirVel * 4.5f)), 60.0f, std::min(2400.0f, mSampleRate * 0.45f));
        mVortexPhase += vortexFreq * (kTwoPi / mSampleRate);
        if (mVortexPhase >= kTwoPi) mVortexPhase -= kTwoPi;

        const float whistle = FastSinTable::sin(mVortexPhase);
        mVortexWhistleLevel = whistle * (0.25f + 0.75f * pink * 3.0f) * turbGain * 0.08f;

        jetSignal = shearNoise + mVortexWhistleLevel;
    } else {
        mVortexWhistleLevel = 0.0f;
    }

    // 2. Poisson Stochastic Droplet Point-Process Clock
    const float moisture = std::clamp(params.multiphaseMoisture, 0.0f, 1.0f);
    if (moisture > 0.01f && params.poissonDropletRate > 0.0f) {
        --mPoissonCountdown;
        if (mPoissonCountdown <= 0) {
            // Trigger stochastic droplet pop
            const float popIntensity = moisture * (0.35f + 0.65f * mPrng.nextFloat());
            triggerDroplet(popIntensity);
            updatePoissonClock(params.poissonDropletRate);
        }
    }

    // Process resonant droplet filters
    float dropletSignal = 0.0f;
    for (size_t i = 0; i < kMaxDropletFilters; ++i) {
        const float impulse = mDropletImpulses[i] * 0.35f;
        mDropletImpulses[i] = flushDenormal(mDropletImpulses[i] * 0.65f);
        dropletSignal += mDropletFilters[i].process(impulse);
    }

    // 3. Minnaert Gas-Liquid Bubble Bank
    const float bubbleDensity = std::clamp(params.minnaertBubbleDensity, 0.0f, 100.0f);
    if (moisture > 0.01f && bubbleDensity > 0.1f && posAirVel > 0.5f) {
        --mBubbleSpawnCountdown;
        if (mBubbleSpawnCountdown <= 0) {
            // Spawn next resonant bubble
            const float rMm = 0.6f + mPrng.nextFloat() * 2.8f; // 0.6 to 3.4 mm
            const float intensity = moisture * (bubbleDensity * 0.01f) * (0.25f + 0.75f * mPrng.nextFloat());
            triggerBubble(rMm, intensity);

            // Calculate next spawn interval (samples)
            const float baseInterval = mSampleRate / (15.0f + 3.0f * bubbleDensity);
            const float jitter = 0.5f + mPrng.nextFloat();
            mBubbleSpawnCountdown = std::max(12, static_cast<int>(baseInterval * jitter));
        }
    }

    // Render 16 bubble voices
    float bubbleSum = 0.0f;
    float currentBubbleActivity = 0.0f;
    const float maxPhaseInc = 0.45f * kTwoPi;
    for (auto& v : mBubbleVoices) {
        if (!v.active) continue;

        v.attackGain += 0.14f * (1.0f - v.attackGain);
        const float val = v.envelope * v.attackGain * FastSinTable::sin(v.phase);
        bubbleSum += val;
        currentBubbleActivity += v.envelope;

        // Advance phase, upward Minnaert chirp & exponential decay
        v.phase += v.phaseInc;
        if (v.phase >= kTwoPi) v.phase -= kTwoPi;
        v.phaseInc = std::min(maxPhaseInc, v.phaseInc * v.chirp);

        v.envelope *= v.decayRate;
        if (v.envelope < 1.0e-4f) {
            v.active = false;
            v.envelope = 0.0f;
            v.attackGain = 0.0f;
        }
    }
    mBubbleActivity = flushDenormal(currentBubbleActivity * (1.0f / static_cast<float>(kMaxBubbleVoices)));

    // Smooth bubble stream with gurgle filter
    const float smoothedBubbles = mGurgleFilter.process(bubbleSum);

    // 4. Acoustic Coupling & Fluid Viscosity Damping
    const float rawAeroFluid = jetSignal * (1.0f - 0.4f * moisture) +
                               (dropletSignal * 0.75f + smoothedBubbles * 0.55f) * moisture;

    // Cutoff frequency drops with viscosity: 8000 Hz down to 600 Hz
    const float visc = std::clamp(params.fluidViscosity, 0.0f, 1.0f);
    if (std::abs(visc - mLastViscosity) > 1.0e-5f) {
        const float viscCutoff = 8000.0f * std::pow(0.075f, visc);
        mViscosityFilter.setCutoff(mSampleRate, viscCutoff);
        mLastViscosity = visc;
    }

    const float outSignal = mViscosityFilter.process(rawAeroFluid);
    return flushDenormal(outSignal);
}

} // namespace pf02::dsp
