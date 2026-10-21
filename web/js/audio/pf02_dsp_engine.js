/**
 * @file pf02_dsp_engine.js
 * @brief 100% Procedural Physical Modeling DSP Engine for Poopenfarten II (PF-02)
 *
 * Implements C++20 Mirrored Algorithms:
 *  1. Karnopp velocity-deadband stick-slip mucosal valve (vd = 0.0015 m/s)
 *  2. Stribeck breakaway exponential curve (vs = 0.06 m/s)
 *  3. Hunt-Crossley nonlinear Hertzian impact contact mechanics (y <= 0)
 *  4. Buchla 292 optical vactrol dynamic sag droop (attack ~2 ms, sagging tau)
 *  5. Helical Beltrami vortex jet (curl v = lambda * v) & Paul Kellet pinking filter
 *  6. Minnaert resonant gas-liquid bubble bank (f = 3.28325 / R)
 *  7. Poisson droplet point-process clock (Delta t = -ln(1-U)/lambda) with Irwin-Hall mass
 *  8. Bjorklund Euclidean peristaltic sequencer E(k, n)
 *  9. 16-Pole Zavalishin TPT State-Variable Filter (SVF) matrix with acoustic radiation damping
 * 10. O(N) Orthogonal Householder energy scattering matrix (H = I - (2/16) 1 1^T)
 * 11. DarkSnail Skew-Symmetric Bilinear Triad Operator S(W, i, j, k) = -S(W, i, k, j)
 * 12. DarkSnail so(16) TriadCascade with exact S^15 spherical norm retraction (< 1e-6 drift)
 * 13. 2D Continuous Spectral Jordan Cavity Field (201 channels projected to 16 modes)
 * 14. 3D RK4 Continuous Chaotic Lorenz Strange Attractor (odd/even modal detuning & Q-spread)
 * 15. Roland Dimension D tri-phase BBD chorus (0°, 120°, 240°, mono cancellation immune)
 * 16. Golden-Ratio stereo modal panning (Phi = 137.507764°)
 * 17. 15 Hz infrasonic 2-pole TPT highpass & 38 Hz visceral sub-bass reinforcement
 * 18. Hermite C1 soft-knee BoundedSaturator (knee = 0.72, ceiling = 1.00) & true-peak limiter (<= 1.05)
 *
 * ZERO Base64 PCM tables! ZERO static audio arrays! 100% Procedural synthesis.
 */

// ============================================================================
// Mathematical & Physical Constants
// ============================================================================
export const kPi             = 3.14159265358979323846;
export const kTwoPi          = 6.28318530717958647692;
export const kHalfPi         = 1.57079632679489661923;
export const kSqrt2          = 1.41421356237309504880;
export const kGoldenAngleRad = 2.39996322972865332223; // 137.507764° in radians
export const kAirDensityRho  = 1.204;                  // kg/m^3 at 20°C
export const kSlitWidthW     = 0.012;                  // 12 mm nominal mucosal slit width

export function flushDenormal(val) {
  if (!Number.isFinite(val) || Math.abs(val) < 1.0e-15) return 0.0;
  return val;
}

export function clamp(val, min, max) {
  return Math.max(min, Math.min(max, val));
}

export function dbToGain(db) {
  return Math.pow(10.0, db * 0.05);
}

export function gainToDb(gain) {
  return gain > 1.0e-5 ? 20.0 * Math.log10(gain) : -100.0;
}

export function midiNoteToHz(midiNote, a4Hz = 440.0) {
  return a4Hz * Math.pow(2.0, (midiNote - 69.0) / 12.0);
}

// ============================================================================
// Fast Precomputed Sine Lookup Table (2048 points, linear interpolation)
// ============================================================================
export class FastSinTable {
  static kTableSize = 2048;
  static kMask = 2047;
  static table = (() => {
    const t = new Float32Array(FastSinTable.kTableSize);
    for (let i = 0; i < FastSinTable.kTableSize; ++i) {
      t[i] = Math.sin((i * kTwoPi) / FastSinTable.kTableSize);
    }
    return t;
  })();

  static sin(angle) {
    if (!Number.isFinite(angle)) return 0.0;
    const norm = angle * (FastSinTable.kTableSize / kTwoPi);
    const idx = Math.floor(norm);
    const frac = norm - idx;
    const i0 = idx & FastSinTable.kMask;
    const i1 = (i0 + 1) & FastSinTable.kMask;
    return FastSinTable.table[i0] + frac * (FastSinTable.table[i1] - FastSinTable.table[i0]);
  }

  static cos(angle) {
    return FastSinTable.sin(angle + kHalfPi);
  }
}

// ============================================================================
// Fast XorShift64* PRNG
// ============================================================================
export class FastPRNG {
  constructor(seed = 0x853c49e6748fea9bn) {
    this.state = BigInt(seed);
    if (this.state === 0n) this.state = 0x853c49e6748fea9bn;
  }

  setSeed(seed) {
    this.state = BigInt(seed);
    if (this.state === 0n) this.state = 0x853c49e6748fea9bn;
  }

  nextU64() {
    let x = this.state;
    x ^= (x >> 12n) & 0xFFFFFFFFFFFFFFFFn;
    x ^= (x << 25n) & 0xFFFFFFFFFFFFFFFFn;
    x ^= (x >> 27n) & 0xFFFFFFFFFFFFFFFFn;
    this.state = x;
    return (x * 0x2545F4914F6CDD1Dn) & 0xFFFFFFFFFFFFFFFFn;
  }

  nextFloat() {
    const u = Number((this.nextU64() >> 32n) & 0xFFFFFFFFn);
    return u / 4294967296.0;
  }

  nextFloat01Strict() {
    return clamp(this.nextFloat(), 1.0e-7, 0.9999999);
  }

  nextSignedFloat() {
    return this.nextFloat() * 2.0 - 1.0;
  }

  nextIrwinHall() {
    return (this.nextFloat() + this.nextFloat() + this.nextFloat()) * (1.0 / 3.0);
  }
}

// ============================================================================
// 4-Point Catmull-Rom Hermite Cubic Interpolation Delay Line
// ============================================================================
export class HermiteDelayLine {
  constructor(capacity = 4096) {
    this.capacity = capacity;
    this.mask = capacity - 1;
    this.buffer = new Float32Array(capacity);
    this.writeIndex = 0;
  }

  reset() {
    this.buffer.fill(0.0);
    this.writeIndex = 0;
  }

  write(sample) {
    this.buffer[this.writeIndex] = flushDenormal(sample);
    this.writeIndex = (this.writeIndex + 1) & this.mask;
  }

  readHermite(delaySamples) {
    if (!Number.isFinite(delaySamples)) delaySamples = 1.0;
    const clampedDelay = clamp(delaySamples, 1.0, this.capacity - 4);
    const intDelay = Math.floor(clampedDelay);
    const frac = clampedDelay - intDelay;

    const base = (this.writeIndex + this.capacity - 1 - intDelay) & this.mask;
    const idxM1 = (base + 1) & this.mask;
    const idx0  = base;
    const idx1  = (base + this.capacity - 1) & this.mask;
    const idx2  = (base + this.capacity - 2) & this.mask;

    const ym1 = this.buffer[idxM1];
    const y0  = this.buffer[idx0];
    const y1  = this.buffer[idx1];
    const y2  = this.buffer[idx2];

    const c0 = y0;
    const c1 = 0.5 * (y1 - ym1);
    const c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
    const c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
    return flushDenormal(((c3 * frac + c2) * frac + c1) * frac + c0);
  }
}

// ============================================================================
// One-Pole Filters
// ============================================================================
export class OnePoleLowpass {
  constructor() {
    this.alpha = 0.1;
    this.state = 0.0;
  }

  reset() {
    this.state = 0.0;
  }

  setCutoff(sampleRate, cutoffHz) {
    const fs = sampleRate > 100.0 ? sampleRate : 48000.0;
    const fc = clamp(cutoffHz, 1.0, fs * 0.495);
    this.alpha = 1.0 - Math.exp(-kTwoPi * (fc / fs));
  }

  process(x) {
    this.state = flushDenormal(this.state + this.alpha * (x - this.state));
    return this.state;
  }
}

export class OnePoleHighpass {
  constructor() {
    this.alpha = 0.99;
    this.prevX = 0.0;
    this.state = 0.0;
  }

  reset() {
    this.prevX = 0.0;
    this.state = 0.0;
  }

  setCutoff(sampleRate, cutoffHz) {
    const fs = sampleRate > 100.0 ? sampleRate : 48000.0;
    const fc = clamp(cutoffHz, 1.0, fs * 0.495);
    this.alpha = 1.0 / (1.0 + kTwoPi * (fc / fs));
  }

  process(x) {
    const y = this.alpha * (this.state + x - this.prevX);
    this.prevX = flushDenormal(x);
    this.state = flushDenormal(y);
    return flushDenormal(y);
  }
}

// ============================================================================
// O(N) Orthogonal Householder Energy Scattering Matrix (N = 16)
// ============================================================================
export function applyHouseholderScattering16(modes, couplingDepth = 1.0) {
  let sum = 0.0;
  for (let i = 0; i < 16; ++i) sum += modes[i];
  const factor = (couplingDepth * (2.0 / 16.0)) * sum;
  for (let i = 0; i < 16; ++i) {
    modes[i] = flushDenormal(modes[i] - factor);
  }
}

// ============================================================================
// 1. Karnopp Stick-Slip Mucosal Exciter (DECK 01)
// ============================================================================
export class MucosalExciter {
  constructor() {
    this.sampleRate = 48000.0;
    this.prng = new FastPRNG(0x504F4F50454E32n);
    this.dcBlocker = new OnePoleHighpass();

    this.displacement = 0.0;
    this.velocity = 0.0;
    this.acceleration = 0.0;
    this.airVelocity = 0.0;
    this.upstreamPressure = 0.0;
    this.prevVolumeFlow = 0.0;
    this.stuck = true;
    this.burstThresholdPa = 250.0;
    this.justOpened = false;
    this.openingTransientAmp = 0.0;
    this.impactSlapAmp = 0.0;
    this.vactrolConductance = 0.0;
  }

  prepare(sampleRate) {
    this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    this.prng.setSeed(0x504F4F50454E32n);
    this.dcBlocker.setCutoff(this.sampleRate, 14.0);
    this.reset();
  }

  reset() {
    this.displacement = 0.0;
    this.velocity = 0.0;
    this.acceleration = 0.0;
    this.airVelocity = 0.0;
    this.upstreamPressure = 0.0;
    this.prevVolumeFlow = 0.0;
    this.stuck = true;
    this.burstThresholdPa = 250.0;
    this.justOpened = false;
    this.openingTransientAmp = 0.0;
    this.impactSlapAmp = 0.0;
    this.vactrolConductance = 0.0;
    this.dcBlocker.reset();
  }

  triggerSealBurst(burstIntensity) {
    this.stuck = false;
    this.justOpened = true;
    const burstVel = clamp(burstIntensity, 0.1, 10.0) * 2.5;
    this.velocity = burstVel;
    this.openingTransientAmp = burstVel;
    this.displacement = Math.max(this.displacement, 0.0003);
  }

  triggerSlap(vel, hardness = 2.1) {
    const slapVel = -Math.abs(vel) * 2.0;
    this.velocity = slapVel;
    const normHardness = clamp(hardness, 1.2, 3.0);
    this.impactSlapAmp = Math.abs(vel) * Math.pow(normHardness, 1.5);
  }

  processSample(params) {
    this.justOpened = false;
    this.openingTransientAmp = 0.0;
    this.impactSlapAmp = 0.0;

    const dt = 1.0 / this.sampleRate;

    // 1. Buchla 292 Optical Vactrol Dynamic Sag Droop
    const uDrive = clamp(params.colonicPressure ?? 0.70, 0.0, 1.0);
    let vactrolTau;
    if (uDrive >= this.vactrolConductance) {
      vactrolTau = 0.002; // 2 ms attack
    } else {
      const kSagKappa = 4.5;
      const sagFactor = 1.0 + kSagKappa * (1.0 - this.vactrolConductance) * (1.0 - this.vactrolConductance);
      const tauDecaySec = Math.max(0.005, (params.vactrolSagTauMs ?? 85.0) * 0.001);
      vactrolTau = tauDecaySec * sagFactor;
    }

    const alphaVactrol = 1.0 - Math.exp(-dt / Math.max(1.0e-5, vactrolTau));
    this.vactrolConductance += alphaVactrol * (uDrive - this.vactrolConductance);
    this.vactrolConductance = flushDenormal(clamp(this.vactrolConductance, 0.0, 1.0));

    const pDrive = Math.pow(this.vactrolConductance, 1.8) * 2200.0; // Up to 2200 Pa

    if (pDrive < 1.0e-3) {
      this.displacement = 0.0;
      this.velocity = 0.0;
      this.acceleration = 0.0;
      this.airVelocity = 0.0;
      this.upstreamPressure = 0.0;
      this.prevVolumeFlow = 0.0;
      this.stuck = true;
      return this.dcBlocker.process(0.0);
    }

    // 2. Fundamental Frequency & Viscoelastic Mechanics
    const fClamped = clamp(params.frequencyHz ?? 55.0, 7.0, 420.0);
    const tensionScale = 0.75 + 0.50 * clamp(params.mucosalTension ?? 0.50, 0.0, 1.0);
    const fEff = clamp(fClamped * tensionScale, 7.0, 600.0);
    const omega0 = kTwoPi * fEff;

    const mEff = 0.0012; // 1.2 g effective tissue mass
    const kSpring = mEff * omega0 * omega0;
    const rDamping = 2.0 * 0.075 * omega0 * mEff;
    const y0 = clamp((params.restingApertureMm ?? 1.0) * 0.001, 0.0001, 0.005);

    // 3. Upstream Pressure Accumulation
    const fillRate = 3.5 * omega0;
    const currentArea = (this.stuck || this.displacement <= 0.0) ? 0.0 : (kSlitWidthW * this.displacement);
    const currentVolumeFlow = currentArea * this.airVelocity;
    const dPup = fillRate * (pDrive - this.upstreamPressure) - (currentVolumeFlow * omega0 * 10.0);
    this.upstreamPressure = clamp(this.upstreamPressure + dt * dPup, 0.0, 4500.0);
    this.upstreamPressure = flushDenormal(this.upstreamPressure);

    // 4. Karnopp Stick-Slip Friction & Stribeck Dynamics
    const kKarnoppDeadband = 0.0015; // vd = 0.0015 m/s
    const stictionLevel = clamp(params.karnoppStiction ?? 0.65, 0.0, 1.0);
    const normalForce = (this.upstreamPressure * 0.00025) + (stictionLevel * 0.35);
    const fs = 0.85 * normalForce;
    const fc = 0.35 * normalForce;
    const vStribeck = Math.max(0.05, params.stribeckBreakaway ?? 2.2);

    if (this.stuck) {
      if (this.upstreamPressure >= this.burstThresholdPa || (pDrive > 50.0 && this.upstreamPressure >= 0.85 * pDrive)) {
        this.stuck = false;
        this.justOpened = true;
        const surge = Math.sqrt(Math.max(0.0, 2.0 * this.upstreamPressure / mEff)) * 0.038 + 1.6;
        this.velocity = surge;
        this.openingTransientAmp = surge;
        this.displacement = 0.00020;
        this.burstThresholdPa = (150.0 + 600.0 * stictionLevel) * (0.85 + 0.30 * this.prng.nextFloat());
      } else {
        this.displacement = 0.0;
        this.velocity = 0.0;
        this.acceleration = 0.0;
        this.airVelocity = 0.0;
        return this.dcBlocker.process(0.0);
      }
    }

    // 5. Airflow Velocity via Bernoulli Equation
    if (!this.stuck && this.displacement > 1.0e-6 && this.upstreamPressure > 0.0) {
      this.airVelocity = Math.sqrt(2.0 * this.upstreamPressure / kAirDensityRho);
    } else {
      this.airVelocity = 0.0;
    }
    this.airVelocity = flushDenormal(this.airVelocity);

    // 6. Force Balance on Mucosal Tissue
    const dispOffset = this.displacement - y0;
    const fSpring = -kSpring * dispOffset;
    const fDamp   = -rDamping * this.velocity;
    const fPressure = this.upstreamPressure * 0.00022;
    const fBernoulli = (this.displacement > 1.0e-6) ?
      (-0.5 * kAirDensityRho * this.airVelocity * this.airVelocity * (kSlitWidthW * 0.004)) : 0.0;

    let fFriction = 0.0;
    if (Math.abs(this.velocity) < kKarnoppDeadband) {
      fFriction = clamp(this.velocity * 400.0, -fs, fs);
    } else {
      const sgn = (this.velocity > 0.0) ? 1.0 : -1.0;
      const decay = Math.exp(-(this.velocity * this.velocity) / (vStribeck * vStribeck));
      fFriction = sgn * (fc + (fs - fc) * decay) + 0.12 * normalForce * this.velocity;
    }

    // 7. Hunt-Crossley Nonlinear Hertzian Impact Contact Mechanics (y <= 0)
    let fImpact = 0.0;
    if (this.displacement <= 0.0) {
      const penetrationX = -this.displacement;
      const xDot = -this.velocity;
      const hardnessNorm = clamp(((params.tissueHardness ?? 2.1) - 1.2) / 1.8, 0.0, 1.0);
      const kc = 1.0e6 * Math.pow(10.0, 2.8 * hardnessNorm);
      const lambdaC = 40.0 * (1.0 - 0.4 * clamp(params.impactRestitution ?? 0.45, 0.0, 1.0));
      const alpha = clamp(params.tissueHardness ?? 2.1, 1.2, 3.0);

      const xPow = Math.pow(Math.max(1.0e-8, penetrationX), alpha);
      const contactForce = Math.max(0.0, kc * xPow + lambdaC * xPow * xDot);
      fImpact = contactForce;

      if (this.velocity < -0.1) {
        this.impactSlapAmp = contactForce * 0.002;
      }

      if (Math.abs(this.velocity) < kKarnoppDeadband * 2.0 && stictionLevel > 0.15) {
        this.stuck = true;
        this.displacement = 0.0;
        this.velocity = 0.0;
      }
    }

    // 8. Symplectic Verlet Integration
    const totalForce = fSpring + fDamp + fPressure + fBernoulli + fImpact - fFriction;
    const aNext = totalForce / mEff;

    const xNext = this.displacement + this.velocity * dt + 0.5 * this.acceleration * dt * dt;
    this.velocity += 0.5 * (this.acceleration + aNext) * dt;
    this.displacement = flushDenormal(clamp(xNext, -0.001, 0.015));
    this.acceleration = aNext;

    // 9. Acoustic Output Signal
    const newArea = (this.displacement > 0.0) ? (kSlitWidthW * this.displacement) : 0.0;
    const newVolumeFlow = newArea * this.airVelocity;
    const dFlowDt = (newVolumeFlow - this.prevVolumeFlow) * this.sampleRate;
    this.prevVolumeFlow = newVolumeFlow;

    const acousticWave = dFlowDt * 0.08 + this.impactSlapAmp * 0.5 + this.openingTransientAmp * 0.35;
    return this.dcBlocker.process(acousticWave);
  }
}

// ============================================================================
// 2. Multiphase AeroFluid Engine (DECK 02)
// ============================================================================
export class AeroFluidEngine {
  constructor() {
    this.sampleRate = 48000.0;
    this.prng = new FastPRNG(0x4145524F464C55n);
    this.viscosityFilter = new OnePoleLowpass();
    this.gurgleFilter = new OnePoleLowpass();

    this.pinkB0 = 0.0;
    this.pinkB1 = 0.0;
    this.pinkB2 = 0.0;
    this.vortexPhase = 0.0;
    this.reynoldsNumber = 0.0;
    this.vortexWhistleLevel = 0.0;

    // Minnaert Bubble Voices (16)
    this.bubbleVoices = Array.from({ length: 16 }, () => ({
      active: false,
      phase: 0.0,
      phaseInc: 0.0,
      envelope: 0.0,
      decayRate: 0.98,
      frequencyHz: 1500.0
    }));
    this.bubbleSpawnCountdown = 200;
    this.bubbleActivity = 0.0;

    // Poisson Droplets
    this.poissonCountdown = 2000;
    this.dropletTriggered = false;
    this.lastDropletAmp = 0.0;
    this.dropletFilters = Array.from({ length: 4 }, () => ({
      b0: 0.0, b1: 0.0, b2: 0.0, a1: 0.0, a2: 0.0, s1: 0.0, s2: 0.0,
      reset() { this.s1 = 0.0; this.s2 = 0.0; },
      setBandpass(sampleRate, freqHz, q) {
        const fs = sampleRate > 100.0 ? sampleRate : 48000.0;
        const fc = clamp(freqHz, 50.0, fs * 0.48);
        const w0 = kTwoPi * (fc / fs);
        const sinW0 = FastSinTable.sin(w0);
        const cosW0 = FastSinTable.cos(w0);
        const safeQ = Math.max(0.1, q);
        const alpha = sinW0 / (2.0 * safeQ);
        const a0 = 1.0 + alpha;
        const invA0 = 1.0 / a0;
        this.b0 = (sinW0 * 0.5) * invA0;
        this.b1 = 0.0;
        this.b2 = (-sinW0 * 0.5) * invA0;
        this.a1 = (-2.0 * cosW0) * invA0;
        this.a2 = (1.0 - alpha) * invA0;
      },
      process(x) {
        const y = this.b0 * x + this.s1;
        this.s1 = flushDenormal(this.b1 * x - this.a1 * y + this.s2);
        this.s2 = flushDenormal(this.b2 * x - this.a2 * y);
        return flushDenormal(y);
      }
    }));
    this.dropletImpulses = new Float32Array(4);
    this.dropletFilterIdx = 0;

    // Euclidean Sequencer
    this.euclideanPhase = 0.0;
    this.euclideanCurrentStep = 0;
    this.euclideanStepTriggered = false;
  }

  prepare(sampleRate) {
    this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    this.prng.setSeed(0x4145524F464C55n);
    this.viscosityFilter.reset();
    this.gurgleFilter.reset();
    this.gurgleFilter.setCutoff(this.sampleRate, 850.0);
    this.reset();
  }

  reset() {
    this.pinkB0 = 0.0;
    this.pinkB1 = 0.0;
    this.pinkB2 = 0.0;
    this.vortexPhase = 0.0;
    this.reynoldsNumber = 0.0;
    this.vortexWhistleLevel = 0.0;
    for (const v of this.bubbleVoices) {
      v.active = false;
      v.envelope = 0.0;
    }
    this.bubbleSpawnCountdown = 200;
    this.bubbleActivity = 0.0;
    this.poissonCountdown = 200;
    this.dropletTriggered = false;
    this.lastDropletAmp = 0.0;
    for (const f of this.dropletFilters) f.reset();
    this.dropletImpulses.fill(0.0);
    this.dropletFilterIdx = 0;
    this.euclideanPhase = 0.0;
    this.euclideanCurrentStep = 0;
    this.euclideanStepTriggered = false;
  }

  triggerBubble(radiusMm, intensity) {
    const rClamped = clamp(radiusMm, 0.5, 4.0) * 0.001;
    const freq = clamp(3.28325 / rClamped, 650.0, 6566.0);
    const safeFreq = Math.min(freq, this.sampleRate * 0.48);

    let bestIdx = 0;
    let lowestEnv = 1.0e10;
    for (let i = 0; i < 16; ++i) {
      if (!this.bubbleVoices[i].active) {
        bestIdx = i;
        break;
      }
      if (this.bubbleVoices[i].envelope < lowestEnv) {
        lowestEnv = this.bubbleVoices[i].envelope;
        bestIdx = i;
      }
    }

    const v = this.bubbleVoices[bestIdx];
    v.active = true;
    v.frequencyHz = safeFreq;
    v.phase = this.prng.nextFloat() * kTwoPi;
    v.phaseInc = safeFreq * (kTwoPi / this.sampleRate);
    v.envelope = clamp(intensity, 0.01, 1.0);
    const decayTimeSec = 0.012 + 0.018 * (rClamped * 1000.0 / 4.0);
    v.decayRate = Math.exp(-1.0 / (decayTimeSec * this.sampleRate));
  }

  triggerDroplet(intensity) {
    const mass = this.prng.nextIrwinHall();
    const popFreq = 1200.0 + mass * 3600.0;
    const popQ    = 7.0 + mass * 12.0;

    const filter = this.dropletFilters[this.dropletFilterIdx];
    filter.setBandpass(this.sampleRate, popFreq, popQ);
    const safeInt = clamp(intensity, 0.01, 1.0);
    this.dropletImpulses[this.dropletFilterIdx] = safeInt;
    this.lastDropletAmp = safeInt;
    this.dropletTriggered = true;
    this.dropletFilterIdx = (this.dropletFilterIdx + 1) % 4;
  }

  triggerPeristalticPulse(intensity) {
    this.euclideanStepTriggered = true;
    this.triggerBubble(0.8 + this.prng.nextFloat() * 1.5, intensity * 0.65);
    this.triggerDroplet(intensity * 0.85);
  }

  triggerFlapBurst(openingIntensity, moisture, dropletRate) {
    if (moisture < 1.0e-4) return;
    const numDrops = clamp(1 + Math.floor(moisture * (1.0 + dropletRate * 0.15)), 1, 4);
    for (let i = 0; i < numDrops; ++i) {
      const intensity = openingIntensity * moisture * (0.35 + 0.65 * this.prng.nextFloat()) * 0.5;
      this.triggerDroplet(intensity);
    }
    const numBubbles = (this.prng.nextFloat() < (moisture * 0.85)) ? 2 : 1;
    for (let i = 0; i < numBubbles; ++i) {
      const rMm = 0.6 + this.prng.nextFloat() * 2.5;
      const bInt = moisture * (0.3 + 0.7 * this.prng.nextFloat()) * (0.3 + 0.3 * openingIntensity);
      this.triggerBubble(rMm, bInt);
    }
  }

  updatePoissonClock(rateHz) {
    if (rateHz <= 0.01) {
      this.poissonCountdown = 48000;
      return;
    }
    const u = this.prng.nextFloat01Strict();
    const dt = -Math.log(1.0 - u) / rateHz;
    this.poissonCountdown = Math.max(1, Math.floor(dt * this.sampleRate));
  }

  updateEuclideanStep(params) {
    const k = clamp(params.euclideanPeristalsisK ?? 5, 1, 16);
    const n = clamp(params.euclideanPeristalsisN ?? 16, 1, 16);
    const beatsPerSec = (params.tempoBpm ?? 120.0) / 60.0;
    const stepRate = beatsPerSec * 4.0; // 16th notes
    this.euclideanPhase += stepRate / this.sampleRate;

    if (this.euclideanPhase >= 1.0) {
      this.euclideanPhase -= 1.0;
      this.euclideanCurrentStep = (this.euclideanCurrentStep + 1) % n;
      if ((this.euclideanCurrentStep * k) % n < k) {
        this.triggerPeristalticPulse(0.70);
      }
    }
  }

  processSample(airVelocity, aperture, zenoFlux, params) {
    this.dropletTriggered = false;
    this.euclideanStepTriggered = false;
    this.updateEuclideanStep(params);

    const posAirVel = Math.max(0.0, airVelocity);
    const posAperture = Math.max(0.0, aperture);
    this.reynoldsNumber = flushDenormal(posAirVel * posAperture * 1.3e5);

    let jetSignal = 0.0;
    if (posAirVel > 0.01 && posAperture > 1.0e-5) {
      const white = this.prng.nextSignedFloat();
      this.pinkB0 = 0.99886 * this.pinkB0 + white * 0.0555179;
      this.pinkB1 = 0.99332 * this.pinkB1 + white * 0.0750759;
      this.pinkB2 = 0.96900 * this.pinkB2 + white * 0.1538520;
      const pink = (this.pinkB0 + this.pinkB1 + this.pinkB2 + white * 0.5362) * 0.11;

      const turbLevel = clamp(params.reynoldsTurbulence ?? 0.40, 0.0, 1.0);
      const turbGain = turbLevel * Math.pow(posAirVel / 25.0, 1.6) * (1.0 + 0.6 * zenoFlux);
      const shearNoise = pink * turbGain * 0.35;

      const lambda = clamp(params.beltramiVortexPitch ?? 2.8, 0.1, 10.0);
      const vortexFreq = clamp(lambda * posAirVel * 25.0, 80.0, this.sampleRate * 0.45);
      this.vortexPhase += vortexFreq * (kTwoPi / this.sampleRate);
      if (this.vortexPhase >= kTwoPi) this.vortexPhase -= kTwoPi;

      const whistle = FastSinTable.sin(this.vortexPhase);
      this.vortexWhistleLevel = whistle * turbGain * 0.18;
      jetSignal = shearNoise + this.vortexWhistleLevel;
    } else {
      this.vortexWhistleLevel = 0.0;
    }

    // Poisson Droplets
    const moisture = clamp(params.multiphaseMoisture ?? 0.35, 0.0, 1.0);
    const dropletRate = params.poissonDropletRate ?? 8.5;
    if (moisture > 0.01 && dropletRate > 0.0) {
      --this.poissonCountdown;
      if (this.poissonCountdown <= 0) {
        const popIntensity = moisture * (0.35 + 0.65 * this.prng.nextFloat());
        this.triggerDroplet(popIntensity);
        this.updatePoissonClock(dropletRate);
      }
    }

    let dropletSignal = 0.0;
    for (let i = 0; i < 4; ++i) {
      const impulse = this.dropletImpulses[i];
      this.dropletImpulses[i] = 0.0;
      dropletSignal += this.dropletFilters[i].process(impulse);
    }

    // Minnaert Bubbles
    const bubbleDensity = clamp(params.minnaertBubbleDensity ?? 40.0, 0.0, 100.0);
    if (moisture > 0.01 && bubbleDensity > 0.1 && posAirVel > 0.5) {
      --this.bubbleSpawnCountdown;
      if (this.bubbleSpawnCountdown <= 0) {
        const rMm = 0.6 + this.prng.nextFloat() * 2.8;
        const intensity = moisture * (bubbleDensity * 0.01) * (0.25 + 0.75 * this.prng.nextFloat());
        this.triggerBubble(rMm, intensity);
        const baseInterval = this.sampleRate / (15.0 + 3.0 * bubbleDensity);
        const jitter = 0.5 + this.prng.nextFloat();
        this.bubbleSpawnCountdown = Math.max(12, Math.floor(baseInterval * jitter));
      }
    }

    let bubbleSum = 0.0;
    let currentBubbleActivity = 0.0;
    for (const v of this.bubbleVoices) {
      if (!v.active) continue;
      const val = v.envelope * FastSinTable.sin(v.phase);
      bubbleSum += val;
      currentBubbleActivity += v.envelope;
      v.phase += v.phaseInc;
      if (v.phase >= kTwoPi) v.phase -= kTwoPi;
      v.envelope *= v.decayRate;
      if (v.envelope < 1.0e-4) {
        v.active = false;
        v.envelope = 0.0;
      }
    }
    this.bubbleActivity = flushDenormal(currentBubbleActivity * (1.0 / 16.0));
    const smoothedBubbles = this.gurgleFilter.process(bubbleSum);

    const rawAeroFluid = jetSignal * (1.0 - 0.4 * moisture) +
      (dropletSignal * 0.75 + smoothedBubbles * 0.55) * moisture;

    const visc = clamp(params.fluidViscosity ?? 0.30, 0.0, 1.0);
    const viscCutoff = 8000.0 * Math.pow(0.075, visc);
    this.viscosityFilter.setCutoff(this.sampleRate, viscCutoff);

    return flushDenormal(this.viscosityFilter.process(rawAeroFluid));
  }
}

// ============================================================================
// 3. DarkSnail Skew-Symmetric Bilinear Triad Operator & Lie Algebra
// ============================================================================
export class SkewSymmetricTriadOperator {
  constructor(Cu = 16, M = 16) {
    this.Cu = Cu;
    this.M = M;
    this.weights = Array.from({ length: Cu }, () =>
      Array.from({ length: M }, () => new Float32Array(M))
    );
    this.initDefaultWeights();
  }

  initDefaultWeights() {
    const prng = new FastPRNG(0x4441524B534E41n); // "DARKSNA"
    const scale = 0.5 / Math.sqrt(this.M);
    for (let i = 0; i < this.Cu; ++i) {
      for (let j = 0; j < this.M; ++j) {
        for (let k = 0; k < this.M; ++k) {
          if (j === k) {
            this.weights[i][j][k] = 0.0;
          } else {
            this.weights[i][j][k] = prng.nextSignedFloat() * scale;
          }
        }
      }
    }
  }

  contractToLieMatrix(u) {
    const A = Array.from({ length: this.M }, () => new Float32Array(this.M));
    for (let j = 0; j < this.M; ++j) {
      A[j][j] = 0.0;
      for (let k = j + 1; k < this.M; ++k) {
        let val = 0.0;
        for (let i = 0; i < this.Cu; ++i) {
          val += u[i] * (this.weights[i][j][k] - this.weights[i][k][j]);
        }
        A[j][k] = val;
        A[k][j] = -val;
      }
    }
    return A;
  }

  evaluate(u, v) {
    const A = this.contractToLieMatrix(u);
    const out = new Float32Array(this.M);

    for (let k = 0; k < this.M; ++k) {
      let sum = 0.0;
      for (let j = 0; j < this.M; ++j) {
        sum += v[j] * A[j][k];
      }
      out[k] = sum;
    }

    // Two-pass Kahan/Parlett tangent projection: eliminates float roundoff <v, out> = 0
    let vNormSq = 0.0;
    for (let k = 0; k < this.M; ++k) vNormSq += v[k] * v[k];

    if (vNormSq > 1.0e-30) {
      for (let pass = 0; pass < 2; ++pass) {
        let inner = 0.0;
        for (let k = 0; k < this.M; ++k) inner += v[k] * out[k];
        const proj = inner / vNormSq;
        for (let k = 0; k < this.M; ++k) out[k] -= proj * v[k];
      }
    }

    // Coordinate-wise polish on argmax |v_j|
    let inner = 0.0;
    let maxIdx = 0;
    let maxAbsV = 0.0;
    for (let k = 0; k < this.M; ++k) {
      inner += v[k] * out[k];
      const absV = Math.abs(v[k]);
      if (absV > maxAbsV) {
        maxAbsV = absV;
        maxIdx = k;
      }
    }
    if (maxAbsV > 1.0e-5) {
      out[maxIdx] -= inner / v[maxIdx];
    }

    for (let k = 0; k < this.M; ++k) out[k] = flushDenormal(out[k]);
    return out;
  }
}

// SlowFastTriadCascade with S^15 spherical norm retraction
export class SlowFastTriadCascade {
  constructor(N = 16) {
    this.N = N;
    this.cascadeGain = 0.85;
    this.unrollSteps = 4;
    this.lastRadialDrift = 0.0;
    this.lastTangentOrthErr = 0.0;
  }

  reset() {
    this.lastRadialDrift = 0.0;
    this.lastTangentOrthErr = 0.0;
  }

  static retractToSphere(vUpd, vRef) {
    let refNormSq = 0.0;
    let updNormSq = 0.0;
    for (let i = 0; i < 16; ++i) {
      refNormSq += vRef[i] * vRef[i];
      updNormSq += vUpd[i] * vUpd[i];
    }
    if (refNormSq <= 1.0e-30 || updNormSq <= 1.0e-30) return new Float32Array(vUpd);

    const scale = Math.sqrt(refNormSq) / Math.sqrt(updNormSq);
    const out = new Float32Array(16);
    for (let i = 0; i < 16; ++i) {
      out[i] = flushDenormal(vUpd[i] * scale);
    }
    return out;
  }

  processCascade(uSlow, vFast, triadOp, unrollOverride = 0) {
    const K = (unrollOverride >= 1 && unrollOverride <= 16) ? unrollOverride : this.unrollSteps;
    const v0 = new Float32Array(vFast);

    let norm0Sq = 0.0;
    for (let i = 0; i < 16; ++i) norm0Sq += v0[i] * v0[i];

    const uCur = new Float32Array(uSlow);
    let vCur = new Float32Array(vFast);
    const invK = 1.0 / K;
    let maxOrthErr = 0.0;

    for (let step = 0; step < K; ++step) {
      const t = 16 - K + step;
      const alpha_t = 0.15 * Math.sin(t * (kPi / 16.0));
      const gamma_t = this.cascadeGain * (1.0 + alpha_t);

      const scaledU = new Float32Array(16);
      for (let i = 0; i < 16; ++i) scaledU[i] = uCur[i] * gamma_t;

      const deltaV = triadOp.evaluate(scaledU, vCur);

      let inner = 0.0;
      for (let i = 0; i < 16; ++i) inner += vCur[i] * deltaV[i];
      maxOrthErr = Math.max(maxOrthErr, Math.abs(inner));

      const vUpd = new Float32Array(16);
      for (let i = 0; i < 16; ++i) vUpd[i] = vCur[i] + deltaV[i];

      const vTilde = SlowFastTriadCascade.retractToSphere(vUpd, v0);

      if (step + 1 < K) {
        for (let i = 0; i < 16; ++i) {
          uCur[i] = flushDenormal(uCur[i] + invK * (vTilde[i] - vCur[i]));
        }
      }
      vCur = vTilde;
    }

    let normFinalSq = 0.0;
    for (let i = 0; i < 16; ++i) normFinalSq += vCur[i] * vCur[i];
    this.lastRadialDrift = Math.abs(normFinalSq - norm0Sq);
    this.lastTangentOrthErr = maxOrthErr;

    return vCur;
  }
}

// 201-Channel 2D Continuous Spectral Jordan Field
export class ContinuousSpectralJordanField {
  constructor() {
    this.channels = new Float32Array(201);
    this.generateCavityField(1.0, 0.5, 1);
  }

  generateCavityField(cavityScale = 1.0, aspect = 0.5, boundaryMode = 1) {
    const H = 8, W = 8;
    const scale = clamp(cavityScale, 0.2, 3.0);
    const rAspect = clamp(aspect, 0.2, 2.0);

    // 1. Ch 0..63: 2D DCT-II
    for (let ky = 0; ky < 8; ++ky) {
      for (let kx = 0; kx < 8; ++kx) {
        const ch = ky * 8 + kx;
        const yr = 3.5, xc = 3.5;
        const psiY = Math.cos((kPi * (2.0 * yr + 1.0) * ky) / (2.0 * H));
        const psiX = Math.cos((kPi * (2.0 * xc + 1.0) * kx) / (2.0 * W));
        const heatLoss = Math.exp(-0.045 * (ky * ky + kx * kx) / scale);
        this.channels[ch] = flushDenormal(psiY * psiX * heatLoss);
      }
    }

    // 2. Ch 64..143: Ray reflection
    let reflectCoeff = 0.85;
    if (boundaryMode === 0) reflectCoeff = 0.95;
    else if (boundaryMode === 2) reflectCoeff = 0.65;
    else if (boundaryMode === 3) reflectCoeff = 0.90;

    for (let i = 64; i < 144; ++i) {
      const angle = (i - 64) * (kTwoPi / 80.0);
      const path1 = Math.cos(angle * 3.0 * rAspect);
      const path2 = reflectCoeff * Math.cos(angle * 7.0 / scale);
      this.channels[i] = flushDenormal(0.5 * path1 + 0.5 * path2);
    }

    // 3. Ch 144..200: Jordan potentials
    for (let i = 144; i < 201; ++i) {
      const idxNorm = (i - 144) / 56.0;
      const rad = idxNorm * kPi;
      const gx = Math.sin(rad * 2.0);
      const gy = Math.cos(rad * 3.0 * rAspect);
      const curl2D = gx * Math.sin(rad * 4.0) - gy * Math.cos(rad * 4.0);
      const div2D  = -4.0 * Math.cos(rad * 2.0);
      const beltramiDet = Math.sqrt(1.0 + gx * gx + gy * gy + curl2D * curl2D) - 1.0;
      this.channels[i] = flushDenormal(0.4 * curl2D + 0.3 * div2D + 0.3 * beltramiDet);
    }
  }

  projectTo16Modes() {
    const modes = new Float32Array(16);
    for (let m = 0; m < 16; ++m) {
      let sum = 0.0;
      for (let k = 0; k < 12; ++k) {
        const ch = (m * 12 + k) % 201;
        sum += this.channels[ch];
      }
      modes[m] = flushDenormal(sum * (1.0 / 12.0));
    }
    return modes;
  }
}

// ============================================================================
// 4. 16-Pole Zavalishin TPT SVF Modal Resonator Matrix (DECK 03)
// ============================================================================
export const kManifoldRatios = {
  SigmoidHorn: [
    1.0000, 1.1926, 1.4866, 1.8493,
    2.2561, 2.6926, 3.1496, 3.6210,
    4.1024, 4.5913, 5.0858, 5.5846,
    6.0867, 6.5914, 7.0980, 7.6062
  ],
  ChladniRing: [
    1.0000, 1.5882, 2.0588, 2.7647,
    3.1765, 4.0588, 4.4118, 5.3529,
    5.8824, 6.7647, 7.4118, 8.5294,
    9.3529, 10.2941, 11.6471, 13.0000
  ],
  GlottalColonic: [
    1.0000, 1.4500, 2.1000, 2.8500,
    3.7500, 4.6000, 5.5500, 6.6000,
    7.7500, 9.0000, 10.3500, 11.8000,
    13.4000, 15.1000, 17.0000, 19.1000
  ],
  PorcelainBowl: [
    1.0000, 1.3416, 1.7321, 2.2361,
    2.6458, 3.1623, 3.6056, 4.1231,
    4.5826, 5.0990, 5.5678, 6.0828,
    6.5574, 7.0711, 7.5498, 8.0623
  ]
};

export class ModalResonatorMatrix {
  constructor() {
    this.sampleRate = 48000.0;
    this.g = new Float32Array(16);
    this.k = new Float32Array(16);
    this.a1 = new Float32Array(16);
    this.s1 = new Float32Array(16);
    this.s2 = new Float32Array(16);
    this.qNorm = new Float32Array(16);
    this.currentFrequencies = new Float32Array(16);
    this.modalEnergies = new Float32Array(16);
    this.slowState = new Float32Array(16);
    this.triadEnergyFlow = new Float32Array(16);

    this.triadOp = new SkewSymmetricTriadOperator(16, 16);
    this.triadCascade = new SlowFastTriadCascade(16);
    this.jordanField = new ContinuousSpectralJordanField();

    // Prime delay diffusers (13.7 ms and 19.3 ms)
    this.delay1 = new HermiteDelayLine(2048);
    this.delay2 = new HermiteDelayLine(2048);
    this.diffuserFilter1 = new OnePoleLowpass();
    this.diffuserFilter2 = new OnePoleLowpass();
    this.delay1Samples = 658.0;
    this.delay2Samples = 926.0;

    // Lorenz modulations
    this.lorenzFreqMult = new Float32Array(16).fill(1.0);
    this.lorenzQSpread = 1.0;
    this.lorenzPanOffset = 0.0;

    // Golden Ratio Stereo Panning
    this.panL = new Float32Array(16);
    this.panR = new Float32Array(16);
  }

  prepare(sampleRate) {
    this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    this.delay1Samples = clamp(this.sampleRate * 0.0137, 10.0, 2044.0);
    this.delay2Samples = clamp(this.sampleRate * 0.0193, 10.0, 2044.0);
    this.diffuserFilter1.reset();
    this.diffuserFilter1.setCutoff(this.sampleRate, 3600.0);
    this.diffuserFilter2.reset();
    this.diffuserFilter2.setCutoff(this.sampleRate, 3600.0);
    this.reset();
  }

  reset() {
    this.g.fill(0.01);
    this.k.fill(0.1);
    this.a1.fill(1.0);
    this.s1.fill(0.0);
    this.s2.fill(0.0);
    this.qNorm.fill(1.0);
    this.currentFrequencies.fill(100.0);
    this.modalEnergies.fill(0.0);
    this.slowState.fill(0.0);
    this.triadEnergyFlow.fill(0.0);
    this.lorenzFreqMult.fill(1.0);
    this.lorenzQSpread = 1.0;
    this.lorenzPanOffset = 0.0;
    this.delay1.reset();
    this.delay2.reset();
    this.diffuserFilter1.reset();
    this.diffuserFilter2.reset();
    this.triadCascade.reset();
  }

  setLorenzModulation(freqMultipliers, qSpreadFactor, panOffsetRad) {
    for (let i = 0; i < 16; ++i) this.lorenzFreqMult[i] = freqMultipliers[i] ?? 1.0;
    this.lorenzQSpread = clamp(qSpreadFactor, 0.2, 5.0);
    this.lorenzPanOffset = panOffsetRad;
  }

  computeBaseRatios(manifoldIndex, morph) {
    const keys = ['SigmoidHorn', 'ChladniRing', 'GlottalColonic', 'PorcelainBowl'];
    const idx = clamp(Math.floor(manifoldIndex), 0, 3);
    const nextIdx = (idx + 1) % 4;
    const rA = kManifoldRatios[keys[idx]];
    const rB = kManifoldRatios[keys[nextIdx]];
    const m = clamp(morph, 0.0, 1.0);

    const out = new Float32Array(16);
    for (let i = 0; i < 16; ++i) {
      out[i] = rA[i] + m * (rB[i] - rA[i]);
    }
    return out;
  }

  getDampingQ(dampingIndex) {
    let qBase = 20.0, slope = 0.1, exponent = 1.0;
    switch (dampingIndex) {
      case 0: qBase = 15.0;  slope = 0.14;  exponent = 1.15; break; // Mucosal Tissue
      case 1: qBase = 125.0; slope = 0.025; exponent = 0.85; break; // Porcelain
      case 2: qBase = 32.0;  slope = 0.08;  exponent = 1.00; break; // Wet Elastomer
      case 3: qBase = 240.0; slope = 0.008; exponent = 0.70; break; // Bell Bronze
      case 4: qBase = 24.0;  slope = 0.06;  exponent = 0.95; break; // Stiff Cartilage
      default: qBase = 20.0; slope = 0.10;  exponent = 1.00; break;
    }
    const out = new Float32Array(16);
    for (let i = 0; i < 16; ++i) {
      const loss = 1.0 + slope * Math.pow(i, exponent);
      out[i] = Math.max(1.5, qBase / loss);
    }
    return out;
  }

  updateFilterCoefficients(params) {
    const baseRatios = this.computeBaseRatios(params.geometricManifold ?? 2, params.manifoldMorph ?? 0.0);
    const baseQ = this.getDampingQ(params.dampingProfile ?? 0);
    const f0 = clamp(params.fundamentalHz ?? 55.0, 7.0, 600.0);
    const nyquist = this.sampleRate * 0.49;
    const spread = clamp((params.stereoSpread ?? 75.0) * 0.01, 0.0, 1.0);

    for (let i = 0; i < 16; ++i) {
      const fTarget = f0 * baseRatios[i] * this.lorenzFreqMult[i];
      const fClamped = clamp(fTarget, 10.0, nyquist);
      this.currentFrequencies[i] = fClamped;

      const modeSpread = 1.0 + (this.lorenzQSpread - 1.0) * (i - 7.5) / 8.0;
      const qMode = clamp(baseQ[i] * Math.max(0.2, modeSpread), 1.0, 400.0);

      const g = Math.tan(kPi * (fClamped / this.sampleRate));
      const k = 1.0 / qMode;
      this.g[i] = g;
      this.k[i] = k;
      this.a1[i] = 1.0 / (1.0 + g * (g + k));
      this.qNorm[i] = Math.sqrt(28.0 / (1.0 + 0.05 * qMode));

      // Golden Ratio Panning law
      const angle = (i * kGoldenAngleRad + this.lorenzPanOffset) % kTwoPi;
      const pan = clamp(Math.cos(angle) * spread, -1.0, 1.0);
      const normPan = 0.5 * (pan + 1.0);
      this.panL[i] = Math.cos(kHalfPi * normPan);
      this.panR[i] = Math.sin(kHalfPi * normPan);
    }
  }

  processSample(exciterInput, params) {
    this.updateFilterCoefficients(params);

    // 1. Acyclic Delay Diffuser
    const diff1 = this.diffuserFilter1.process(this.delay1.readHermite(this.delay1Samples));
    const diff2 = this.diffuserFilter2.process(this.delay2.readHermite(this.delay2Samples));
    const tailDiff = (diff1 + diff2) * 0.5;
    const xIn = exciterInput + tailDiff * 0.25;

    // 2. 16-Pole Zavalishin TPT SVF Step
    const modeOutputs = new Float32Array(16);
    for (let i = 0; i < 16; ++i) {
      const vHp = this.a1[i] * (xIn - (this.k[i] + this.g[i]) * this.s1[i] - this.s2[i]);
      const vBp = this.g[i] * vHp + this.s1[i];
      const vLp = this.g[i] * vBp + this.s2[i];

      const vSq = vBp * vBp;
      const radDamp = 1.0 / (1.0 + 0.08 * vSq);

      let nextS1 = (2.0 * vBp - this.s1[i]) * radDamp;
      let nextS2 = 2.0 * vLp - this.s2[i];

      if (Math.abs(nextS1) > 2.0) {
        const sgn = nextS1 > 0 ? 1.0 : -1.0;
        nextS1 = sgn * (2.0 + 2.0 * Math.tanh((Math.abs(nextS1) - 2.0) * 0.5));
      }
      if (Math.abs(nextS2) > 2.0) {
        const sgn = nextS2 > 0 ? 1.0 : -1.0;
        nextS2 = sgn * (2.0 + 2.0 * Math.tanh((Math.abs(nextS2) - 2.0) * 0.5));
      }

      this.s1[i] = flushDenormal(clamp(nextS1, -4.0, 4.0));
      this.s2[i] = flushDenormal(clamp(nextS2, -4.0, 4.0));

      const yOut = vBp * this.qNorm[i];
      modeOutputs[i] = flushDenormal(yOut);
      this.modalEnergies[i] = flushDenormal(0.999 * this.modalEnergies[i] + 0.001 * (yOut * yOut));
    }

    // 3. Fast Householder Scattering
    const hhCoupling = clamp(params.householderCoupling ?? 0.45, 0.0, 1.0);
    applyHouseholderScattering16(modeOutputs, hhCoupling);

    let modalSum = 0.0;
    for (let i = 0; i < 16; ++i) modalSum += modeOutputs[i];
    this.delay1.write(modalSum * 0.12);
    this.delay2.write(-modalSum * 0.12);

    // 4. DarkSnail Skew-Symmetric Triad & SlowFastTriadCascade in so(16)
    for (let i = 0; i < 16; ++i) {
      this.slowState[i] = flushDenormal(0.995 * this.slowState[i] + 0.005 * modeOutputs[i]);
    }

    this.triadCascade.cascadeGain = params.triadCouplingGain ?? 0.85;
    this.triadCascade.unrollSteps = params.zenoUnrollK ?? 4;

    let coupledModes = modeOutputs;
    if ((params.triadCouplingGain ?? 0.85) > 0.001) {
      coupledModes = this.triadCascade.processCascade(this.slowState, modeOutputs, this.triadOp, params.zenoUnrollK ?? 4);
      for (let i = 0; i < 16; ++i) {
        this.triadEnergyFlow[i] = flushDenormal(Math.abs(coupledModes[i] - modeOutputs[i]));
      }
    } else {
      this.triadEnergyFlow.fill(0.0);
    }

    // 5. Jordan Cavity Mix
    const cavityMix = clamp((params.porcelainCavityMix ?? 35.0) * 0.01, 0.0, 1.0);
    if (cavityMix > 0.001) {
      this.jordanField.generateCavityField(1.0, 0.65, params.porcelainBoundaryMode ?? 1);
      const cavityModes = this.jordanField.projectTo16Modes();
      for (let i = 0; i < 16; ++i) {
        coupledModes[i] = flushDenormal(coupledModes[i] * (1.0 - cavityMix * 0.35) +
          coupledModes[i] * cavityModes[i] * cavityMix * 0.75);
      }
    }

    // 6. Stereo Panning Accumulation
    let accL = 0.0;
    let accR = 0.0;
    for (let i = 0; i < 16; ++i) {
      accL += coupledModes[i] * this.panL[i];
      accR += coupledModes[i] * this.panR[i];
    }

    return {
      left: flushDenormal(accL * 0.25),
      right: flushDenormal(accR * 0.25)
    };
  }
}

// ============================================================================
// 5. 3D RK4 Continuous Chaotic Lorenz Strange Attractor (DECK 04)
// ============================================================================
export class LorenzAttractor {
  static kSigma = 10.0;
  static kRho   = 28.0;
  static kBeta  = 8.0 / 3.0;

  constructor() {
    this.sampleRate = 48000.0;
    this.rateHz = 0.65;
    this.dt = 0.005;
    this.depth = 0.25;
    this.maxDetuneCents = 45.0;
    this.qModDepth = 0.50;

    this.x = 0.10;
    this.y = 0.0;
    this.z = 0.0;

    this.normX = 0.0;
    this.normY = 0.0;
    this.normZ = 0.0;

    this.freqMultipliers = new Float32Array(16).fill(1.0);
    this.qSpread = 1.0;
    this.panOffsetRad = 0.0;
  }

  prepare(sampleRate) {
    this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    this.reset();
  }

  reset() {
    this.x = 0.10;
    this.y = 0.0;
    this.z = 0.0;
    this.normX = 0.0;
    this.normY = 0.0;
    this.normZ = 0.0;
    this.qSpread = 1.0;
    this.panOffsetRad = 0.0;
    this.freqMultipliers.fill(1.0);
    this.setRateHz(this.rateHz);
    this.updateNormalizedOutputs();
  }

  setRateHz(rateHz) {
    this.rateHz = clamp(rateHz, 0.01, 15.0);
    this.dt = clamp((this.rateHz * 8.0) / this.sampleRate, 1.0e-5, 0.005);
  }

  setDepth(depth) {
    this.depth = clamp(depth, 0.0, 1.0);
  }

  step() {
    const h = this.dt;
    const deriv = (x, y, z) => ({
      dx: LorenzAttractor.kSigma * (y - x),
      dy: x * (LorenzAttractor.kRho - z) - y,
      dz: x * y - LorenzAttractor.kBeta * z
    });

    const k1 = deriv(this.x, this.y, this.z);
    const k2 = deriv(this.x + 0.5 * h * k1.dx, this.y + 0.5 * h * k1.dy, this.z + 0.5 * h * k1.dz);
    const k3 = deriv(this.x + 0.5 * h * k2.dx, this.y + 0.5 * h * k2.dy, this.z + 0.5 * h * k2.dz);
    const k4 = deriv(this.x + h * k3.dx, this.y + h * k3.dy, this.z + h * k3.dz);

    this.x += (h * (1.0 / 6.0)) * (k1.dx + 2.0 * k2.dx + 2.0 * k3.dx + k4.dx);
    this.y += (h * (1.0 / 6.0)) * (k1.dy + 2.0 * k2.dy + 2.0 * k3.dy + k4.dy);
    this.z += (h * (1.0 / 6.0)) * (k1.dz + 2.0 * k2.dz + 2.0 * k3.dz + k4.dz);

    this.x = flushDenormal(this.x);
    this.y = flushDenormal(this.y);
    this.z = flushDenormal(this.z);

    if (!Number.isFinite(this.x) || !Number.isFinite(this.y) || !Number.isFinite(this.z) ||
        Math.abs(this.x) > 100.0 || Math.abs(this.y) > 100.0 || this.z > 150.0 || this.z < -10.0) {
      this.x = 0.10;
      this.y = 0.0;
      this.z = 0.0;
    }

    this.updateNormalizedOutputs();
  }

  updateNormalizedOutputs() {
    this.normX = clamp(this.x * (1.0 / 20.0), -1.0, 1.0);
    this.normY = clamp(this.y * (1.0 / 25.0), -1.0, 1.0);
    this.normZ = clamp(this.z * (1.0 / 50.0),  0.0, 1.0);

    const detuneCents = this.normX * this.depth * this.maxDetuneCents;
    const ratioUp   = Math.pow(2.0, detuneCents * (1.0 / 1200.0));
    const ratioDown = Math.pow(2.0, -detuneCents * (1.0 / 1200.0));

    for (let i = 0; i < 16; ++i) {
      this.freqMultipliers[i] = (i % 2 === 1) ? ratioUp : ratioDown;
    }

    this.qSpread = clamp(1.0 + (this.normZ - 0.5) * this.depth * this.qModDepth * 2.0, 0.2, 5.0);
    this.panOffsetRad = this.normY * this.depth * 0.45;
  }
}

// ============================================================================
// 6. Roland Dimension D Chorus & Analog Mastering Chain (DECK 05)
// ============================================================================
export class MasteringChain {
  constructor() {
    this.sampleRate = 48000.0;
    this.bbd1 = new HermiteDelayLine(4096);
    this.bbd2 = new HermiteDelayLine(4096);
    this.bbd3 = new HermiteDelayLine(4096);

    this.lfoPhase = 0.0;
    this.lfoInc = 0.001;
    this.rateHz = 0.55;
    this.depthMs = 2.2;
    this.baseDelaySamples = 264.0;

    // Compander pre-emphasis (+6 dB above 1.8 kHz)
    this.preShelf = {
      b0: 1.0, b1: 0.0, a1: 0.0, state: 0.0,
      reset() { this.state = 0.0; },
      process(x) {
        const y = this.b0 * x + this.state;
        this.state = flushDenormal(this.b1 * x - this.a1 * y);
        return flushDenormal(y);
      }
    };

    // De-emphasis (-6 dB above 1.8 kHz)
    const makeDeemph = () => ({
      b0: 1.0, b1: 0.0, a1: 0.0, state: 0.0,
      reset() { this.state = 0.0; },
      process(x) {
        const y = this.b0 * x + this.state;
        this.state = flushDenormal(this.b1 * x - this.a1 * y);
        return flushDenormal(y);
      }
    });
    this.deemph1 = makeDeemph();
    this.deemph2 = makeDeemph();
    this.deemph3 = makeDeemph();

    // 2-pole Butterworth 9.5 kHz reconstruction LP
    const makeRecon = () => ({
      b0: 1.0, b1: 0.0, b2: 0.0, a1: 0.0, a2: 0.0, s1: 0.0, s2: 0.0,
      reset() { this.s1 = 0.0; this.s2 = 0.0; },
      process(x) {
        const y = this.b0 * x + this.s1;
        this.s1 = flushDenormal(this.b1 * x - this.a1 * y + this.s2);
        this.s2 = flushDenormal(this.b2 * x - this.a2 * y);
        return flushDenormal(y);
      }
    });
    this.reconLp1 = makeRecon();
    this.reconLp2 = makeRecon();
    this.reconLp3 = makeRecon();

    // 15 Hz 2-pole TPT Highpass DC Blocker
    const makeTpt15Hz = () => ({
      a1: 1.0, g: 0.001, k: 1.41421356, s1: 0.0, s2: 0.0,
      prepare(fs) {
        this.g = Math.tan(kPi * (15.0 / fs));
        this.a1 = 1.0 / (1.0 + this.g * (this.g + this.k));
        this.reset();
      },
      reset() { this.s1 = 0.0; this.s2 = 0.0; },
      process(x) {
        const vHp = this.a1 * (x - (this.k + this.g) * this.s1 - this.s2);
        const vBp = this.g * vHp + this.s1;
        const vLp = this.g * vBp + this.s2;
        this.s1 = flushDenormal(2.0 * vBp - this.s1);
        this.s2 = flushDenormal(2.0 * vLp - this.s2);
        return flushDenormal(vHp);
      }
    });
    this.dcBlockL = makeTpt15Hz();
    this.dcBlockR = makeTpt15Hz();

    // 38 Hz Visceral Sub Resonator
    this.subResonator = {
      a1: 1.0, g: 0.002, k: 0.50, s1: 0.0, s2: 0.0,
      prepare(fs) {
        this.g = Math.tan(kPi * (38.0 / fs));
        this.a1 = 1.0 / (1.0 + this.g * (this.g + this.k));
        this.reset();
      },
      reset() { this.s1 = 0.0; this.s2 = 0.0; },
      process(x) {
        const vHp = this.a1 * (x - (this.k + this.g) * this.s1 - this.s2);
        const vBp = this.g * vHp + this.s1;
        const vLp = this.g * vBp + this.s2;
        this.s1 = flushDenormal(2.0 * vBp - this.s1);
        this.s2 = flushDenormal(2.0 * vLp - this.s2);
        return flushDenormal(vLp + vBp * 0.5);
      }
    };

    this.peakHold = 0.0;
    this.rmsAcc = 0.0;
    this.peakLevelDb = -100.0;
    this.rmsLevelDb = -100.0;
  }

  prepare(sampleRate) {
    this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    this.dcBlockL.prepare(this.sampleRate);
    this.dcBlockR.prepare(this.sampleRate);
    this.subResonator.prepare(this.sampleRate);

    this.baseDelaySamples = clamp(this.sampleRate * 0.0055, 10.0, 3500.0);

    const f1 = 1800.0, f2 = 3600.0;
    const tanW1 = Math.tan(kTwoPi * (f1 / this.sampleRate) * 0.5);
    const tanW2 = Math.tan(kTwoPi * (f2 / this.sampleRate) * 0.5);

    const preDenom = 1.0 + tanW2;
    this.preShelf.b0 = (1.0 + tanW1 * 2.0) / preDenom;
    this.preShelf.b1 = (-1.0 + tanW1 * 2.0) / preDenom;
    this.preShelf.a1 = (-1.0 + tanW2) / preDenom;

    const deDenom = 1.0 + tanW1 * 2.0;
    const deB0 = (1.0 + tanW2) / deDenom;
    const deB1 = (-1.0 + tanW2) / deDenom;
    const deA1 = (-1.0 + tanW1 * 2.0) / deDenom;

    this.deemph1.b0 = deB0; this.deemph1.b1 = deB1; this.deemph1.a1 = deA1;
    this.deemph2.b0 = deB0; this.deemph2.b1 = deB1; this.deemph2.a1 = deA1;
    this.deemph3.b0 = deB0; this.deemph3.b1 = deB1; this.deemph3.a1 = deA1;

    const fcRecon = Math.min(9500.0, this.sampleRate * 0.45);
    const c = Math.tan(kPi * (fcRecon / this.sampleRate));
    const cSq = c * c;
    const reconDenom = 1.0 + kSqrt2 * c + cSq;

    const rcB0 = cSq / reconDenom;
    const rcB1 = 2.0 * cSq / reconDenom;
    const rcB2 = cSq / reconDenom;
    const rcA1 = (2.0 * (cSq - 1.0)) / reconDenom;
    const rcA2 = (1.0 - kSqrt2 * c + cSq) / reconDenom;

    this.reconLp1.b0 = rcB0; this.reconLp1.b1 = rcB1; this.reconLp1.b2 = rcB2;
    this.reconLp1.a1 = rcA1; this.reconLp1.a2 = rcA2;
    this.reconLp2.b0 = rcB0; this.reconLp2.b1 = rcB1; this.reconLp2.b2 = rcB2;
    this.reconLp2.a1 = rcA1; this.reconLp2.a2 = rcA2;
    this.reconLp3.b0 = rcB0; this.reconLp3.b1 = rcB1; this.reconLp3.b2 = rcB2;
    this.reconLp3.a1 = rcA1; this.reconLp3.a2 = rcA2;

    this.reset();
  }

  reset() {
    this.bbd1.reset();
    this.bbd2.reset();
    this.bbd3.reset();
    this.lfoPhase = 0.0;
    this.preShelf.reset();
    this.reconLp1.reset(); this.reconLp2.reset(); this.reconLp3.reset();
    this.deemph1.reset();  this.deemph2.reset();  this.deemph3.reset();
    this.dcBlockL.reset();
    this.dcBlockR.reset();
    this.subResonator.reset();
    this.peakHold = 0.0;
    this.rmsAcc = 0.0;
    this.peakLevelDb = -100.0;
    this.rmsLevelDb = -100.0;
    this.updateChorusSettings(1);
  }

  updateChorusSettings(mode) {
    switch (mode) {
      case 0: this.rateHz = 0.40; this.depthMs = 1.5; break; // Mode I
      case 1: this.rateHz = 0.55; this.depthMs = 2.2; break; // Mode II
      case 2: this.rateHz = 0.75; this.depthMs = 3.2; break; // Mode III
      case 3: this.rateHz = 1.10; this.depthMs = 4.5; break; // Mode IV
      default: this.rateHz = 0.55; this.depthMs = 2.2; break;
    }
    this.lfoInc = (this.rateHz * kTwoPi) / this.sampleRate;
  }

  process(inL, inR, params) {
    this.updateChorusSettings(params.dimensionDMode ?? 1);

    // 1. 15 Hz DC Blocker
    let sigL = this.dcBlockL.process(inL);
    let sigR = this.dcBlockR.process(inR);

    // 2. Roland Dimension D Tri-Phase BBD Chorus
    const chorusEnable = (params.dimensionDEnable ?? 1) >= 0.5;
    const chorusMix = clamp((params.dimensionDMix ?? 40.0) * 0.01, 0.0, 1.0);

    if (chorusEnable && chorusMix > 0.001) {
      const monoIn = (sigL + sigR) * 0.5;
      const compIn = this.preShelf.process(monoIn);

      this.bbd1.write(compIn);
      this.bbd2.write(compIn);
      this.bbd3.write(compIn);

      const depthSamples = (this.depthMs * 0.001) * this.sampleRate;
      const kPhase120 = kTwoPi * (1.0 / 3.0);
      const kPhase240 = kTwoPi * (2.0 / 3.0);

      const mod1 = FastSinTable.sin(this.lfoPhase);
      const mod2 = FastSinTable.sin(this.lfoPhase + kPhase120);
      const mod3 = FastSinTable.sin(this.lfoPhase + kPhase240);

      const del1 = clamp(this.baseDelaySamples + depthSamples * mod1, 10.0, 4090.0);
      const del2 = clamp(this.baseDelaySamples + depthSamples * mod2, 10.0, 4090.0);
      const del3 = clamp(this.baseDelaySamples + depthSamples * mod3, 10.0, 4090.0);

      const raw1 = this.bbd1.readHermite(del1);
      const raw2 = this.bbd2.readHermite(del2);
      const raw3 = this.bbd3.readHermite(del3);

      const tap1 = this.deemph1.process(this.reconLp1.process(raw1));
      const tap2 = this.deemph2.process(this.reconLp2.process(raw2));
      const tap3 = this.deemph3.process(this.reconLp3.process(raw3));

      // Dimension D spatial matrix
      const rawWetL = 0.5 * (tap1 - 0.5 * tap2 + 0.5 * tap3);
      const rawWetR = 0.5 * (-0.5 * tap1 + tap2 + 0.5 * tap3);

      const mid = 0.5 * (rawWetL + rawWetR);
      const side = 0.5 * (rawWetL - rawWetR);
      const spread = clamp(params.dimensionDSpread ?? 1.0, 0.0, 1.5);
      const wetL = mid + spread * side;
      const wetR = mid - spread * side;

      sigL = (1.0 - chorusMix) * sigL + chorusMix * wetL;
      sigR = (1.0 - chorusMix) * sigR + chorusMix * wetR;

      this.lfoPhase += this.lfoInc;
      if (this.lfoPhase >= kTwoPi) this.lfoPhase -= kTwoPi;
    }

    // 3. Phase-Locked Visceral Sub-Bass Reinforcement
    const subDb = params.visceralSubReinforceDb ?? 0.0;
    if (subDb > -35.0) {
      const subDrive = (sigL + sigR) * 0.5;
      const subWave = this.subResonator.process(subDrive);
      const subGain = dbToGain(subDb);
      const subSignal = subWave * subGain * 0.45;
      sigL += subSignal;
      sigR += subSignal;
    }

    // 4. Colonic Saturation Drive & Hermite C1 Soft-Knee BoundedSaturator
    const driveGain = 1.0 + 3.0 * clamp((params.colonicSaturationDrive ?? 30.0) * 0.01, 0.0, 1.0);
    sigL *= driveGain;
    sigR *= driveGain;

    const hermiteSaturate = (x) => {
      if (!Number.isFinite(x)) return 0.0;
      const absX = Math.abs(x);
      const k = 0.72;
      const C = 1.00;
      if (absX <= k) return x;
      const sgn = x > 0 ? 1.0 : -1.0;
      if (absX >= C) return sgn * C;
      const u = (absX - k) / (C - k);
      const poly = u * (1.0 + u * (1.0 - u));
      return sgn * (k + (C - k) * poly);
    };

    sigL = hermiteSaturate(sigL);
    sigR = hermiteSaturate(sigR);

    // 5. Master Output Trim
    const masterGain = dbToGain(params.masterVolumeDb ?? 0.0);
    sigL *= masterGain;
    sigR *= masterGain;

    // 6. True-Peak Brickwall Limiter (<= 1.05)
    sigL = flushDenormal(clamp(sigL, -1.05, 1.05));
    sigR = flushDenormal(clamp(sigR, -1.05, 1.05));

    // Telemetry ballistics
    const samplePeak = Math.max(Math.abs(sigL), Math.abs(sigR));
    this.peakHold = Math.max(samplePeak, this.peakHold * 0.9995);
    this.rmsAcc = 0.999 * this.rmsAcc + 0.001 * (0.5 * (sigL * sigL + sigR * sigR));
    this.peakLevelDb = gainToDb(this.peakHold);
    this.rmsLevelDb  = gainToDb(Math.sqrt(Math.max(1.0e-12, this.rmsAcc)));

    return { left: sigL, right: sigR };
  }
}

// ============================================================================
// 7. Physical Synthesizer Voice (`PhysicalVoice`)
// ============================================================================
export class PhysicalVoice {
  constructor() {
    this.sampleRate = 48000.0;
    this.active = false;
    this.gateState = 'Off'; // 'Off', 'NoteOn', 'Release'
    this.midiNote = -1;
    this.velocity = 0.0;
    this.onsetTime = 0;

    this.currentFreqHz = 55.0;
    this.targetFreqHz = 55.0;
    this.targetMidiNote = 33.0;
    this.pitchBendOffsetSemitones = 0.0;
    this.glideTimeMs = 45.0;
    this.glideCoeff = 1.0;

    this.envelope = 0.0;
    this.attackCoeff = 0.05;
    this.releaseCoeff = 0.999;

    this.stealingFade = false;
    this.stealFadeGain = 1.0;
    this.stealFadeStep = 0.005;

    this.mucosalExciter = new MucosalExciter();
    this.aeroFluidEngine = new AeroFluidEngine();
    this.modalMatrix = new ModalResonatorMatrix();

    this.params = {};
  }

  prepare(sampleRate) {
    this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    this.mucosalExciter.prepare(this.sampleRate);
    this.aeroFluidEngine.prepare(this.sampleRate);
    this.modalMatrix.prepare(this.sampleRate);

    this.attackCoeff = 1.0 - Math.exp(-1.0 / (0.002 * this.sampleRate));
    this.stealFadeStep = 1.0 / (0.005 * this.sampleRate);
    this.updateFrequencyGlide();
    this.reset();
  }

  reset() {
    this.active = false;
    this.gateState = 'Off';
    this.midiNote = -1;
    this.velocity = 0.0;
    this.onsetTime = 0;
    this.envelope = 0.0;
    this.stealingFade = false;
    this.stealFadeGain = 1.0;
    this.mucosalExciter.reset();
    this.aeroFluidEngine.reset();
    this.modalMatrix.reset();
  }

  updateFrequencyGlide() {
    if (this.glideTimeMs <= 0.5) {
      this.glideCoeff = 1.0;
    } else {
      this.glideCoeff = 1.0 - Math.exp(-1.0 / (this.glideTimeMs * 0.001 * this.sampleRate));
    }
  }

  setTargetPitchMidi(midiNote, immediate = false) {
    this.targetMidiNote = midiNote;
    const effectiveNote = this.targetMidiNote + this.pitchBendOffsetSemitones;
    this.targetFreqHz = clamp(midiNoteToHz(effectiveNote), 7.0, 420.0);
    if (immediate) {
      this.currentFreqHz = this.targetFreqHz;
    }
  }

  noteOn(midiNote, velocity) {
    if (velocity <= 0.001) {
      this.noteOff(0.0);
      return;
    }

    this.midiNote = midiNote;
    this.velocity = clamp(velocity, 0.0, 1.0);
    this.setTargetPitchMidi(midiNote, !this.active);

    this.mucosalExciter.triggerSlap(this.velocity, this.params.tissueHardness ?? 2.1);
    if (this.velocity > 0.60) {
      this.mucosalExciter.triggerSealBurst(this.velocity);
    }
    this.aeroFluidEngine.triggerFlapBurst(
      this.velocity,
      this.params.multiphaseMoisture ?? 0.35,
      this.params.poissonDropletRate ?? 8.5
    );

    this.gateState = 'NoteOn';
    this.active = true;
    this.stealingFade = false;
    this.stealFadeGain = 1.0;
  }

  noteOff() {
    this.gateState = 'Release';
    const tauS = Math.max(0.02, (this.params.vactrolSagTauMs ?? 85.0) * 0.001);
    this.releaseCoeff = Math.exp(-1.0 / (tauS * this.sampleRate));
  }

  killQuick() {
    this.stealingFade = true;
    this.stealFadeGain = 1.0;
  }

  processSample(externalPressureMod = 0.0) {
    if (!this.active) return { left: 0.0, right: 0.0 };

    // 1. Portamento glide
    this.currentFreqHz += this.glideCoeff * (this.targetFreqHz - this.currentFreqHz);
    this.currentFreqHz = flushDenormal(this.currentFreqHz);

    // 2. Dynamic colonic pressure
    const localExciterParams = { ...this.params, frequencyHz: this.currentFreqHz };
    const velocityScale = 0.35 + 0.65 * this.velocity;
    localExciterParams.colonicPressure = clamp(
      (this.params.colonicPressure ?? 0.70) * velocityScale + externalPressureMod,
      0.0, 1.0
    );

    // 3. Mucosal Valve Excitation
    const exciterSig = this.mucosalExciter.processSample(localExciterParams);
    const airVelocity = this.mucosalExciter.airVelocity;
    const aperture = this.mucosalExciter.displacement;

    // 4. Multiphase AeroFluid Flow
    const fluidSig = this.aeroFluidEngine.processSample(airVelocity, aperture, 0.0, this.params);

    // 5. 16-Pole Modal Resonator Matrix
    const totalExcitation = exciterSig + fluidSig;
    const modalParams = { ...this.params, fundamentalHz: this.currentFreqHz };
    const stereoOut = this.modalMatrix.processSample(totalExcitation, modalParams);

    // 6. Local Vactrol Envelope Gate
    if (this.gateState === 'NoteOn') {
      this.envelope += this.attackCoeff * (1.0 - this.envelope);
    } else if (this.gateState === 'Release') {
      this.envelope *= this.releaseCoeff;
      this.envelope = flushDenormal(this.envelope);
      if (this.envelope < 1.0e-4) {
        this.envelope = 0.0;
        this.active = false;
        this.gateState = 'Off';
      }
    }

    let outL = stereoOut.left * this.envelope;
    let outR = stereoOut.right * this.envelope;

    // 7. Steal Crossfade
    if (this.stealingFade) {
      this.stealFadeGain -= this.stealFadeStep;
      if (this.stealFadeGain <= 0.0) {
        this.stealFadeGain = 0.0;
        this.stealingFade = false;
        this.active = false;
        this.gateState = 'Off';
      }
      outL *= this.stealFadeGain;
      outR *= this.stealFadeGain;
    }

    return {
      left: flushDenormal(outL),
      right: flushDenormal(outR)
    };
  }
}

// ============================================================================
// 8. Voice Allocator & Voicing Manager (`VoiceManager`)
// ============================================================================
export class VoiceManager {
  constructor(maxVoices = 8) {
    this.maxVoices = maxVoices;
    this.sampleRate = 48000.0;
    this.voicingMode = 0; // 0: MonoLegato, 1: Poly8, 2: StereoUnison
    this.glideTimeMs = 45.0;
    this.pitchBendSemitones = 0.0;
    this.clock = 0;

    this.voices = Array.from({ length: maxVoices }, () => new PhysicalVoice());
    this.noteStack = []; // For mono legato note priority
    this.parameters = {};

    this.unisonDetuneCents = [-14.0, -7.0, +7.0, +14.0];
    this.unisonPanOffsets = [-0.80, -0.30, +0.30, +0.80];
  }

  prepare(sampleRate) {
    this.sampleRate = sampleRate;
    for (const v of this.voices) v.prepare(sampleRate);
    this.reset();
  }

  reset() {
    for (const v of this.voices) v.reset();
    this.noteStack = [];
    this.clock = 0;
  }

  setVoicingMode(mode) {
    this.voicingMode = mode;
  }

  setPortamentoGlideMs(glideMs) {
    this.glideTimeMs = glideMs;
    for (const v of this.voices) {
      v.glideTimeMs = glideMs;
      v.updateFrequencyGlide();
    }
  }

  setPitchBend(semitones) {
    this.pitchBendSemitones = semitones;
    for (const v of this.voices) {
      v.pitchBendOffsetSemitones = semitones;
      v.setTargetPitchMidi(v.targetMidiNote, false);
    }
  }

  setParameter(key, val) {
    this.parameters[key] = val;
    for (const v of this.voices) {
      v.params[key] = val;
    }
  }

  setLorenzModulation(freqMultipliers, qSpreadFactor, panOffsetRad) {
    for (const v of this.voices) {
      v.modalMatrix.setLorenzModulation(freqMultipliers, qSpreadFactor, panOffsetRad);
    }
  }

  noteOn(midiNote, velocity) {
    ++this.clock;
    if (this.voicingMode === 0) {
      // Mono Legato
      const isLegato = this.voices[0].active;
      this.noteStack.push({ midiNote, velocity });
      const v = this.voices[0];
      v.onsetTime = this.clock;
      v.setTargetPitchMidi(midiNote, !isLegato);
      if (!isLegato) {
        v.noteOn(midiNote, velocity);
      }
    } else if (this.voicingMode === 1) {
      // 8-Voice Polyphonic
      let freeIdx = this.voices.findIndex(v => !v.active);
      if (freeIdx === -1) {
        // Steal oldest / lowest velocity voice
        let oldestClock = Infinity;
        let lowestVel = 2.0;
        freeIdx = 0;
        for (let i = 0; i < this.maxVoices; ++i) {
          if (this.voices[i].onsetTime < oldestClock || this.voices[i].velocity < lowestVel) {
            oldestClock = this.voices[i].onsetTime;
            lowestVel = this.voices[i].velocity;
            freeIdx = i;
          }
        }
        this.voices[freeIdx].killQuick();
      }
      const v = this.voices[freeIdx];
      v.onsetTime = this.clock;
      v.noteOn(midiNote, velocity);
    } else if (this.voicingMode === 2) {
      // Stereo Unison (4 voices)
      for (let i = 0; i < 4; ++i) {
        const v = this.voices[i];
        v.onsetTime = this.clock;
        const detuneSemi = this.unisonDetuneCents[i] / 100.0;
        v.pitchBendOffsetSemitones = this.pitchBendSemitones + detuneSemi;
        v.noteOn(midiNote, velocity);
      }
    }
  }

  noteOff(midiNote) {
    if (this.voicingMode === 0) {
      this.noteStack = this.noteStack.filter(n => n.midiNote !== midiNote);
      if (this.noteStack.length > 0) {
        const prev = this.noteStack[this.noteStack.length - 1];
        this.voices[0].setTargetPitchMidi(prev.midiNote, false);
      } else {
        this.voices[0].noteOff();
      }
    } else if (this.voicingMode === 1) {
      for (const v of this.voices) {
        if (v.active && v.midiNote === midiNote) {
          v.noteOff();
        }
      }
    } else if (this.voicingMode === 2) {
      for (let i = 0; i < 4; ++i) {
        this.voices[i].noteOff();
      }
    }
  }

  allNotesOff() {
    this.noteStack = [];
    for (const v of this.voices) v.noteOff();
  }

  processSample() {
    let sumL = 0.0;
    let sumR = 0.0;
    for (const v of this.voices) {
      if (!v.active) continue;
      const s = v.processSample();
      sumL += s.left;
      sumR += s.right;
    }
    return { left: sumL, right: sumR };
  }

  getActiveAperture() {
    for (const v of this.voices) {
      if (v.active) return v.mucosalExciter.displacement;
    }
    return this.voices[0].mucosalExciter.displacement;
  }

  getActivePressure() {
    for (const v of this.voices) {
      if (v.active) return v.mucosalExciter.upstreamPressure;
    }
    return this.voices[0].mucosalExciter.upstreamPressure;
  }

  getActiveAirVelocity() {
    for (const v of this.voices) {
      if (v.active) return v.mucosalExciter.airVelocity;
    }
    return this.voices[0].mucosalExciter.airVelocity;
  }

  getActiveBubbleIntensity() {
    for (const v of this.voices) {
      if (v.active) return v.aeroFluidEngine.bubbleActivity;
    }
    return 0.0;
  }

  wasDropletTriggered() {
    for (const v of this.voices) {
      if (v.aeroFluidEngine.dropletTriggered) return true;
    }
    return false;
  }

  getModalEnergies() {
    for (const v of this.voices) {
      if (v.active) return v.modalMatrix.modalEnergies;
    }
    return this.voices[0].modalMatrix.modalEnergies;
  }

  getTriadEnergyFlow() {
    for (const v of this.voices) {
      if (v.active) return v.modalMatrix.triadEnergyFlow;
    }
    return this.voices[0].modalMatrix.triadEnergyFlow;
  }

  getLastRadialDrift() {
    for (const v of this.voices) {
      if (v.active) return v.modalMatrix.triadCascade.lastRadialDrift;
    }
    return 0.0;
  }
}

// ============================================================================
// 9. Master Procedural DSP Engine (`Pf02DspEngine`)
// ============================================================================
export class Pf02DspEngine {
  constructor(sampleRate = 48000.0) {
    this.sampleRate = sampleRate;
    this.powerActive = true;

    this.voiceManager = new VoiceManager(8);
    this.lorenzAttractor = new LorenzAttractor();
    this.masteringChain = new MasteringChain();

    // 40 Parameters storage (ID -> Value)
    this.parameters = {};
    this.initDefaultParameters();

    // Telemetry decimation (60 Hz)
    this.telemetryIntervalSamples = Math.max(64, Math.floor(sampleRate / 60.0));
    this.telemetrySampleCounter = 0;
    this.waveformWindowL = new Float32Array(128);
    this.waveformWindowR = new Float32Array(128);
    this.waveformPos = 0;

    this.latestTelemetry = null;
    this.prepare(sampleRate);
  }

  initDefaultParameters() {
    // 40 Canonical Parameters matching Survey 3 & verify.mjs
    const defaults = {
      param_colonic_pressure: 0.70,
      param_mucosal_tension: 0.50,
      param_resting_aperture: 1.0,
      param_karnopp_stiction: 0.65,
      param_stribeck_breakaway: 2.2,
      param_tissue_hardness: 2.1,
      param_impact_restitution: 0.45,
      param_vactrol_sag_tau: 85.0,
      param_beltrami_vortex_pitch: 2.8,
      param_reynolds_turbulence: 0.40,
      param_multiphase_moisture: 0.35,
      param_fluid_viscosity: 0.30,
      param_minnaert_bubble_density: 40.0,
      param_poisson_droplet_rate: 8.5,
      param_euclidean_peristalsis_k: 5,
      param_euclidean_peristalsis_n: 16,
      param_triad_coupling_gain: 0.85,
      param_so16_skew_generator: 0.50,
      param_spherical_retraction: 1,
      param_householder_coupling: 0.45,
      param_geometric_manifold: 2,
      param_damping_profile: 0,
      param_porcelain_cavity_mix: 35.0,
      param_porcelain_boundary_mode: 1,
      param_zeno_unroll_override: 0,
      param_zeno_conflict_threshold: 0.55,
      param_lorenz_orbit_rate: 0.65,
      param_lorenz_chaos_depth: 25.0,
      param_dimension_d_enable: 1,
      param_dimension_d_mode: 1,
      param_dimension_d_mix: 40.0,
      param_colonic_saturation_drive: 30.0,
      param_visceral_sub_reinforce: 0.0,
      param_golden_stereo_spread: 75.0,
      param_voicing_mode: 0,
      param_glide_portamento_ms: 45.0,
      param_master_volume_db: 0.0,
      param_power_active: 1,
      macro_gut_squeeze: 0.50,
      macro_fluid_moisture: 0.50
    };

    for (const [k, v] of Object.entries(defaults)) {
      this.setParameter(k, v);
    }
  }

  prepare(sampleRate) {
    this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    this.voiceManager.prepare(this.sampleRate);
    this.lorenzAttractor.prepare(this.sampleRate);
    this.masteringChain.prepare(this.sampleRate);
    this.telemetryIntervalSamples = Math.max(64, Math.floor(this.sampleRate / 60.0));
    this.telemetrySampleCounter = 0;
    this.waveformPos = 0;
    this.waveformWindowL.fill(0.0);
    this.waveformWindowR.fill(0.0);
  }

  reset() {
    this.voiceManager.reset();
    this.lorenzAttractor.reset();
    this.masteringChain.reset();
  }

  noteOn(midiNote, velocity) {
    this.voiceManager.noteOn(midiNote, velocity);
  }

  noteOff(midiNote) {
    this.voiceManager.noteOff(midiNote);
  }

  pitchBend(semitones) {
    this.voiceManager.setPitchBend(semitones);
  }

  allNotesOff() {
    this.voiceManager.allNotesOff();
  }

  setParameter(key, val) {
    this.parameters[key] = val;

    switch (key) {
      case 'param_power_active':
        this.powerActive = val >= 0.5;
        break;
      case 'param_voicing_mode':
        this.voiceManager.setVoicingMode(Math.round(val));
        break;
      case 'param_glide_portamento_ms':
        this.voiceManager.setPortamentoGlideMs(val);
        break;
      case 'param_lorenz_orbit_rate':
        this.lorenzAttractor.setRateHz(val);
        break;
      case 'param_lorenz_chaos_depth':
        this.lorenzAttractor.setDepth(val * 0.01);
        break;
      case 'macro_gut_squeeze':
        this.applyMacroSqueeze(val);
        break;
      case 'macro_fluid_moisture':
        this.applyMacroMoisture(val);
        break;
      default:
        this.voiceManager.setParameter(this._paramKeyToSubsystem(key), val);
        break;
    }
  }

  _paramKeyToSubsystem(key) {
    const map = {
      param_colonic_pressure: 'colonicPressure',
      param_mucosal_tension: 'mucosalTension',
      param_resting_aperture: 'restingApertureMm',
      param_karnopp_stiction: 'karnoppStiction',
      param_stribeck_breakaway: 'stribeckBreakaway',
      param_tissue_hardness: 'tissueHardness',
      param_impact_restitution: 'impactRestitution',
      param_vactrol_sag_tau: 'vactrolSagTauMs',
      param_beltrami_vortex_pitch: 'beltramiVortexPitch',
      param_reynolds_turbulence: 'reynoldsTurbulence',
      param_multiphase_moisture: 'multiphaseMoisture',
      param_fluid_viscosity: 'fluidViscosity',
      param_minnaert_bubble_density: 'minnaertBubbleDensity',
      param_poisson_droplet_rate: 'poissonDropletRate',
      param_euclidean_peristalsis_k: 'euclideanPeristalsisK',
      param_euclidean_peristalsis_n: 'euclideanPeristalsisN',
      param_triad_coupling_gain: 'triadCouplingGain',
      param_so16_skew_generator: 'so16SkewGenerator',
      param_spherical_retraction: 'sphericalRetraction',
      param_householder_coupling: 'householderCoupling',
      param_geometric_manifold: 'geometricManifold',
      param_damping_profile: 'dampingProfile',
      param_porcelain_cavity_mix: 'porcelainCavityMix',
      param_porcelain_boundary_mode: 'porcelainBoundaryMode',
      param_zeno_unroll_override: 'zenoUnrollK',
      param_golden_stereo_spread: 'stereoSpread',
      param_dimension_d_enable: 'dimensionDEnable',
      param_dimension_d_mode: 'dimensionDMode',
      param_dimension_d_mix: 'dimensionDMix',
      param_colonic_saturation_drive: 'colonicSaturationDrive',
      param_visceral_sub_reinforce: 'visceralSubReinforceDb',
      param_master_volume_db: 'masterVolumeDb'
    };
    return map[key] || key;
  }

  applyMacroSqueeze(squeeze) {
    const delta = (squeeze - 0.50) * 2.0;
    const baseP = this.parameters.param_colonic_pressure ?? 0.70;
    const baseT = this.parameters.param_mucosal_tension ?? 0.50;
    const baseD = this.parameters.param_colonic_saturation_drive ?? 30.0;
    this.voiceManager.setParameter('colonicPressure', clamp(baseP + delta * 0.25, 0.0, 1.0));
    this.voiceManager.setParameter('mucosalTension', clamp(baseT + delta * 0.20, 0.0, 1.0));
    this.voiceManager.setParameter('colonicSaturationDrive', clamp(baseD + delta * 25.0, 0.0, 100.0));
  }

  applyMacroMoisture(moisture) {
    const delta = (moisture - 0.50) * 2.0;
    const baseM = this.parameters.param_multiphase_moisture ?? 0.35;
    const baseB = this.parameters.param_minnaert_bubble_density ?? 40.0;
    const baseD = this.parameters.param_poisson_droplet_rate ?? 8.5;
    this.voiceManager.setParameter('multiphaseMoisture', clamp(baseM + delta * 0.35, 0.0, 1.0));
    this.voiceManager.setParameter('minnaertBubbleDensity', clamp(baseB + delta * 30.0, 0.0, 100.0));
    this.voiceManager.setParameter('poissonDropletRate', clamp(baseD + delta * 15.0, 0.0, 60.0));
  }

  processBlock(numSamples) {
    const outL = new Float32Array(numSamples);
    const outR = new Float32Array(numSamples);

    if (!this.powerActive) {
      return { left: outL, right: outR };
    }

    // Step Lorenz Attractor and distribute modulations
    this.lorenzAttractor.step();
    this.voiceManager.setLorenzModulation(
      this.lorenzAttractor.freqMultipliers,
      this.lorenzAttractor.qSpread,
      this.lorenzAttractor.panOffsetRad
    );

    const masterParams = {
      dimensionDEnable: (this.parameters.param_dimension_d_enable ?? 1) >= 0.5,
      dimensionDMode: Math.round(this.parameters.param_dimension_d_mode ?? 1),
      dimensionDMix: this.parameters.param_dimension_d_mix ?? 40.0,
      dimensionDSpread: 1.0,
      colonicSaturationDrive: this.parameters.param_colonic_saturation_drive ?? 30.0,
      visceralSubReinforceDb: this.parameters.param_visceral_sub_reinforce ?? 0.0,
      masterVolumeDb: this.parameters.param_master_volume_db ?? 0.0
    };

    for (let i = 0; i < numSamples; ++i) {
      const v = this.voiceManager.processSample();
      const m = this.masteringChain.process(v.left, v.right, masterParams);

      outL[i] = m.left;
      outR[i] = m.right;

      // Telemetry rolling window
      this.waveformWindowL[this.waveformPos] = m.left;
      this.waveformWindowR[this.waveformPos] = m.right;
      this.waveformPos = (this.waveformPos + 1) & 127;

      ++this.telemetrySampleCounter;
      if (this.telemetrySampleCounter >= this.telemetryIntervalSamples) {
        this.telemetrySampleCounter = 0;
        this.updateTelemetryFrame();
      }
    }

    return { left: outL, right: outR };
  }

  updateTelemetryFrame() {
    const waveL = new Float32Array(128);
    const waveR = new Float32Array(128);
    for (let i = 0; i < 128; ++i) {
      const idx = (this.waveformPos + i) & 127;
      waveL[i] = this.waveformWindowL[idx];
      waveR[i] = this.waveformWindowR[idx];
    }

    const aperture = this.voiceManager.getActiveAperture();
    const airVelocity = this.voiceManager.getActiveAirVelocity();
    const zenoFlux = clamp(Math.abs(airVelocity) * 0.05, 0.0, 1.0);

    let activeK = 4;
    const override = Math.round(this.parameters.param_zeno_unroll_override ?? 0);
    if (override === 1) activeK = 1;
    else if (override === 2) activeK = 4;
    else if (override === 3) activeK = 8;
    else if (override === 4) activeK = 16;
    else {
      if (zenoFlux > 0.70) activeK = 16;
      else if (zenoFlux > 0.45) activeK = 8;
      else if (zenoFlux > 0.20) activeK = 4;
      else activeK = 1;
    }

    this.latestTelemetry = {
      waveformL: waveL,
      waveformR: waveR,
      aperture: aperture,
      colonicPressure: this.voiceManager.getActivePressure(),
      airVelocity: airVelocity,
      bubbleIntensity: this.voiceManager.getActiveBubbleIntensity(),
      dropletTriggered: this.voiceManager.wasDropletTriggered(),
      zenoFlux: zenoFlux,
      activeUnrollK: activeK,
      lorenzX: this.lorenzAttractor.normX,
      lorenzY: this.lorenzAttractor.normY,
      lorenzZ: this.lorenzAttractor.normZ,
      modalEnergies: new Float32Array(this.voiceManager.getModalEnergies()),
      triadEnergyFlow: new Float32Array(this.voiceManager.getTriadEnergyFlow()),
      sphericalNormDrift: this.voiceManager.getLastRadialDrift(),
      peakLevelDb: this.masteringChain.peakLevelDb,
      rmsLevelDb: this.masteringChain.rmsLevelDb
    };
  }

  getTelemetry() {
    if (!this.latestTelemetry) {
      this.updateTelemetryFrame();
    }
    return this.latestTelemetry;
  }
}
