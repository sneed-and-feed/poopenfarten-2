# TEST_INFRA.md: E2E Testing Infrastructure & Quality Assurance Architecture
**PF-02 · Darm-Akustischer & Ultrametrischer Triaden-Synthesizer**

---

## 1. Executive Summary & Verification Philosophy

Poopenfarten II (`PF-02`) is a first-principles procedural physical-modeling aeroacoustic synthesizer combining Project DarkSnail's continuous mathematical Lie-algebraic primitives with viscoelastic contact mechanics, a 16-pole modal resonator matrix, and German functionalist laboratory industrial design.

To guarantee zero regression, total synthesis purity (zero static PCM tables or sample bank shortcuts), hard real-time safety, and mathematical rigor, the verification architecture establishes a strict **4-Tier Independent Opaque-Box Testing Track** alongside an automated **Zero-PCM Source Audit**.

### Core Guarantees Verified
1. **100% Procedural Synthesis**: Automated scanning ensures 0 bytes of static audio sample tables (`SampleBank.cpp` style), 0 hardcoded float audio arrays, and 0 Base64-encoded PCM strings.
2. **Mathematical Invariant Rigor**: Strict numerical tolerances enforced for skew-symmetric triad antisymmetry ($< 1\text{e}-7$), tangent orthogonality ($< 1\text{e}-6$), spherical norm conservation ($< 1\text{e}-6$), and $O(N)$ Householder scattering isometry ($< 1\text{e}-6$).
3. **Hard Real-Time Audio-Thread Safety**: Guaranteed zero heap allocations (`operator new/delete` assertion guard), zero unbounded latency, and bitwise branchless denormal flushing across all rendering blocks.
4. **Mono Cancellation Immunity**: Mathematical proof and empirical verification that Roland Dimension D tri-phase chorus ($0^\circ, 120^\circ, 240^\circ$) preserves non-zero acoustic energy under mono-sum ($\lvert\text{Wet}_{\text{mono}}\rvert > 0$).
5. **Numerical Stability Under Chaos**: 100,000+ continuous sample blocks tested under $+40\text{ dBFS}$ overdrive bursts and rapid parameter churn with strictly zero NaNs, infinities, or blowups.

---

## 2. 4-Tier Test Architecture & Methodology

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                           POOPENFARTEN II TEST TRACK                          │
├───────────────────────┬───────────────────────────────────────────────────────┤
│ TIER 1: FEATURES      │ Functional correctness of all 47 features (≥ 5/feat)  │
│ TIER 2: BOUNDARIES    │ Multirate (44.1-192 kHz), buffers (1-2048), sweeps    │
│ TIER 3: INVARIANTS    │ DarkSnail Lie algebra, spherical norm, Householder    │
│ TIER 4: STRESS & PROD │ 16 presets, polyphony, zero allocations, +40 dB burst │
├───────────────────────┴───────────────────────────────────────────────────────┤
│ AUDIT: ZERO-PCM       │ Static analysis: 0 sample banks, 0 PCM tables, 0 B64  │
└───────────────────────────────────────────────────────────────────────────────┘
```

### 2.1 Tier 1: Functional & Algorithmic Feature Coverage
- **Objective**: Verify the primary operational behavior (happy path, parameter modulation, and functional outputs) for every subsystem in isolation and coupled pipelines.
- **Requirement**: A minimum of **5 distinct test assertions per feature** across all 47 features enumerated in `PROJECT.md § Feature Inventory`.
- **Key Subsystems Tested**:
  - *Deck 01*: Karnopp stick-slip velocity deadband $(v_d = 0.0015\text{ m/s})$, Stribeck breakaway exponential velocity $(v_s = 0.06\text{ m/s})$, Hunt-Crossley Hertzian impact contact restitution $(F_c = k_c x^\alpha + \lambda_c x^\alpha \dot{x})$, Buchla 292 optical vactrol sag $(\tau_{\text{decay}}[1 + \kappa(1-g)^2])$.
  - *Deck 02*: Helical Beltrami vortex jet $(\nabla \times \mathbf{v} = \lambda \mathbf{v})$, Minnaert gas-liquid bubble bank $(f = 3.28325 / R)$, Poisson stochastic droplet point-process clock $(\Delta t = -\ln(1-U)/\lambda)$, Bjorklund Euclidean peristaltic sequencer $E(k, n)$.
  - *Deck 03*: 16-pole Zavalishin TPT SVF bandpass filter bank, $O(N)$ Householder scattering matrix $(\mathbf{H} = \mathbf{I} - \frac{2}{16}\mathbf{1}\mathbf{1}^T)$, 4 geometric manifolds (Sigmoid Horn, Chladni Ring, Vocal Tract, Porcelain Jordan Cavity), 5 viscoelastic damping profiles.
  - *Deck 04*: DarkSnail $\mathfrak{so}(16)$ Lie generators $(A = \frac{1}{2}(B - B^\top))$, bilinear triad operator $S(W, i, c_1, c_2)$, `SlowFastTriadCascade`, Zeno-Amygdala adaptive unroll controller $(K \in \lbrace 1, 4, 8, 16\rbrace)$, 3D RK4 Lorenz strange attractor.
  - *Deck 05*: Constant-power Golden-Ratio stereo modal panning $(\Phi = 137.508^\circ)$, Roland Dimension D tri-phase BBD chorus $(0^\circ, 120^\circ, 240^\circ)$, 15 Hz infrasonic DC blocker, phase-locked visceral sub-bass reinforcement, Hermite $C^1$ soft-knee BoundedSaturator + true-peak limiter.
  - *Voice & Telemetry*: Polyphonic voice architecture (Mono-Legato, Poly 8-Voice, Stereo Unison Detune), APVTS 40-parameter matrix, lock-free SPSC telemetry queue.

### 2.2 Tier 2: Boundary, Multirate & Buffer Resilience
- **Objective**: Ensure the synthesis engine executes without degradation, numerical instability, or discontinuities across extreme operating conditions.
- **Multirate Matrix**:
  - Sample rates tested: **44.1 kHz, 48.0 kHz, 88.2 kHz, 96.0 kHz, 176.4 kHz, 192.0 kHz**.
  - Pass criteria: Output signals remain stable, filter poles do not warp beyond Nyquist, and frequency tracking scales proportionally.
- **Buffer Size Resilience**:
  - Buffer sizes tested: **1 (single-sample / AudioWorklet step), 16, 32, 64, 128 (Web Audio block), 256, 512, 1024, 2048 samples**.
  - Pass criteria: Bit-identical state evolution regardless of buffer slicing granularity; zero state reset artifacts across buffer boundaries.
- **Corner Cases & Extremes**:
  - Colonic pressure head at $P = 0.0$ (complete silence, zero idle bleed) vs $P = 1.0$ (maximum acoustic drive).
  - Resting aperture at minimum ($0.1\text{ mm}$, tight stiction) vs maximum ($5.0\text{ mm}$, open pipe blow).
  - 100% parameter step transients (instant jump from min to max within 1 sample) without zipper pop or overflow.
  - Input with subnormal float values ($1\text{e}-38$) properly flushed to zero without CPU stall.

### 2.3 Tier 3: Cross-Feature Interactions & Mathematical Invariants

**Objective**: Mathematically verify the fundamental physical and Lie-algebraic theorems underpinning Poopenfarten II.

#### 1. Skew-Symmetric Triad Antisymmetry

```math
\forall i, c_1, c_2: \quad \lvert S(W, i, c_1, c_2) + S(W, i, c_2, c_1) \rvert < 1.0 \times 10^{-7}
```

```math
\forall i, c: \quad \lvert S(W, i, c, c) \rvert < 1.0 \times 10^{-7}
```

#### 2. Exact Pointwise Tangent Orthogonality

```math
\lvert \langle v, B_W(u, v) \rangle \rvert < 1.0 \times 10^{-6}
```

#### 3. Multi-Step Spherical Norm Conservation (`SlowFastTriadCascade`)

Across unroll depths $K \in \lbrace 1, 4, 8, 16\rbrace$, initial state $v^{(0)}$ and retracted state $\tilde{v}^{(T)}$ satisfy:

```math
\left\lvert \lVert\tilde{v}^{(T)}\rVert_2^2 - \lVert v^{(0)}\rVert_2^2 \right\rvert < 1.0 \times 10^{-6}
```

guaranteeing $0.000\text{e}+00$ radial drift on the $S^{15}$ hypersphere.

#### 4. $O(N)$ Householder Scattering Isometry

For $\mathbf{H} = \mathbf{I} - \frac{2}{N}\mathbf{1}\mathbf{1}^T$ with $N=16$:

```math
\left\lvert \lVert\mathbf{H}\mathbf{y}\rVert_2 - \lVert\mathbf{y}\rVert_2 \right\rvert < 1.0 \times 10^{-6} \quad \forall \mathbf{y} \in \mathbb{R}^{16}
```

#### 5. Roland Dimension D Mono-Sum Cancellation Immunity

With wet chorus enabled and left/right outputs summed to mono:

```math
\frac{\lVert\text{Wet}_{\text{mono}}\rVert_2}{\lVert\text{Dry}\rVert_2} \ge \frac{\sqrt{3}}{4} > 0.40
```

with zero phase-cancellation nulls across $20\text{ Hz} - 20\text{ kHz}$ frequency sweeps.

#### 6. Zeno-Amygdala Adaptive Unroll Monotonicity

As triad flux $\Phi_{\text{Zeno}}$ and pressure conflict increase, unroll depth $K$ escalates monotonically:

```math
T_{\text{Zeno}} < 0.45 \implies K = 1, \quad T_{\text{Zeno}} \ge 0.75 \implies K = 16
```

#### 7. Hermite Soft-Knee BoundedSaturator Asymptotic Ceiling

```math
\forall x \in \mathbb{R}: \quad \lvert y(x) \rvert \le 1.0500 \quad (0.42\text{ dBFS ceiling})
```

```math
\lvert x \rvert \le 0.72 \implies y(x) = x \quad (\text{exact linear transparency})
```

with $C^1$ continuous first derivative across transition point $k=0.72$.

### 2.4 Tier 4: Real-World Scenarios & Adversarial Hardening
- **Objective**: Full-system stress testing under production presets, multi-voice polyphony, and adversarial chaos.
- **Factory Presets Verification**:
  - All 16 factory presets executed under sustained notes ($> 5.0\text{ seconds}$), rapid note bursts ($32\text{nd}$ notes at 180 BPM), and maximum performance macro modulations (*Bauchpresse* $\times$ *Schleimhautfeuchte*).
- **Polyphony & Voice Stealing**:
  - 8-voice polyphonic overload test: rapidly triggering 32 simultaneous notes verifies smooth de-clicked voice stealing without audio pops or clicks.
  - Stereo Unison mode: 4-voice detune spread ($\pm 7$, $\pm 14$ cents) with decorrelated phase coherence.
- **Zero Audio-Thread Heap Allocation Guard**:
  - Custom `operator new/delete` override with thread-local allocation tracker active during audio block rendering.
  - Assertion: `gAllocationCount == 0` during all `processBlock` / `processSample` calls.
- **Adversarial Overdrive & Denormal Stress**:
  - 100,000 continuous samples fed with $+40\text{ dBFS}$ impulse bursts and random white-noise pressure head.
  - Assertion: Zero NaNs, zero Infinities, zero subnormal numbers, peak output $\le 1.05$.

---

## 3. Automated Zero-PCM Source Code Audit

To eliminate legacy anti-patterns (such as `poopenfarten-pro`'s 5.3 MB `SampleBank.cpp` and 1.18 MB `sample_bank.js`), an automated Python audit script (`scripts/audit_zero_pcm.py`) scans the entire repository before test runs and CI checks.

### Audit Rules & Blacklist
| Pattern / Token | Description | Action if Found |
|---|---|---|
| `SampleBank` / `sample_bank` | Legacy PCM sample table classes | Immediate Test Failure |
| `s_sample_` | Hardcoded static audio float arrays | Immediate Test Failure |
| `SamplePitcherEngine` | Sample playback resampling engines | Immediate Test Failure |
| `base64Data` / `atob(` | Base64-encoded PCM audio strings in JS | Immediate Test Failure |
| Large float literals arrays ($> 128$ floats) | Any disguised audio sample table in source | Immediate Test Failure |
| Audio binary assets (`.wav`, `.aif`, `.pcm`, `.raw`) in `source/` or `web/` | Embedded audio files | Immediate Test Failure |

---

## 4. Test Execution & Runner Commands

### 4.1 Headless C++ DSP Verification Runner
Executable Target: `poopenfarten2_headless_dsp_tests.exe` (CMake target `poopenfarten2_headless_dsp_tests`)
Location: `build/source/tests/Release/poopenfarten2_headless_dsp_tests.exe` (or `build/`)

| Test Scope | Command | Description |
|---|---|---|
| **Run All Tiers (Full Suite)** | `poopenfarten2_headless_dsp_tests.exe --all` | Executes Tiers 1, 2, 3, and 4 |
| **Run Tier 1 Only** | `poopenfarten2_headless_dsp_tests.exe --tier=1` | Functional & algorithmic feature tests |
| **Run Tier 2 Only** | `poopenfarten2_headless_dsp_tests.exe --tier=2` | Multirate, buffer sizes, boundary tests |
| **Run Tier 3 Only** | `poopenfarten2_headless_dsp_tests.exe --tier=3` | Mathematical invariants & Lie algebra |
| **Run Tier 4 Only** | `poopenfarten2_headless_dsp_tests.exe --tier=4` | Presets, polyphony, zero-allocation guard |
| **CTest Execution** | `ctest --test-dir build --output-on-failure` | CMake standard test runner invocation |

### 4.2 Automated Web Audio & Node.js Verification Suite
Test Runner: Node.js built-in test runner (`node:test`, zero dependencies)
Location: `web/verify.mjs`

| Test Scope | Command | Description |
|---|---|---|
| **Run All Web Verifications** | `npm run verify:all` | Executes all 7 web verification suites |
| **Standard Test Alias** | `npm test` | Runs complete verification suite |
| **Direct Node Execution** | `node --test web/verify.mjs` | Direct invocation via Node test runner |

### 4.3 Zero-PCM Static Source Audit
Location: `scripts/audit_zero_pcm.py`

| Scope | Command | Description |
|---|---|---|
| **Automated Audit** | `python scripts/audit_zero_pcm.py` | Scans `source/` and `web/` for static PCM tables |

### 4.4 Master Batch Runner (Windows CLI)
Location: `scripts/run_all_verifications.bat`

```cmd
scripts\run_all_verifications.bat
```
Executes:
1. `python scripts/audit_zero_pcm.py`
2. `npm run verify:all`
3. `poopenfarten2_headless_dsp_tests.exe --all`

---

## 5. Coverage & Acceptance Sign-Off Matrix

| Metric | Target Threshold | Verification Mechanism |
|---|---|---|
| Total Features Covered | 47 / 47 features (100%) | `Tier1_FeatureTests.h` & `verify.mjs` |
| Tests per Feature (Tier 1) | $\ge 5$ distinct assertions | `Tier1_FeatureTests.h` |
| Sample Rate Support | 44.1 kHz - 192.0 kHz | `Tier2_BoundaryTests.h` |
| Buffer Size Granularity | 1 to 2048 samples | `Tier2_BoundaryTests.h` |
| Skew-Symmetric Triad Error | $< 1.0 \times 10^{-7}$ | `Tier3_MathematicalInvariantTests.h` |
| Spherical Norm Radial Drift | $< 1.0 \times 10^{-6}$ | `Tier3_MathematicalInvariantTests.h` |
| Householder Scattering Error | $< 1.0 \times 10^{-6}$ | `Tier3_MathematicalInvariantTests.h` |
| Audio-Thread Allocations | Exactly 0 bytes / 0 calls | `Tier4_StressTests.h` (`operator new` guard) |
| Audio Output Peak Limiting | $\le 1.0500$ $(\le 0.42\text{ dBFS})$ | `Tier3` & `Tier4` saturator tests |
| Embedded PCM Samples | Exactly 0 instances | `scripts/audit_zero_pcm.py` |
| Web UI & Preset Compliance | 16 presets, RFC 8259, DIN 1451 | `web/verify.mjs` |
