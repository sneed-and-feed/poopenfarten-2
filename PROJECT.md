# Project: Poopenfarten II (`PF-02 · Darm-Akustischer & Ultrametrischer Triaden-Synthesizer`)

## Architecture
Poopenfarten II (`PF-02`) is a first-principles procedural physical-modeling aeroacoustic synthesizer combining Project DarkSnail's continuous mathematical Lie-algebraic primitives with contact mechanics, modal resonator matrix, and German functionalist laboratory industrial design (DIN 1451 Mittelschrift typography, anodized aluminum chassis, tactile precision controls).

The system is delivered in two mirrored, bit-compatible real-time implementations:
1. **Native C++20 / JUCE 8 Plugin Suite**: Headless DSP core (`pf02_dsp_core`), polyphonic voice manager, APVTS parameter system, Edge WebView2 GUI bridge, Standalone (.exe), VST3 (.vst3), CLAP (.clap), and headless test executable (`poopenfarten2_headless_dsp_tests.exe`).
2. **Zero-Install Web Audio API Application**: Client-side procedural DSP graph, AudioWorkletProcessor, German functionalist 19" studio laboratory UI, and 4-mode 60 FPS vector CRT visualizer with phosphor persistence.

### High-Level Signal Flow
```
[MIDI / MPE Input / Performance Controls (Bauchpresse / Schleimhautfeuchte)]
                             │
                             ▼
         [DECK 01 · KARNOPP-SCHLEIMHAUT & DRUCKVENTIL]
    Upstream Colonic Pressure Head $P_{\text{gut}}$ + Tissue Tension
    ├── Karnopp Velocity-Deadband Stick-Slip Friction ($v_d = 0.0015\text{ m/s}$)
    ├── Stribeck Breakaway Exponential Velocity ($v_s = 0.06\text{ m/s}$)
    ├── Hunt-Crossley Viscoelastic Tissue Impact ($F_c = k_c x^\alpha + \lambda_c x^\alpha \dot{x}$)
    └── Buchla 292 Optical Vactrol Sag Droop ($\tau_{\text{decay}}[1 + \kappa(1-g)^2]$)
                             │ Aperture $y(t)$ & Slit Air Velocity $v(t)$
                             ▼
        [DECK 02 · BELTRAMI-STRAHL & MINNAERT-PHASENFLUID]
    ├── Helical Beltrami Vortex Jet ($\nabla \times \mathbf{v} = \lambda \mathbf{v}$)
    ├── Minnaert Gas-Liquid Bubble Bank ($f = 3.28325 / R$)
    ├── Poisson Droplet Point-Process Clock ($\Delta t = -\ln(1-U)/\lambda$)
    └── Bjorklund Euclidean Peristaltic Sequencer $E(k, n)$
                             │ Wideband Aeroacoustic & Fluid Excitation Signal
                             ▼
       [DECK 03 · SO(16) TRIADEN-RESONATOR & PORZELLAN-FELD]
    ├── 16-Pole Zavalishin TPT State-Variable Filter (SVF) Bandpass Matrix
    ├── $O(N)$ Orthogonal Householder Energy Scattering Matrix ($\mathbf{H} = \mathbf{I} - \frac{2}{16}\mathbf{1}\mathbf{1}^T$)
    ├── 4 Geometric Manifolds (Sigmoid Horn, Chladni Ring, Vocal Tract, Jordan Cavity)
    ├── 5 Viscoelastic Damping Profiles (Tissue, Ceramic, Elastomer, Bronze, Cartilage)
    ├── DarkSnail Skew-Symmetric Bilinear Triad Operator $S(W, i, c_1, c_2) = -S(W, i, c_2, c_1)$
    └── Multi-Step SlowFastTriadCascade in $\mathfrak{so}(16)$ ($A = B - B^\top$) with
        exact Spherical Norm Retraction ($\|\tilde{v}\|_2 = \|v_0\|_2$, $\langle v, \delta v \rangle = 0$)
                             │ Coupled Modal State Vector $v \in \mathbb{R}^{16}$
                             ▲
       [DECK 04 · ZENON-AMYGDALA & LORENZ-ATTRAKTOR]
    ├── Audio-Rate Zeno-Amygdala Controller ($T_{\text{zeno}} \in [0, 1] \implies K \in \{1, 4, 8, 16\}$)
    └── 3D RK4 Chaotic Lorenz Attractor (Odd/Even Detuning & Q-Spread)
                             │
                             ▼
       [DECK 05 · DIMENSION-D BBD & HERMITE-BEGRENZER]
    ├── Golden-Ratio Stereo Modal Panning ($\Phi = 137.508^\circ$)
    ├── Roland Dimension D Tri-Phase BBD Chorus ($0^\circ, 120^\circ, 240^\circ$, mono cancellation immune)
    ├── 15 Hz Infrasonic DC Blocker & Visceral Sub-Bass Reinforcement
    └── Hermite $C^1$ Soft-Knee BoundedSaturator + True-Peak Brickwall Limiter ($\le 1.05$ peak)
                             │
                             ▼
                 [Stereo Audio Output Bus]
                             │
                             ▼ (Lock-Free SPSC Ring Buffer / MessagePort)
          [BESPOKE 4-MODE 60 FPS VECTOR CRT VISUALIZER]
    ├── Mode 1: BELTRAMI-KAMMER (Aperture, Vortex Streamlines, Bubble Nucleation, Jordan Waves)
    ├── Mode 2: SO(16) TRIADEN-NETZ ($S^{15}$ Modal Nodes, $\mathfrak{so}(16)$ Energy Arcs, Drift Readout)
    ├── Mode 3: ZENON-LORENZ (3D Lorenz Attractor Orbit, Zeno Conflict Meter, $K \in \{1, 4, 8, 16\}$)
    └── Mode 4: SPEKTRUM / LISSAJOUS (16 Modal Partial Bars, XY Vector Lissajous Scope, Peak Meters)
```

---

## Feature Inventory

Every requirement and sub-feature from the Survey phase is enumerated here with assigned milestones:

| # | Feature | Description | Milestone | Source |
|---|---------|-------------|:---------:|--------|
| 1 | Karnopp Stick-Slip Friction | Velocity deadband ($v_d = 0.0015\text{ m/s}$), stiction balance, normal-force scaling | M1 | Survey 2 |
| 2 | Stribeck Breakaway Dynamics | Exponential breakaway velocity curve ($v_s = 0.06\text{ m/s}$), seal-burst transients | M1 | Survey 2 |
| 3 | Hunt-Crossley Tissue Impact | Hertzian restitution $F_c = k_c x^\alpha + \lambda_c x^\alpha \dot{x}$, Verlet integration, tissue slap | M1 | Survey 2 |
| 4 | Buchla 292 Optical Vactrol Sag | Dual-time-constant vactrol droop $\tau_{\text{decay}}[1 + \kappa(1-g)^2]$, fast attack $\approx 2\text{ ms}$ | M1 | Survey 2 |
| 5 | Beltrami Helical Vortex Jet | Helical vortex swirl $\nabla \times \mathbf{v} = \lambda \mathbf{v}$ modulated by Reynolds number and Beltrami flux | M1 | Survey 1, 2 |
| 6 | Minnaert Gas-Liquid Bubble Bank | Procedural resonant bubble synthesis $f = 3.28325 / R$, exponential damping, multivoice pool | M1 | Survey 2 |
| 7 | Poisson Droplet Point-Process | Stochastic interval clock $\Delta t = -\ln(1-U)/\lambda$, Irwin-Hall droplet mass | M1 | Survey 2 |
| 8 | Bjorklund Euclidean Sequencer | Peristaltic rhythmic pulse generator $E(k, n)$ driving periodic mucosal bursts | M1 | Survey 2 |
| 9 | 16-Pole Zavalishin TPT SVF Matrix | Topology-preserving transform state-variable bandpass filter bank with radiation resistance | M1 | Survey 2 |
| 10 | $O(N)$ Householder Scattering | Orthogonal energy conservation matrix $\mathbf{H} = \mathbf{I} - \frac{2}{16}\mathbf{1}\mathbf{1}^T$ with acyclic tail diffusion | M1 | Survey 2 |
| 11 | DarkSnail Skew-Symmetric Triad | Bilinear triad operator $S(W, i, c_1, c_2) = -S(W, i, c_2, c_1)$, $\langle v, S(u, v) \rangle = 0$ | M1 | Survey 1 |
| 12 | $\mathfrak{so}(16)$ Lie Generators | Lie algebra skew-symmetric matrix representation $A = \frac{1}{2}(B - B^\top)$, commutator curvature | M1 | Survey 1 |
| 13 | SlowFastTriadCascade | Multi-step tangent updates with exact spherical norm retraction $\|\tilde{v}\|_2 = \|v_0\|_2$ ($0.000\text{e}+00$ drift) | M1 | Survey 1 |
| 14 | Zeno-Amygdala Adaptive Controller | Dynamic unroll scheduling $K \in \{1, 4, 8, 16\}$ driven by Zeno triad flux and pressure conflict | M1 | Survey 1 |
| 15 | 3D RK4 Lorenz Strange Attractor | 4th-order Runge-Kutta integration of chaotic Lorenz attractor modulating modal frequencies and Q | M1 | Survey 2 |
| 16 | 4 Geometric Manifolds | Sigmoid Hyperbolic Horn, Biharmonic Chladni Ring, Glottal-Colonic Tract, Porcelain Jordan Cavity | M1 | Survey 1, 3 |
| 17 | 5 Viscoelastic Damping Profiles | Mucosal Tissue, Porcelain Ceramic, Wet Elastomer, Bell Bronze, Stiff Cartilage | M1 | Survey 2, 3 |
| 18 | Roland Dimension D Tri-Phase Chorus | 3 BBD delay lines ($0^\circ, 120^\circ, 240^\circ$), Catmull-Rom interpolation, NE570 compander, mono-sum safe | M1 | Survey 2 |
| 19 | Golden-Ratio Stereo Modal Panning | Constant-power dispersion law $\Phi = 137.507764^\circ$ across 16 modal outputs | M1 | Survey 2 |
| 20 | Infrasonic Filter & Sub Reinforce | 15 Hz DC blocking highpass filter, phase-locked visceral sub-bass reinforcement | M1 | Survey 2, 3 |
| 21 | Hermite BoundedSaturator & Limiter | $C^1$ cubic soft-knee saturator with strict asymptotic ceiling containment ($\le 1.05$ peak) | M1 | Survey 2 |
| 22 | Spectral Modal Extractor | Radix-2 FFT peak picking + parabolic sub-bin interpolation + Beltrami parameter estimation | M1 | Survey 2 |
| 23 | 40-Parameter APVTS Matrix | 38 engine parameters + 2 XY macros with standardized IDs, ranges, defaults, and groupings | M2 | Survey 3 |
| 24 | 16 Curated Factory Presets | Diverse sound design taxonomy in code and RFC 8259 JSON format | M2 | Survey 3 |
| 25 | Polyphonic Voice Architecture | Mono-Legato (with portamento & priority stack), 8-Voice Polyphonic, and Stereo Unison Detune | M2 | Survey 3 |
| 26 | Real-Time Telemetry Pipeline | Lock-free SPSC ring buffer in C++ and AudioWorklet MessagePort in Web (60 Hz telemetry frames) | M2 | Survey 2, 3 |
| 27 | Standalone Native Executable | JUCE 8 standalone application target (`poopenfarten2_standalone.exe`) with WebView2 UI | M3 | Survey 2, 3 |
| 28 | VST3 Plugin Target | Native Windows x64 VST3 plugin bundle (`poopenfarten2.vst3`) | M3 | Survey 2, 3 |
| 29 | CLAP Plugin Target | Native Windows x64 CLAP plugin bundle (`poopenfarten2.clap`) via `clap-juce-extensions` | M3 | Survey 2, 3 |
| 30 | Headless C++ Test Runner Target | CMake executable (`poopenfarten2_headless_dsp_tests.exe`) executing Tiers 1-4 DSP test suites | M3 | Survey 2, 3 |
| 31 | German Functionalist Hardware UI | 19" 2U studio laboratory chassis, Light (`#ECEBE4`) and Dark (`#141517`) themes, DIN 1451 font | M4 | Survey 2, 3 |
| 32 | Interactive Controls & Macro Pad | Knurled rotary knobs, illuminated safety rocker switch, 2D XY macro pad, kinetic burst buttons | M4 | Survey 3 |
| 33 | 16-Key Microtonal Keyboard Strip | Onboard musical keyboard with scale quantization, pitch bend, and peristaltic pulse trigger | M4 | Survey 3 |
| 34 | Onboard 16-bit Master WAV Recorder | Client-side WAV recording and instant audio file export | M4 | Survey 3 |
| 35 | CRT Mode 1: BELTRAMI-KAMMER | Live cross-section of mucosal aperture, Beltrami streamlines, Minnaert bubbles, Jordan standing waves | M4 | Survey 3 |
| 36 | CRT Mode 2: SO(16) TRIADEN-NETZ | 16 modal nodes on $S^{15}$, skew-symmetric $\mathfrak{so}(16)$ energy transfer arcs, radial drift readout | M4 | Survey 3 |
| 37 | CRT Mode 3: ZENON-LORENZ | 3D vector wireframe Lorenz attractor, Zeno conflict meter, illuminated $K \in \{1, 4, 8, 16\}$ unroll boxes | M4 | Survey 3 |
| 38 | CRT Mode 4: SPEKTRUM / LISSAJOUS | 16 ballistic modal bars, XY vector Lissajous stereo scope, peak headroom metering | M4 | Survey 3 |
| 39 | Procedural Web Audio Engine | 100% procedural Web Audio / JS DSP engine matching C++ algorithms (zero sample banks) | M5 | Survey 3 |
| 40 | Web AudioWorklet Real-Time Graph | AudioWorkletProcessor executing real-time voice graph and streaming telemetry | M5 | Survey 3 |
| 41 | Automated Node.js Verification Suite | `npm run verify:all` (`verify.mjs`) testing parameter parity, presets, math invariants, UI bindings | M5 | Survey 3 |
| 42 | Local Web Server & Launch Script | `server.js` and `start.bat` for instant zero-install local execution | M5 | Survey 3 |
| 43 | Automated Zero-PCM Source Audit | Automated Python script verifying zero static audio arrays, sample banks, or Base64 PCM strings | M6 | Survey 3 |
| 44 | Headless C++ 4-Tier Test Pass | 100% pass on Features, Boundaries, Invariants, and Adversarial Allocation tests | M6 | Survey 3 |
| 45 | Tier 5 Adversarial Coverage Hardening | White-box gap analysis, edge case probes, and adversarial fuzz tests | M6 | Survey 3 |
| 46 | Complete Documentation | `README.md`, mathematical `ARCHITECTURE.md` with Mermaid diagrams, `VERIFICATION_CHECKLIST.md` | M7 | Survey 3 |
| 47 | Git Repository Initialization & Push | Initialized git repo, committed full clean tree, pushed to `https://github.com/sneed-and-feed/poopenfarten-2` | M7 | Survey 3 |

*Cross-Check Verification*: 47 / 47 features assigned. 0 unassigned features. Check passed!

---

## Milestones

| # | Name | Scope | Dependencies | Status |
|---|------|-------|:------------:|:------:|
| **M1** | Mathematical & Physical Modeling DSP Core | C++20 decoupled `pf02_dsp_core` static library (`source/dsp/`): Karnopp/Stribeck exciter, Hunt-Crossley impact, Buchla 292 sag, Beltrami/Minnaert fluid, 16-pole Zavalishin TPT SVF, Householder scattering, DarkSnail $\mathfrak{so}(16)$ TriadCascade with spherical norm retraction, Zeno-Amygdala controller, 3D RK4 Lorenz attractor, Dimension D chorus, Golden-Ratio panner, Hermite BoundedSaturator, and Spectral Modal Extractor. | None | **DONE** |
| **M2** | Voice Architecture, Engine & APVTS Parameters | `source/engine/` & `source/parameters/`: 40-parameter APVTS layout, 16 curated factory presets, `PhysicalVoice`, `VoiceManager` (Mono-Legato, Poly 8-Voice, Stereo Unison), `Pf02AudioEngine`, lock-free SPSC telemetry queue, zero-allocation enforcement. | M1 | **DONE** |
| **M3** | JUCE 8 CMake Plugin Targets & Headless Runner | `CMakeLists.txt`, `source/plugin/`: `PluginProcessor`, `PluginEditor` (Edge WebView2 bridge), Standalone (.exe), VST3 (.vst3), CLAP (.clap), and headless test executable (`poopenfarten2_headless_dsp_tests.exe`). | M1, M2 | **DONE** |
| **M4** | German Functionalist Hardware UI & 4-Mode Vector CRT | `web/`: 19" 2U studio chassis HTML5/CSS (Light/Dark themes, DIN 1451 font), knurled knobs, safety rocker switch, 2D XY macro pad, kinetic triggers, 16-key keyboard, preset manager, WAV recorder, and 4-mode 60 FPS vector CRT visualizer with phosphor persistence. | None | **DONE** |
| **M5** | Web Audio API Engine & Node.js Verification | `web/js/audio/`: Mirrored procedural DSP engine, AudioWorkletProcessor, `server.js`, `start.bat`, and automated Node.js test suite (`npm run verify:all`). | M1, M2, M4 | **DONE** |
| **M6** | Final Verification: 100% E2E Test Pass & Tier 5 Hardening | Execute 100% of headless C++ test suite (Tiers 1-4) and Web test suite (`npm run verify:all`); execute Phase 2 Tier 5 Adversarial Coverage Hardening; pass automated zero-PCM audit. | M3, M5, E2E-Track | **IN_PROGRESS** |
| **M7** | Documentation, Release Packaging & Git Delivery | Complete `README.md`, `ARCHITECTURE.md` (with Mermaid diagrams and math derivations), `VERIFICATION_CHECKLIST.md`, initialize Git, commit all code and build artifacts, push to `https://github.com/sneed-and-feed/poopenfarten-2`. | M6 | **IN_PROGRESS** |

### Parallel Dual Track: E2E Testing Track
- **Scope**: Requirements-driven, opaque-box test suite independent of internal implementation code.
  - Test harness creation: C++ headless test harness and Node.js test harness.
  - Tiers 1-4 test case implementation per specifications:
    - Tier 1: Feature Coverage ($\ge 5$ per feature across all 47 features)
    - Tier 2: Boundary & Corner Cases (multirate 44.1 kHz - 192 kHz, buffer sizes 1 - 2048, parameter extremes)
    - Tier 3: Cross-Feature Pairwise Interactions & Mathematical Invariants (Triad antisymmetry, norm conservation, Householder conservation, mono chorus immunity)
    - Tier 4: Real-World Application Scenarios (16 factory presets, long sustained notes, rapid polyphonic bursts)
  - Publishes `TEST_READY.md` upon completion.

---

## Interface Contracts

### 1. `pf02_dsp_core` ↔ `VoiceManager` / `PhysicalVoice`
```cpp
namespace pf02::dsp {
    // Single Voice Physical Synthesizer State & Processor
    class PhysicalVoice {
    public:
        void prepare(double sampleRate) noexcept;
        void reset() noexcept;
        void noteOn(int midiNote, float velocity) noexcept;
        void noteOff(float velocity) noexcept;
        void setParameter(ParameterId id, float value) noexcept;
        
        // Generates one audio sample of physical excitation & resonance
        // Zero audio-thread allocations; zero Lookahead latency
        float processSample(float externalPressureMod = 0.0f) noexcept;
        
        bool isActive() const noexcept;
        float getAperture() const noexcept;
        float getColonicPressure() const noexcept;
        float getAirVelocity() const noexcept;
        const std::array<float, 16>& getModalAmplitudes() const noexcept;
    };
}
```

### 2. `Pf02AudioEngine` ↔ `PluginProcessor` / `TelemetryQueue`
```cpp
namespace pf02::engine {
    struct VisualizerTelemetryFrame {
        std::array<float, 128> waveformL;
        std::array<float, 128> waveformR;
        float aperture;
        float colonicPressure;
        float airVelocity;
        float bubbleIntensity;
        bool  dropletTriggered;
        float zenoFlux;
        int   activeUnrollK; // 1, 4, 8, 16
        float lorenzX, lorenzY, lorenzZ;
        std::array<float, 16> modalEnergies;
        std::array<float, 16> triadEnergyFlow;
        float sphericalNormDrift; // guaranteed < 1e-6
        float peakLevelDb;
        float rmsLevelDb;
    };

    class Pf02AudioEngine {
    public:
        void prepare(double sampleRate, int maxBlockSize) noexcept;
        void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) noexcept;
        bool popTelemetry(VisualizerTelemetryFrame& outFrame) noexcept;
        void setParameter(std::string_view paramId, float value) noexcept;
    };
}
```

### 3. C++ APVTS ↔ Web Audio API Parameter Contract
- Exact 1:1 mapping between 40 APVTS parameter IDs and Web Audio `AudioParam` / engine properties (see Feature Inventory #23 and Survey 3 Table 4.1).
- Range, default, unit, and scaling exponent are identical across C++ and JavaScript.

---

## Code Layout

```
poopenfarten-2/
├── CMakeLists.txt                    # Root CMake build configuration (JUCE 8, Standalone, VST3, CLAP, Tests)
├── package.json                      # Web Audio & Node.js test configuration (npm test / npm run verify:all)
├── server.js                         # Lightweight zero-dependency static server for web application
├── start.bat                         # Instant 1-click launcher for Web Audio app
├── README.md                         # Comprehensive instrument overview, architecture, quickstart
├── ARCHITECTURE.md                   # Full mathematical derivations, Mermaid signal-flow diagrams
├── VERIFICATION_CHECKLIST.md         # Complete sign-off matrix for all 47 features and criteria
├── scripts/
│   ├── audit_zero_pcm.py             # CI script scanning for static PCM sample arrays / shortcuts
│   └── run_all_verifications.bat     # Master batch runner for C++ and Web test suites
├── source/
│   ├── dsp/                          # Decoupled C++20 physical modeling DSP library (pf02_dsp_core)
│   │   ├── DspMath.h                 # Utilities, Householder scattering, Hermite spline, flushDenormal
│   │   ├── DarkSnailMath.h           # Bilinear triad operator, so(16) Lie generators, SlowFastTriadCascade
│   │   ├── MucosalExciter.h/.cpp     # Karnopp stick-slip, Stribeck breakaway, Hunt-Crossley, Buchla 292
│   │   ├── AeroFluidEngine.h/.cpp    # Beltrami vortex jet, Minnaert bubbles, Poisson & Euclidean clocks
│   │   ├── ModalResonatorMatrix.h/.cpp# 16-pole Zavalishin TPT SVF, manifolds, damping, Householder
│   │   ├── LorenzAttractor.h/.cpp    # 3D RK4 chaotic strange attractor
│   │   ├── MasteringChain.h/.cpp     # Roland Dimension D BBD, Golden-Ratio panner, Hermite BoundedSaturator
│   │   └── SpectralModalExtractor.h/.cpp # FFT peak picking & Beltrami parameter extractor
│   ├── engine/                       # Polyphonic synthesizer engine
│   │   ├── PhysicalVoice.h/.cpp      # Single-voice coupled physical-modeling pipeline
│   │   ├── VoiceManager.h/.cpp       # Mono-Legato, Poly 8-Voice, Stereo Unison management
│   │   └── Pf02AudioEngine.h/.cpp    # Master audio engine coordinating voices, FX, SPSC telemetry
│   ├── parameters/                   # Parameter definitions and preset storage
│   │   ├── Parameters.h              # 40-parameter APVTS layout and constant string identifiers
│   │   └── PresetManager.h/.cpp      # 16 curated factory presets & JSON import/export
│   ├── plugin/                       # JUCE 8 plugin wrappers & UI bridge
│   │   ├── PluginProcessor.h/.cpp    # AudioProcessor lifecycle, APVTS, MIDI, SPSC queue
│   │   └── PluginEditor.h/.cpp       # AudioProcessorEditor hosting Edge WebView2 / WebKit bridge
│   └── tests/                        # Headless C++ DSP verification test suite
│       └── pf02_headless_dsp_tests.cpp # 4-tier C++ test runner (Features, Boundaries, Invariants, Stress)
└── web/                              # 100% Client-Side Zero-Install Web Audio Application
    ├── index.html                    # German functionalist laboratory UI & 4-mode vector CRT visualizer
    ├── css/
    │   └── style.css                 # 19" 2U studio chassis styling, DIN 1451 Mittelschrift, tokens
    ├── js/
    │   ├── main.js                   # Application bootstrap, event coordination, theme toggle
    │   ├── ui/                       # Tactile knobs, switches, XY macro pad, keyboard, presets, recorder
    │   │   ├── knobs.js
    │   │   ├── macro_pad.js
    │   │   ├── keyboard.js
    │   │   ├── preset_manager.js
    │   │   └── wav_recorder.js
    │   ├── visualizer/               # 4-mode 60 FPS hardware-accelerated vector CRT oscilloscope
    │   │   └── crt_visualizer.js
    │   └── audio/                    # Mirrored procedural physical modeling DSP in Web Audio
    │       ├── pf02_dsp_engine.js
    │       ├── pf02_audio_worklet.js
    │       └── modal_extractor.js
    └── verify.mjs                    # Automated Node.js test suite for web application
```
