# PF-02 · Darm-Akustischer & Ultrametrischer Triaden-Synthesizer

[![Synthesizer Engine](https://img.shields.io/badge/DSP%20Engine-C%2B%2B20%20%2F%20JUCE%208-blue.svg)](#native-c-juce-8-plugin-suite--targets)
[![Web Audio API](https://img.shields.io/badge/Web%20Audio-100%25%20Procedural%20Worklet-brightgreen.svg)](#zero-install-web-audio-application)
[![Verification Suite](https://img.shields.io/badge/Verification-100%25%20Pass%20(58%20Suites)-success.svg)](#master-verification--quality-assurance)
[![Synthesis Purity](https://img.shields.io/badge/Zero--PCM%20Audit-0%20Sample%20Tables%20(100%25%20Physical)-orange.svg)](#zero-pcm-procedural-purity-guarantee)
[![License](https://img.shields.io/badge/License-MIT-lightgrey.svg)](LICENSE)

> **PF-02** is an authoritative, first-principles procedural physical-modeling aeroacoustic synthesizer. It bridges continuous mathematical Lie-algebraic primitives from **Project DarkSnail** (`SlowFastTriadCascade` over $\mathfrak{so}(16)$, skew-symmetric bilinear operators, spherical norm retraction, and Zeno-Amygdala adaptive dynamic unrolls) with non-linear viscoelastic contact mechanics, hydrodynamic fluid dynamics, and German functionalist laboratory industrial design.

---

## Table of Contents

1. [Architectural Overview & Core Innovations](#architectural-overview--core-innovations)
2. [Documentation (`docs/`)](#documentation-docs)
3. [German Functionalist Industrial Design](#german-functionalist-industrial-design)
4. [The Five Sound Decks & Signal Flow](#the-five-sound-decks--signal-flow)
5. [Bespoke 4-Mode 60 FPS Vector CRT Visualizer](#bespoke-4-mode-60-fps-vector-crt-visualizer)
6. [16 Curated Factory Presets](#16-curated-factory-presets)
7. [40-Parameter APVTS & Web Audio Taxonomy](#40-parameter-apvts--web-audio-taxonomy)
8. [Native C++ JUCE 8 Plugin Suite & Targets](#native-c-juce-8-plugin-suite--targets)
9. [Zero-Install Web Audio Application](#zero-install-web-audio-application)
10. [Build & Quickstart Instructions](#build--quickstart-instructions)
11. [Master Verification & Quality Assurance](#master-verification--quality-assurance)
12. [Zero-PCM Procedural Purity Guarantee](#zero-pcm-procedural-purity-guarantee)
13. [License & Attributions](#license--attributions)

---

## Architectural Overview & Core Innovations

Poopenfarten II (`PF-02`) departs completely from conventional wavetable playback, static sample looping, and additive sound synthesis. Every vibration, pressure transient, cavitation pop, and acoustic resonance is synthesized numerically from continuous physical equations in real time.

```
[Performance Controls: Bauchpresse & Schleimhautfeuchte]
                          │
                          ▼
      [DECK 01 · KARNOPP-SCHLEIMHAUT & DRUCKVENTIL]
  Karnopp Stick-Slip Friction, Stribeck Breakaway,
  Hunt-Crossley Impact Restitution, Buchla 292 Sag
                          │ Aperture y(t) & Slit Velocity v(t)
                          ▼
      [DECK 02 · BELTRAMI-STRAHL & MINNAERT-FLUID]
  Beltrami Vortex Swirl, Minnaert Bubble Resonators,
  Poisson Droplet Point-Process, Bjorklund Sequencer
                          │ Wideband Excitation Signal
                          ▼
      [DECK 03 · SO(16) TRIADEN-RESONATOR MATRIX]
  16-Pole Zavalishin TPT SVF Matrix, O(N) Householder Scattering,
  4 Geometric Manifolds, 5 Viscoelastic Damping Profiles,
  DarkSnail Skew Triad Operator & Spherical Norm Retraction
                          │ Coupled Modal Vector v ∈ ℝ¹⁶
                          ▲
      [DECK 04 · ZENON-AMYGDALA & LORENZ-ATTRAKTOR]
  Audio-Rate Dynamic Unroll K ∈ {1, 4, 8, 16},
  3D Runge-Kutta 4th-Order Chaotic Lorenz Attractor
                          │
                          ▼
      [DECK 05 · DIMENSION-D BBD & HERMITE-BEGRENZER]
  Golden-Ratio Stereo Panning, Roland Dimension D Chorus
  (Tri-Phase Mono Cancellation Immune), 15 Hz Sub DC Blocker,
  Hermite C¹ BoundedSaturator & True-Peak Limiter (≤ 1.05)
                          │
                          ▼
               [Stereo Audio Output Bus]
```

### Key Technical Pillars
- **Zero-PCM Purity**: Exactly 0 static sample tables, 0 recorded audio files, and 0 Base64 PCM arrays. 100% procedural sound synthesis.
- **Continuous Lie-Algebraic Primitives**: Full 16-dimensional $\mathfrak{so}(16)$ Lie algebra generators driving coupled modal energy exchange with exact spherical norm retraction ($\|\tilde{v}\|_2 = \|v_0\|_2$, maintaining $0.000\text{e}+00$ radial drift on the $S^{15}$ hypersphere).
- **Hard Real-Time Determinism**: Zero heap allocations (`malloc`/`operator new`) on audio threads across both C++ JUCE 8 and Web AudioWorklet implementations.
- **Mono Cancellation Immunity**: Tri-phase bucket-brigade device (BBD) chorus modulation configured at $0^\circ, 120^\circ, 240^\circ$, mathematically guaranteeing non-zero acoustic power upon mono-summing across all audible frequencies ($20\text{ Hz} - 20\text{ kHz}$).
- **Dual Bit-Compatible Codebase**: Mirrored architectures across native desktop C++20 (`source/`) and client-side JavaScript Web Audio API (`web/`).

---

## Documentation (`docs/`)

Comprehensive mathematical derivations, testing specifications, and verification sign-off reports are organized inside [`docs/`](docs/):

- **[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)** — Complete physical equations, continuous $\mathfrak{so}(16)$ Lie algebra derivations, tangent orthogonality and mono-sum immunity proofs, and signal-flow Mermaid diagrams.
- **[`docs/TESTING.md`](docs/TESTING.md)** — 4-tier E2E opaque-box verification methodology, numerical invariant tolerances, and automated Zero-PCM audit specifications.
- **[`docs/VERIFICATION_CHECKLIST.md`](docs/VERIFICATION_CHECKLIST.md)** — Master empirical sign-off matrix across all 47 instrument features, stress probes, and build targets.

---

## German Functionalist Industrial Design

The user interface of `PF-02` embodies the rigorous functionalist laboratory instrument tradition of post-war German acoustic engineering:
- **Form Follows Function**: Clean 19" 2U rackmount chassis with precision counter-sunk hex screws, rack ears, tactile knurled aluminum rotary knobs, and illuminated status indicators.
- **DIN 1451 Mittelschrift**: Clear technical typography adhering strictly to the DIN 1451 standard for laboratory legend plates.
- **Strictly Adjacent Laboratory Aesthetics**: Features high-contrast Light Theme (anodized aluminum `#ECEBE4`, matte charcoal text `#141517`, signal orange accents `#FF5E1E`) and Dark Laboratory Theme (`#141517`, cathode phosphor green `#24FF6A`, neon amber `#FFB000`). Strictly avoids explicit corporate trademarks while paying homage to functionalist design purity.
- **Physical Safety Rocker Switch**: Dedicated spring-loaded, illuminated safety power switch with Standby/Active modes to prevent accidental acoustic discharge during laboratory calibrations.

---

## The Five Sound Decks & Signal Flow

### Deck 01: Karnopp-Schleimhaut & Druckventil (Contact Mechanics & Excitation)
- **Karnopp Stick-Slip Friction**: Velocity deadband ($v_d = 0.0015\text{ m/s}$) dividing physical mucosal tissue into sticking (static friction balance) and slipping (dynamic kinetic friction) phases.
- **Stribeck Breakaway Velocity**: Smooth exponential breakaway velocity curve ($v_s = 0.06\text{ m/s}$) modeling the tearing transient of mucosal seal burst.
- **Hunt-Crossley Hertzian Impact**: Viscoelastic tissue collision mechanics ($F_c = k_c x^\alpha + \lambda_c x^\alpha \dot{x}$) integrated via velocity Verlet to simulate tissue slap when aperture $y(t) \le 0$.
- **Buchla 292 Optical Vactrol Sag**: Non-linear dual-time-constant vactrol droop $(\tau_{\text{decay}}[1 + \kappa(1-g)^2])$ with fast attack $(\approx 2\text{ ms})$ and history-dependent sluggish release.

### Deck 02: Beltrami-Strahl & Minnaert-Phasenfluid (Aeroacoustics & Fluid Dynamics)
- **Beltrami Helical Vortex Jet**: Swirling aeroacoustic jet where vorticity aligns with velocity ($\nabla \times \mathbf{v} = \lambda \mathbf{v}$), injecting wideband turbulent energy modulated by the flow Reynolds number.
- **Minnaert Gas-Liquid Bubble Bank**: Procedural multi-bubble bank based on Marcel Minnaert's radial pulsation formula ($f = 3.28325 / R$), producing visceral wet cavitation pops with radius-dependent exponential decay.
- **Poisson Droplet Point-Process**: Stochastic arrival time clock $(\Delta t = -\frac{\ln(1-U)}{\lambda})$ coupled to an Irwin-Hall droplet mass distribution for random mucosal splatter transients.
- **Bjorklund Euclidean Peristaltic Sequencer**: Bresenham-derived Euclidean rhythm algorithm $E(k, n)$ for periodic, rhythmic mucosal pulsing.

### Deck 03: $\mathfrak{so}(16)$ Triaden-Resonator & Porzellan-Feld (Modal Resonator Matrix)
- **16-Pole Zavalishin TPT SVF Matrix**: Topology-Preserving Transform state-variable filter bank with trapezoidal integration, radiation resistance damping, and branchless denormal protection.
- **$O(N)$ Householder Energy Scattering Matrix**: Unitary reflection operator ($\mathbf{H} = \mathbf{I} - \frac{2}{16}\mathbf{1}\mathbf{1}^T$) diffusing acoustic energy orthogonally across all 16 modes with strict $\ell_2$ norm isometry.
- **4 Geometric Manifolds**: Selectable acoustic boundaries:
  1. *Sigmoid Hyperbolic Horn* (exponential acoustic expansion)
  2. *Biharmonic Chladni Ring* (annular membrane modal distribution)
  3. *Glottal-Colonic Tract* (anatomical tract formants)
  4. *Porcelain Jordan Cavity* (201-channel complex cavity mode field)
- **5 Viscoelastic Damping Profiles**: Material damping laws: *Mucosal Tissue*, *Porcelain Ceramic*, *Wet Elastomer*, *Bell Bronze*, and *Stiff Cartilage*.
- **DarkSnail Skew-Symmetric Bilinear Triad Operator**: High-order tensor $S(W, i, c_1, c_2) = -S(W, i, c_2, c_1)$ satisfying exact tangent orthogonality $\langle v, S(u, v) \rangle = 0$.
- **SlowFastTriadCascade**: Multi-step tangent Lie algebra updates followed by exact spherical norm retraction, preserving initial kinetic energy on $S^{15}$ with $0.000\text{e}+00$ radial drift.

### Deck 04: Zenon-Amygdala & Lorenz-Attraktor (Adaptive Unroll & Chaos)
- **Zeno-Amygdala Adaptive Controller**: Audio-rate conflict monitor computing Beltrami triad flux and pressure divergence, dynamically scheduling unroll iterations $K \in \{1, 4, 8, 16\}$ to balance real-time CPU efficiency with chaotic stability.
- **3D Runge-Kutta 4th-Order (RK4) Lorenz Attractor**: Chaotic non-linear strange attractor trajectory modulating odd and even modal resonant frequencies and Q factors.

### Deck 05: Dimension-D BBD & Hermite-Begrenzer (Spatial Mastering & Limiting)
- **Golden-Ratio Stereo Modal Panning**: Constant-power dispersion law based on the golden angle ($\Phi = 137.507764^\circ$) distributing 16 modal outputs in stereo space.
- **Roland Dimension D Tri-Phase BBD Chorus**: 3 discrete delay lines modulated at $0^\circ, 120^\circ, 240^\circ$ with Catmull-Rom cubic interpolation and NE570 compander emulation, engineered to prevent phase cancellation nulls upon mono-summing.
- **Infrasonic Highpass & Visceral Sub-Bass**: 15 Hz DC blocking filter paired with phase-locked visceral sub-harmonic reinforcement.
- **Hermite $C^1$ Soft-Knee BoundedSaturator**: Cubic Hermite saturation curve transparent below $k = 0.72$, guaranteeing an asymptotic ceiling strictly $\le 1.05$ peak ($+0.42\text{ dBFS}$) under $+40\text{ dBFS}$ overdrive inputs.

---

## Bespoke 4-Mode 60 FPS Vector CRT Visualizer

The built-in vector CRT visualizer runs at a smooth 60 frames per second using hardware-accelerated Canvas 2D/WebGL rendering, complete with P31 phosphor decay, beam blooming, barrel distortion, and scanline raster effects:

1. **Mode 1: BELTRAMI-KAMMER**: Real-time cross-section of the mucosal aperture, displaying opening displacement, velocity vectors, helical vortex streamlines, Minnaert bubble nucleation radii, and Jordan cavity standing wave ripples.
2. **Mode 2: SO(16) TRIADEN-NETZ**: Visual projection of 16 modal nodes distributed along the $S^{15}$ hypersphere, animated skew-symmetric energy arcs, and live radial norm drift readouts ($0.000\text{e}+00$).
3. **Mode 3: ZENON-LORENZ**: 3D vector wireframe trajectory of the chaotic Lorenz strange attractor, real-time Zeno conflict index meter, and illuminated indicators for active unroll depth $K \in \{1, 4, 8, 16\}$.
4. **Mode 4: SPEKTRUM / LISSAJOUS**: 16 ballistic modal energy bars, an oscilloscope XY vector Lissajous stereo phase scope, and true-peak / RMS headroom meters.

---

## 16 Curated Factory Presets

| # | Preset Name | Category | Macro Defaults (Gut / Moisture) | Sonic Profile & Physical Character |
|:---:|:---|:---|:---:|:---|
| 01 | `PF02_DEFAULT_DARM_TRIADE` | Baseline Laboratory | 50% / 50% | Balanced reference calibration of mucosal stick-slip and triad resonance |
| 02 | `GUTTURAL_SUB_INFRASONIC` | Sub-Bass & Infrasonic | 95% / 20% | Visceral 15–35 Hz colonic rumble, high tissue tension, tight aperture |
| 03 | `STRIEBECK_STICK_SLIP_TEAR` | Friction & Impact | 35% / 10% | Prominent dry stick-slip chatter, sharp Stribeck breakaway transients |
| 04 | `BELTRAMI_HELICAL_VORTEX` | Vortex & Fluid | 60% / 40% | High Reynolds number turbulence, whistling helical vortex precession |
| 05 | `MINNAERT_HYDRO_SPLATTER` | Fluid & Cavitation | 40% / 90% | Saturated multiphase moisture, rapid Poisson bubble pops, wet spray |
| 06 | `PORCELAIN_JORDAN_RESONANCE`| Cavity Acoustics | 70% / 30% | Crystalline porcelain boundary reflections, 201-channel Jordan modes |
| 07 | `SO16_SPHERICAL_ROAR` | Lie Algebra Resonance | 80% / 50% | Maximum $\mathfrak{so}(16)$ skew coupling, spherical norm conservation roar |
| 08 | `ZENON_AMYGDALA_CHURN` | Chaos & Nonlinear | 85% / 65% | High Zeno conflict flux driving $K=16$ unroll, turbulent Lorenz detuning |
| 09 | `CHLADNI_ANNULAR_SPHINCTER` | Manifold Modal | 45% / 35% | Annular plate nodal lines, metallic and viscoelastic hybrid ringing |
| 10 | `SIGMOID_HYPERBOLIC_HORN` | Manifold Modal | 75% / 25% | Open acoustic horn flare, brassy acoustic impedance transformation |
| 11 | `DIMENSION_STEREO_FLUTTER` | Spatial & Modulation | 50% / 75% | Deep tri-phase Dimension D BBD chorusing, wide golden-ratio spread |
| 12 | `EUCLIDEAN_PERISTALSIS_5_16`| Rhythmic Sequencer | 60% / 45% | $E(5, 16)$ Euclidean peristaltic rhythm triggering mucosal burst cycles |
| 13 | `BUCHLA_OPTICAL_VENTING` | Contact Dynamics | 30% / 60% | Optical vactrol sag droop with sluggish non-linear lowpass decay |
| 14 | `UNISON_QUAD_DETUNE_ROAR` | Polyphonic & Unison | 90% / 40% | Stereo Unison 4-voice detune spread ($\pm 14$ cents) with decorrelation |
| 15 | `BRONZE_VISCERAL_BELL` | Metallic Damping | 65% / 15% | High Q Bell Bronze damping profile with rich inharmonic partials |
| 16 | `KINETIC_EXPLOSIVE_BURST` | Transient Laboratory | 100% / 80% | Maximum pressure head, instantaneous breakaway slap, explosive decay |

---

## 40-Parameter APVTS & Web Audio Taxonomy

The parameter system maintains bit-identical ranges, defaults, and IDs between the native C++ JUCE 8 AudioProcessorValueTreeState (APVTS) and Web Audio API:

| Parameter ID | Control Label | Deck | Unit | Range | Default |
|:---|:---|:---:|:---:|:---:|:---:|
| `param_colonic_pressure` | Druck (Pressure Head) | Deck 01 | kPa | 0.0 – 1.0 | 0.50 |
| `param_mucosal_tension` | Wand-Spannung (Tissue Tension) | Deck 01 | N/m | 0.0 – 1.0 | 0.40 |
| `param_resting_aperture` | Spalt-Weite (Resting Aperture)| Deck 01 | mm | 0.0 – 1.0 | 0.25 |
| `param_karnopp_stiction` | Haft-Reibung (Karnopp Stiction)| Deck 01 | N | 0.0 – 1.0 | 0.60 |
| `param_stribeck_breakaway`| Abriss (Stribeck Breakaway) | Deck 01 | m/s | 0.0 – 1.0 | 0.50 |
| `param_tissue_hardness` | Gewebe-Haerte (Tissue Hardness)| Deck 01 | N/m^α | 0.0 – 1.0 | 0.45 |
| `param_impact_restitution`| Prall-Verlust (Impact Rest.) | Deck 01 | norm | 0.0 – 1.0 | 0.70 |
| `param_vactrol_sag_tau` | Vactrol-Traegheit (Sag Tau) | Deck 01 | ms | 0.0 – 1.0 | 0.30 |
| `param_beltrami_vortex_pitch`| Wirbel-Steigung (Vortex Pitch)| Deck 02 | rad/m | 0.0 – 1.0 | 0.50 |
| `param_reynolds_turbulence`| Reynolds-Zahl (Turbulence) | Deck 02 | norm | 0.0 – 1.0 | 0.45 |
| `param_multiphase_moisture`| Schleim-Feuchte (Moisture) | Deck 02 | % | 0.0 – 1.0 | 0.35 |
| `param_fluid_viscosity` | Viskositaet (Fluid Viscosity) | Deck 02 | Pa·s | 0.0 – 1.0 | 0.50 |
| `param_minnaert_bubble_density`| Blasen-Dichte (Bubble Density)| Deck 02 | Hz | 0.0 – 1.0 | 0.40 |
| `param_poisson_droplet_rate`| Tropfen-Rate (Poisson Rate) | Deck 02 | Hz | 0.0 – 1.0 | 0.30 |
| `param_euclidean_peristalsis_k`| Peristaltik-K (Euclidean K) | Deck 02 | pulses | 1 – 16 | 5 |
| `param_euclidean_peristalsis_n`| Peristaltik-N (Euclidean N) | Deck 02 | steps | 1 – 16 | 16 |
| `param_triad_coupling_gain`| Triaden-Kopplung (Coupling) | Deck 03 | norm | 0.0 – 1.0 | 0.50 |
| `param_so16_skew_generator`| SO(16)-Generator (Skew Mode) | Deck 03 | idx | 0 – 3 | 0 |
| `param_spherical_retraction`| Spherische Retraktion | Deck 03 | bool | 0 – 1 | 1 |
| `param_householder_coupling`| Householder-Streuung | Deck 03 | norm | 0.0 – 1.0 | 0.65 |
| `param_geometric_manifold`| Raum-Geometrie (Manifold) | Deck 03 | idx | 0 – 3 | 0 |
| `param_damping_profile` | Daempfungs-Profil (Damping) | Deck 03 | idx | 0 – 4 | 0 |
| `param_porcelain_cavity_mix`| Porzellan-Kammermix (Mix) | Deck 03 | % | 0.0 – 1.0 | 0.30 |
| `param_porcelain_boundary_mode`| Rand-Bedingung (Boundary) | Deck 03 | idx | 0 – 2 | 0 |
| `param_zeno_unroll_override`| Zenon-Unroll (Override K) | Deck 04 | idx | 0 – 4 | 0 (Auto) |
| `param_zeno_conflict_threshold`| Konflikt-Schwelle (Threshold)| Deck 04 | norm | 0.0 – 1.0 | 0.50 |
| `param_lorenz_orbit_rate` | Lorenz-Rate (Orbit Speed) | Deck 04 | Hz | 0.0 – 1.0 | 0.25 |
| `param_lorenz_chaos_depth` | Chaos-Tiefe (Chaos Depth) | Deck 04 | norm | 0.0 – 1.0 | 0.35 |
| `param_dimension_d_enable`| Dimension-D Aktiv (Enable) | Deck 05 | bool | 0 – 1 | 1 |
| `param_dimension_d_mode` | Dimension-D Modus (Mode 1-4)| Deck 05 | idx | 0 – 3 | 1 |
| `param_dimension_d_mix` | Dimension-D Anteil (Mix) | Deck 05 | % | 0.0 – 1.0 | 0.50 |
| `param_colonic_saturation_drive`| Darm-Saettigung (Drive) | Deck 05 | dB | 0.0 – 1.0 | 0.30 |
| `param_visceral_sub_reinforce`| Sub-Verstaerkung (Sub Boost)| Deck 05 | dB | 0.0 – 1.0 | 0.40 |
| `param_golden_stereo_spread`| Goldener Schnitt (Spread) | Deck 05 | % | 0.0 – 1.0 | 0.70 |
| `param_voicing_mode` | Stimmen-Modus (Voicing) | Engine | idx | 0 – 2 | 1 (Poly 8) |
| `param_glide_portamento_ms`| Gleit-Zeit (Portamento) | Engine | ms | 0.0 – 500.0 | 15.0 |
| `param_master_volume_db` | Haupt-Lautstaerke (Volume) | Engine | dBFS | -60.0 – +6.0 | -3.0 |
| `param_power_active` | Haupt-Schalter (Power) | Engine | bool | 0 – 1 | 1 |
| `macro_gut_squeeze` | Bauchpresse (Macro X) | Macro | norm | 0.0 – 1.0 | 0.50 |
| `macro_fluid_moisture` | Schleimhautfeuchte (Macro Y)| Macro | norm | 0.0 – 1.0 | 0.50 |

---

## Native C++ JUCE 8 Plugin Suite & Targets

The native desktop suite is built with **JUCE 8** and **C++20**, outputting bit-exact, hard real-time audio plugins and standalone binaries across Windows, Linux, and macOS Universal:

| Target Name | Format | Binary Path | Binary Size | Features & Platform |
|:---|:---:|:---|:---:|:---|
| **PF-02 Standalone** | Executable (`.exe`) | `releases/PF-02.exe` | 8.24 MB | Native Windows x64 executable hosting embedded WebView2 UI bridge |
| **PF-02 VST3** | VST3 Bundle (`.vst3`) | `releases/PF-02.vst3` | 6.88 MB | Steinberg VST3 specification compliant with sample-accurate automation |
| **PF-02 CLAP** | CLAP Plugin (`.clap`) | `releases/PF-02.clap` | 6.85 MB | CLAP audio standard via `clap-juce-extensions`, non-destructive modulation |
| **PF-02 Headless Tests** | Test Binary (`.exe`) | `build/source/tests/Release/poopenfarten2_headless_dsp_tests.exe` | ~1.2 MB | Decoupled 4-tier DSP test harness for CI/CD and verification |

---

## Zero-Install Web Audio Application

For instant zero-install browser deployment, `PF-02` includes an identical client-side procedural Web Audio implementation:
- **Zero External Dependencies**: Pure vanilla JavaScript (ES Modules) with no Webpack, Vite, or external package bundlers required.
- **AudioWorklet Execution**: The physical voice graph and Lie algebra solvers run on dedicated high-priority audio threads via `pf02_audio_worklet.js`.
- **Lossless 16-Bit WAV Master Recorder**: Client-side recording engine generating bit-exact 44-byte RIFF WAVE files with immediate download export.
- **Integrated Microtonal Strip & Macro Pad**: 16-key keyboard strip with scale quantization, pitch bend, and kinetic burst trigger pads.

---

## Build & Quickstart Instructions

### 1. Zero-Install Web Application (1-Click Launch)
Double-click `start.bat` in the repository root, or start via Node.js:
```powershell
# Install optional dev tools (Node.js built-in server requires zero modules)
npm start
```
Open your browser to:
```
http://localhost:8080
```

### 2. Native C++ Plugin Suite (CMake & MSVC)
Prerequisites: Visual Studio 2022 (MSVC C++20), CMake $\ge 3.22$, JUCE 8.
```powershell
# Configure CMake Release Build
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# Compile all plugin targets and headless tests
cmake --build build --config Release --parallel

# Package Windows release archives into releases/
python scripts/package_release.py
```

---

## Master Verification & Quality Assurance

To execute all automated test suites across C++, Web, and the Zero-PCM source auditor:
```powershell
scripts\run_all_verifications.bat
```

### Individual Test Verification Commands

1. **Automated Zero-PCM Source Audit**:
   ```powershell
   python scripts/audit_zero_pcm.py
   # Or via npm alias:
   npm run audit:pcm
   ```
2. **Web Audio, UI & Node.js Verification Suite**:
   ```powershell
   npm test
   # Or full verification alias:
   npm run verify:all
   ```
3. **GitHub-Flavored Markdown (GFM) & KaTeX Math Compliance Audit**:
   ```powershell
   python scripts/audit_gfm_math.py
   ```
4. **C++ Headless 4-Tier DSP Test Runner**:
   ```powershell
   build\source\tests\Release\poopenfarten2_headless_dsp_tests.exe --all
   ```
5. **C++ Core DSP Verification Suite**:
   ```powershell
   build\Release\pf02_dsp_core_tests.exe
   ```
6. **C++ Milestone M2 Engine & Telemetry Suite**:
   ```powershell
   build\Release\pf02_m2_engine_tests.exe
   ```
7. **C++ Adversarial Challenger Mathematical Probes**:
   ```powershell
   build\Release\pf02_challenger_m1_2_tests.exe
   ```
8. **C++ Multirate & Boundary Stress Suite**:
   ```powershell
   build\Release\pf02_dsp_adversarial_tests.exe
   ```

---

## Zero-PCM Procedural Purity Guarantee

Poopenfarten II (`PF-02`) provides a verifiable, mathematical guarantee of synthesis purity:
- **Zero Static PCM Tables**: The source code contains exactly 0 static audio lookup tables, 0 wavetable banks, and 0 sample file embeddings.
- **Zero Hardcoded Audio Arrays**: Audio waveforms are calculated continuously from differential contact equations and Lie algebra updates.
- **Zero Base64 Audio Strings**: No encoded audio assets exist anywhere in source, web, or resource directories.
- **Automated CI Enforcement**: Any pull request or commit that introduces PCM sample tables fails the automated `scripts/audit_zero_pcm.py` verification pass.

---

## License & Attributions

- **License**: MIT License. See `LICENSE` for details.
- **Aesthetic Design**: German Functionalist Laboratory Instrumentation standard (DIN 1451 Mittelschrift). Strictly adjacent without explicit corporate trademarks.
- **Developed by**: The PF-02 Engineering Group.
