/**
 * @file main.js
 * @brief Main Bootstrap Coordinator & WebView2 Bridge for PF-02
 * German Functionalist Laboratory Instrumentation
 */

import { PfKnob } from './ui/knobs.js';
import { PfMacroPad } from './ui/macro_pad.js';
import { PfKeyboard } from './ui/keyboard.js';
import { PfPresetManager, FACTORY_PRESETS } from './ui/preset_manager.js';
import { PfWavRecorder } from './ui/wav_recorder.js';
import { PfCrtVisualizer } from './visualizer/crt_visualizer.js';
import { Pf02DspEngine } from './audio/pf02_dsp_engine.js';
import { SpectralModalExtractor } from './audio/modal_extractor.js';

class PfApp {
  constructor() {
    this.knobs = new Map();
    this.parameters = {};
    this.theme = 'light';
    this.audioCtx = null;
    this.isMuted = false;
    this.dspEngine = null;
    this.workletNode = null;
    this.scriptNode = null;
    this.modalExtractor = new SpectralModalExtractor();
    this.audioInitialized = false;

    // Initialize all 40 APVTS parameters with defaults
    this._initDefaultParameters();
  }

  _initDefaultParameters() {
    // Exact defaults from Survey 3 Table 4.1
    this.parameters = {
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
      param_geometric_manifold: 2, // Vocal
      param_damping_profile: 0,   // Tissue
      param_porcelain_cavity_mix: 35.0,
      param_porcelain_boundary_mode: 1, // Standard
      param_zeno_unroll_override: 0, // Auto
      param_zeno_conflict_threshold: 0.55,
      param_lorenz_orbit_rate: 0.65,
      param_lorenz_chaos_depth: 25.0,
      param_dimension_d_enable: 1,
      param_dimension_d_mode: 1, // Mode II
      param_dimension_d_mix: 40.0,
      param_colonic_saturation_drive: 30.0,
      param_visceral_sub_reinforce: 0.0,
      param_golden_stereo_spread: 75.0,
      param_voicing_mode: 0, // Mono
      param_glide_portamento_ms: 45.0,
      param_master_volume_db: 0.0,
      param_power_active: 1,
      macro_gut_squeeze: 0.50,
      macro_fluid_moisture: 0.50
    };
  }

  init() {
    this._initVisualizer();
    this._initPresetManager();
    this._initKnobs();
    this._initChoiceButtons();
    this._initMacroPad();
    this._initKeyboard();
    this._initBurstButtons();
    this._initThemeAndPower();
    this._initWavRecorder();
    this._initWebView2Bridge();
    this._initMockTelemetryGenerator();
    this._initDragDrop();
    this._initUserAudioTrigger();
  }

  // --- Visualizer Initialization ---
  _initVisualizer() {
    const canvas = document.getElementById('crt-canvas');
    if (!canvas) return;

    this.visualizer = new PfCrtVisualizer(canvas, {
      mode: 'BELTRAMI-KAMMER',
      phosphorType: 'GREEN_P1',
      intensity: 85
    });

    // CRT Mode buttons
    const modeBtns = document.querySelectorAll('#group-crt-mode .pf02-segment-btn');
    modeBtns.forEach(btn => {
      btn.addEventListener('click', () => {
        modeBtns.forEach(b => b.classList.remove('is-selected'));
        btn.classList.add('is-selected');
        const mode = btn.dataset.val;
        this.visualizer.setMode(mode);
      });
    });

    // Phosphor Type buttons
    const phosphorBtns = document.querySelectorAll('#group-phosphor-type .pf02-segment-btn');
    phosphorBtns.forEach(btn => {
      btn.addEventListener('click', () => {
        phosphorBtns.forEach(b => b.classList.remove('is-selected'));
        btn.classList.add('is-selected');
        const type = btn.dataset.val;
        this.visualizer.setPhosphorType(type);
      });
    });
  }

  // --- Preset Manager & Patch Bar ---
  _initPresetManager() {
    const selectPreset = document.getElementById('select-preset');
    this.presetManager = new PfPresetManager({
      onApplyPreset: (preset) => {
        this.applyPreset(preset);
      }
    });

    if (selectPreset) {
      selectPreset.innerHTML = '';
      const factoryGroup = document.createElement('optgroup');
      factoryGroup.label = '--- 16 FACTORY PRESETS ---';
      FACTORY_PRESETS.forEach(p => {
        const opt = document.createElement('option');
        opt.value = p.id;
        opt.textContent = `${p.category.toUpperCase()} · ${p.name}`;
        factoryGroup.appendChild(opt);
      });
      selectPreset.appendChild(factoryGroup);

      selectPreset.addEventListener('change', (e) => {
        this.presetManager.applyPreset(e.target.value);
      });
    }

    // Save, Load, Export, Reset buttons
    const btnSave = document.getElementById('btn-save-patch');
    if (btnSave) {
      btnSave.addEventListener('click', () => {
        const name = prompt("Patch-Bezeichnung eingeben:", "PF02_Benutzer_Patch");
        if (name) {
          this.presetManager.exportCurrentPatch(this.parameters, name);
          alert(`Patch "${name}" gespeichert.`);
        }
      });
    }

    const btnExport = document.getElementById('btn-export-patch');
    if (btnExport) {
      btnExport.addEventListener('click', () => {
        this.presetManager.downloadPatchFile(this.parameters, "PF02_Laboratory_Patch");
      });
    }

    const btnLoad = document.getElementById('btn-load-patch');
    const inputLoad = document.getElementById('input-load-patch');
    if (btnLoad && inputLoad) {
      btnLoad.addEventListener('click', () => inputLoad.click());
      inputLoad.addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (!file) return;
        const reader = new FileReader();
        reader.onload = (ev) => {
          try {
            const importedParams = this.presetManager.importPatchJSON(ev.target.result);
            this.setAllParameters(importedParams);
          } catch (err) {
            alert(err.message);
          }
        };
        reader.readAsText(file);
      });
    }

    const btnReset = document.getElementById('btn-reset-all');
    if (btnReset) {
      btnReset.addEventListener('click', () => {
        this.presetManager.applyPreset('PF02_DEFAULT_DARM_TRIADE');
      });
    }
  }

  applyPreset(preset) {
    if (!preset || !preset.params) return;
    this.setAllParameters(preset.params);
    const selectPreset = document.getElementById('select-preset');
    if (selectPreset) selectPreset.value = preset.id;
  }

  setAllParameters(params) {
    Object.keys(params).forEach(id => {
      const val = params[id];
      this.setParameter(id, val, false);
      const knob = this.knobs.get(id);
      if (knob) knob.setValue(val, false);
    });
    this._updateChoiceButtonsFromParams();
  }

  // --- Tactile Rotary Knobs Initialization ---
  _initKnobs() {
    const knobSpecs = [
      // Deck 01: Karnopp & Druckventil
      { id: 'param_colonic_pressure', el: 'knob-colonic-pressure', label: 'KOLONDRUCK', min: 0.0, max: 1.0, step: 0.01, unit: '', precision: 2, size: 'large' },
      { id: 'param_mucosal_tension', el: 'knob-mucosal-tension', label: 'SPANNUNG', min: 0.0, max: 1.0, step: 0.01, unit: '', precision: 2 },
      { id: 'param_resting_aperture', el: 'knob-resting-aperture', label: 'RUHEÖFFNUNG', min: 0.1, max: 5.0, step: 0.05, unit: 'mm', precision: 2 },
      { id: 'param_karnopp_stiction', el: 'knob-karnopp-stiction', label: 'HAFTREIBUNG', min: 0.0, max: 1.0, step: 0.01, unit: '', precision: 2 },
      { id: 'param_stribeck_breakaway', el: 'knob-stribeck-breakaway', label: 'ABREISS-GESCHW', min: 0.1, max: 10.0, step: 0.1, unit: 'm/s', precision: 1 },
      { id: 'param_tissue_hardness', el: 'knob-tissue-hardness', label: 'GEWEBEHÄRTE', min: 1.2, max: 3.0, step: 0.05, unit: 'α', precision: 2 },
      { id: 'param_impact_restitution', el: 'knob-impact-restitution', label: 'STOSSDÄMPFUNG', min: 0.0, max: 1.0, step: 0.01, unit: '', precision: 2 },
      { id: 'param_vactrol_sag_tau', el: 'knob-vactrol-sag-tau', label: 'VACTROL-ABSACK', min: 20.0, max: 500.0, step: 1.0, unit: 'ms', precision: 0 },

      // Deck 02: Beltrami-Strahl & Phasenfluid
      { id: 'param_beltrami_vortex_pitch', el: 'knob-beltrami-vortex-pitch', label: 'BELTRAMI-DRALL', min: 0.1, max: 10.0, step: 0.1, unit: 'rad', precision: 1 },
      { id: 'param_reynolds_turbulence', el: 'knob-reynolds-turbulence', label: 'TURBULENZ', min: 0.0, max: 1.0, step: 0.01, unit: '', precision: 2 },
      { id: 'param_multiphase_moisture', el: 'knob-multiphase-moisture', label: 'PHASENFEUCHTE', min: 0.0, max: 1.0, step: 0.01, unit: '', precision: 2 },
      { id: 'param_fluid_viscosity', el: 'knob-fluid-viscosity', label: 'VISKOSITÄT', min: 0.0, max: 1.0, step: 0.01, unit: '', precision: 2 },
      { id: 'param_minnaert_bubble_density', el: 'knob-minnaert-bubble-density', label: 'BLASENDICHTE', min: 0.0, max: 100.0, step: 0.5, unit: '%', precision: 1 },
      { id: 'param_poisson_droplet_rate', el: 'knob-poisson-droplet-rate', label: 'TROPFENRATE', min: 0.0, max: 60.0, step: 0.5, unit: 'Hz', precision: 1 },
      { id: 'param_euclidean_peristalsis_k', el: 'knob-euclidean-peristalsis-k', label: 'EUKLID-IMPULSE', min: 1, max: 16, step: 1, unit: '', precision: 0 },
      { id: 'param_euclidean_peristalsis_n', el: 'knob-euclidean-peristalsis-n', label: 'EUKLID-SCHRITTE', min: 1, max: 16, step: 1, unit: '', precision: 0 },

      // Deck 03: SO(16) Triaden-Resonator
      { id: 'param_triad_coupling_gain', el: 'knob-triad-coupling-gain', label: 'TRIADEN-KOPPLUNG', min: 0.0, max: 2.0, step: 0.01, unit: '', precision: 2, size: 'large' },
      { id: 'param_so16_skew_generator', el: 'knob-so16-skew-generator', label: 'SO(16)-GENERATOR', min: 0.0, max: 1.0, step: 0.01, unit: '', precision: 2 },
      { id: 'param_householder_coupling', el: 'knob-householder-coupling', label: 'HOUSEHOLDER', min: 0.0, max: 1.0, step: 0.01, unit: '', precision: 2 },
      { id: 'param_porcelain_cavity_mix', el: 'knob-porcelain-cavity-mix', label: 'PORZELLAN-ANTEIL', min: 0.0, max: 100.0, step: 0.5, unit: '%', precision: 1 },

      // Deck 04: Zenon-Amygdala & Lorenz
      { id: 'param_zeno_conflict_threshold', el: 'knob-zeno-conflict-threshold', label: 'KONFLIKTSCHWELLE', min: 0.1, max: 1.0, step: 0.01, unit: '', precision: 2 },
      { id: 'param_lorenz_orbit_rate', el: 'knob-lorenz-orbit-rate', label: 'LORENZ-UMLAUF', min: 0.01, max: 12.0, step: 0.05, unit: 'Hz', precision: 2 },
      { id: 'param_lorenz_chaos_depth', el: 'knob-lorenz-chaos-depth', label: 'CHAOS-TIEFE', min: 0.0, max: 100.0, step: 0.5, unit: '%', precision: 1 },

      // Deck 05: Dimension-D & Limiter
      { id: 'param_dimension_d_mix', el: 'knob-dimension-d-mix', label: 'CHORUS-ANTEIL', min: 0.0, max: 100.0, step: 0.5, unit: '%', precision: 1 },
      { id: 'param_colonic_saturation_drive', el: 'knob-colonic-saturation-drive', label: 'KOLONSÄTTIGUNG', min: 0.0, max: 100.0, step: 0.5, unit: '%', precision: 1 },
      { id: 'param_visceral_sub_reinforce', el: 'knob-visceral-sub-reinforce', label: 'SUB-VERSTÄRKUNG', min: -36.0, max: 6.0, step: 0.5, unit: 'dB', precision: 1 },

      // Deck 06: Spatial, Voicing & Master
      { id: 'param_golden_stereo_spread', el: 'knob-golden-stereo-spread', label: 'GOLDENSCHNITT', min: 0.0, max: 100.0, step: 0.5, unit: '%', precision: 1 },
      { id: 'param_glide_portamento_ms', el: 'knob-glide-portamento-ms', label: 'GLEITZEIT', min: 0.0, max: 500.0, step: 1.0, unit: 'ms', precision: 0 },
      { id: 'param_master_volume_db', el: 'knob-master-volume-db', label: 'GESAMTPEGEL', min: -36.0, max: 6.0, step: 0.5, unit: 'dB', precision: 1, size: 'large' }
    ];

    knobSpecs.forEach(spec => {
      const container = document.getElementById(spec.el);
      if (!container) return;
      const initialVal = this.parameters[spec.id] ?? spec.min;
      const knob = new PfKnob(container, {
        paramId: spec.id,
        label: spec.label,
        min: spec.min,
        max: spec.max,
        step: spec.step,
        unit: spec.unit,
        precision: spec.precision,
        size: spec.size || 'medium',
        value: initialVal,
        onChange: (val, id) => {
          this.setParameter(id, val, true);
        }
      });
      this.knobs.set(spec.id, knob);
    });
  }

  // --- Segmented Choice Selectors & Toggles ---
  _initChoiceButtons() {
    const bindGroup = (groupId, paramId) => {
      const btns = document.querySelectorAll(`#${groupId} .pf02-segment-btn`);
      btns.forEach(btn => {
        btn.addEventListener('click', () => {
          btns.forEach(b => b.classList.remove('is-selected'));
          btn.classList.add('is-selected');
          const val = parseInt(btn.dataset.val, 10);
          this.setParameter(paramId, val, true);
        });
      });
    };

    bindGroup('group-manifold', 'param_geometric_manifold');
    bindGroup('group-damping', 'param_damping_profile');
    bindGroup('group-boundary', 'param_porcelain_boundary_mode');
    bindGroup('group-zeno-unroll', 'param_zeno_unroll_override');
    bindGroup('group-dimension-mode', 'param_dimension_d_mode');
    bindGroup('group-voicing-mode', 'param_voicing_mode');

    // Boolean Toggles
    const btnChorus = document.getElementById('btn-chorus-enable');
    if (btnChorus) {
      btnChorus.addEventListener('click', () => {
        const next = this.parameters.param_dimension_d_enable ? 0 : 1;
        this.setParameter('param_dimension_d_enable', next, true);
        btnChorus.classList.toggle('is-active', next === 1);
      });
    }

    const btnRetraction = document.getElementById('btn-spherical-retraction');
    if (btnRetraction) {
      btnRetraction.addEventListener('click', () => {
        const next = this.parameters.param_spherical_retraction ? 0 : 1;
        this.setParameter('param_spherical_retraction', next, true);
        btnRetraction.classList.toggle('is-active', next === 1);
      });
    }
  }

  _updateChoiceButtonsFromParams() {
    const updateGroup = (groupId, paramId) => {
      const val = this.parameters[paramId];
      const btns = document.querySelectorAll(`#${groupId} .pf02-segment-btn`);
      btns.forEach(btn => {
        if (parseInt(btn.dataset.val, 10) === val) {
          btn.classList.add('is-selected');
        } else {
          btn.classList.remove('is-selected');
        }
      });
    };

    updateGroup('group-manifold', 'param_geometric_manifold');
    updateGroup('group-damping', 'param_damping_profile');
    updateGroup('group-boundary', 'param_porcelain_boundary_mode');
    updateGroup('group-zeno-unroll', 'param_zeno_unroll_override');
    updateGroup('group-dimension-mode', 'param_dimension_d_mode');
    updateGroup('group-voicing-mode', 'param_voicing_mode');

    const btnChorus = document.getElementById('btn-chorus-enable');
    if (btnChorus) {
      btnChorus.classList.toggle('is-active', this.parameters.param_dimension_d_enable === 1);
    }
    const btnRetraction = document.getElementById('btn-spherical-retraction');
    if (btnRetraction) {
      btnRetraction.classList.toggle('is-active', this.parameters.param_spherical_retraction === 1);
    }
  }

  // --- 2D XY Macro Pad ---
  _initMacroPad() {
    const padContainer = document.getElementById('macro-pad-container');
    if (!padContainer) return;

    this.macroPad = new PfMacroPad(padContainer, {
      x: this.parameters.macro_gut_squeeze,
      y: this.parameters.macro_fluid_moisture,
      onChange: (x, y) => {
        this.setParameter('macro_gut_squeeze', x, true);
        this.setParameter('macro_fluid_moisture', y, true);
        // Cross-modulate engine parameters
        const modulatedPressure = 0.50 + x * 0.45;
        this.setParameter('param_colonic_pressure', modulatedPressure, false);
        const knobPress = this.knobs.get('param_colonic_pressure');
        if (knobPress) knobPress.setValue(modulatedPressure, false);

        const modulatedMoisture = y;
        this.setParameter('param_multiphase_moisture', modulatedMoisture, false);
        const knobMoist = this.knobs.get('param_multiphase_moisture');
        if (knobMoist) knobMoist.setValue(modulatedMoisture, false);
      }
    });
  }

  // --- 16-Key Keyboard Strip ---
  _initKeyboard() {
    const kbContainer = document.getElementById('keyboard-container');
    if (!kbContainer) return;

    this.keyboard = new PfKeyboard(kbContainer, {
      onNoteOn: (midiNote, freqHz, velocity) => {
        this._dispatchAudioEvent('noteOn', { midiNote, freqHz, velocity });
      },
      onNoteOff: (midiNote) => {
        this._dispatchAudioEvent('noteOff', { midiNote });
      },
      onPitchBend: (cents) => {
        this._dispatchAudioEvent('pitchBend', { cents });
      }
    });
  }

  // --- 4 Kinetic Burst Triggers ---
  _initBurstButtons() {
    const bursts = [
      { id: 'btn-burst-impulse', name: 'IMPULS-SCHLAG', type: 'impulse' },
      { id: 'btn-burst-tear', name: 'KARNOPP-ABRISS', type: 'breakaway' },
      { id: 'btn-burst-splatter', name: 'MINNAERT-PLOPP', type: 'splatter' },
      { id: 'btn-burst-jordan', name: 'JORDAN-RESONANZ', type: 'cavity' }
    ];

    bursts.forEach(b => {
      const el = document.getElementById(b.id);
      if (!el) return;
      el.addEventListener('mousedown', () => {
        el.classList.add('is-firing');
        this._dispatchAudioEvent('kineticBurst', { burstType: b.type, velocity: 1.0 });
        if (this.visualizer) {
          this.visualizer.telemetry.dropletTriggered = true;
          this.visualizer._spawnDropletBurst();
        }
      });
      el.addEventListener('mouseup', () => el.classList.remove('is-firing'));
      el.addEventListener('mouseleave', () => el.classList.remove('is-firing'));
    });
  }

  // --- Theme Toggle & Master Power Switch ---
  _initThemeAndPower() {
    const selectTheme = document.getElementById('select-theme');
    if (selectTheme) {
      selectTheme.addEventListener('change', (e) => {
        this.theme = e.target.value;
        document.documentElement.setAttribute('data-theme', this.theme);
      });
    }

    const btnPower = document.getElementById('btn-power-switch');
    if (btnPower) {
      btnPower.addEventListener('click', () => {
        const next = this.parameters.param_power_active ? 0 : 1;
        this.setParameter('param_power_active', next, true);
        btnPower.classList.toggle('is-active', next === 1);
        const statusText = btnPower.querySelector('.pf02-power-status');
        if (statusText) statusText.textContent = next === 1 ? 'AKTIV' : 'STANDBY';
        if (this.visualizer) {
          this.visualizer.setPower(next === 1);
        }
      });
    }
  }

  // --- WAV Recorder ---
  _initWavRecorder() {
    const btnRecord = document.getElementById('btn-record-wav');
    if (!btnRecord) return;

    this.recorder = new PfWavRecorder(this.audioCtx, {
      onStateChange: (recording, duration) => {
        btnRecord.classList.toggle('is-recording', recording);
        const label = btnRecord.querySelector('.pf02-rec-text');
        if (label) {
          label.textContent = recording ? 'AUFNAHME...' : 'REC WAV';
        }
      }
    });

    btnRecord.addEventListener('click', () => {
      if (!this.recorder.isRecording) {
        this.recorder.start();
      } else {
        this.recorder.stop();
        this.recorder.downloadWav('PF02_Master_Recording.wav');
      }
    });
  }

  // --- Drag & Drop: RFC 8259 JSON Patch or Spectral Modal Extractor Audio File ---
  _initDragDrop() {
    const chassis = document.getElementById('pf02-chassis');
    if (!chassis) return;

    chassis.addEventListener('dragover', (e) => {
      e.preventDefault();
      chassis.classList.add('is-drag-over');
    });

    chassis.addEventListener('dragleave', (e) => {
      if (e.relatedTarget === null || !chassis.contains(e.relatedTarget)) {
        chassis.classList.remove('is-drag-over');
      }
    });

    chassis.addEventListener('drop', (e) => {
      e.preventDefault();
      chassis.classList.remove('is-drag-over');
      const file = e.dataTransfer.files[0];
      if (!file) return;

      const isAudio = file.name.endsWith('.wav') || file.type.startsWith('audio/');
      if (isAudio) {
        const reader = new FileReader();
        reader.onload = (ev) => {
          try {
            const arrayBuf = ev.target.result;
            const parsed = SpectralModalExtractor.parseWavBuffer(arrayBuf);
            const result = this.modalExtractor.analyzeBuffer(parsed.audio, parsed.sampleRate);
            const newParams = {
              param_colonic_pressure: 0.70,
              param_geometric_manifold: result.manifoldIndex,
              param_triad_coupling_gain: result.estimatedTriadCoupling,
              param_zeno_conflict_threshold: result.estimatedZenoFlux,
              param_porcelain_cavity_mix: result.bestManifold === 'PorcelainBowl' ? 70.0 : 35.0,
              param_damping_profile: result.bestManifold === 'PorcelainBowl' ? 1 : 0
            };
            this.setAllParameters(newParams);
            console.log(`[SpectralModalExtractor] Analyzed "${file.name}": ${result.bestManifold} (conf: ${(result.manifoldConfidence * 100).toFixed(1)}%, f0: ${result.fundamentalHz.toFixed(1)} Hz)`);
          } catch (err) {
            console.error('SpectralModalExtractor analysis error:', err);
            alert('Modal Extractor Error: ' + err.message);
          }
        };
        reader.readAsArrayBuffer(file);
        return;
      }

      // JSON Patch Preset Import
      const reader = new FileReader();
      reader.onload = (ev) => {
        try {
          const importedParams = this.presetManager.importPatchJSON(ev.target.result);
          this.setAllParameters(importedParams);
        } catch (err) {
          alert(err.message);
        }
      };
      reader.readAsText(file);
    });
  }

  // --- Audio Lifecycle & Procedural Engine Integration ---
  _initUserAudioTrigger() {
    const trigger = () => {
      this._ensureAudioContext();
    };
    window.addEventListener('pointerdown', trigger, { once: true });
    window.addEventListener('keydown', trigger, { once: true });
  }

  async _ensureAudioContext() {
    if (this.audioCtx) {
      if (this.audioCtx.state === 'suspended') {
        await this.audioCtx.resume();
      }
      return;
    }

    const AudioContextClass = window.AudioContext || window.webkitAudioContext;
    if (!AudioContextClass) return;

    this.audioCtx = new AudioContextClass();
    if (this.audioCtx.state === 'suspended') {
      await this.audioCtx.resume();
    }
    await this._initAudioEngine();
  }

  async _initAudioEngine() {
    if (this.audioInitialized) return;

    // Try AudioWorklet first
    if (this.audioCtx && this.audioCtx.audioWorklet) {
      try {
        await this.audioCtx.audioWorklet.addModule('js/audio/pf02_audio_worklet.js');
        this.workletNode = new AudioWorkletNode(this.audioCtx, 'pf02-audio-worklet');
        this.workletNode.port.onmessage = (e) => {
          if (e.data && e.data.type === 'telemetryFrame') {
            window.__HAS_LIVE_TELEMETRY__ = true;
            if (this.visualizer) {
              this.visualizer.setTelemetry(e.data.frame);
            }
          }
        };

        this.workletNode.connect(this.audioCtx.destination);
        if (this.recorder) {
          this.recorder.audioCtx = this.audioCtx;
          this.recorder.attachSource(this.workletNode);
        }

        // Synchronize all 40 parameters
        for (const [k, v] of Object.entries(this.parameters)) {
          this.workletNode.port.postMessage({ type: 'paramChange', paramId: k, value: v });
        }

        this.audioInitialized = true;
        return;
      } catch (err) {
        console.warn('[PF-02] AudioWorklet initialization fallback to procedural engine:', err);
      }
    }

    // Procedural DSP Engine Fallback (works in offline/file:// mode)
    if (this.audioCtx) {
      this.dspEngine = new Pf02DspEngine(this.audioCtx.sampleRate);
      for (const [k, v] of Object.entries(this.parameters)) {
        this.dspEngine.setParameter(k, v);
      }

      if (this.audioCtx.createScriptProcessor) {
        this.scriptNode = this.audioCtx.createScriptProcessor(512, 0, 2);
        this.scriptNode.onaudioprocess = (e) => {
          const outL = e.outputBuffer.getChannelData(0);
          const outR = e.outputBuffer.getChannelData(1);
          const { left, right } = this.dspEngine.processBlock(outL.length);
          outL.set(left);
          outR.set(right);
          window.__HAS_LIVE_TELEMETRY__ = true;
          if (this.visualizer) {
            this.visualizer.setTelemetry(this.dspEngine.getTelemetry());
          }
        };
        this.scriptNode.connect(this.audioCtx.destination);
        if (this.recorder) {
          this.recorder.audioCtx = this.audioCtx;
          this.recorder.attachSource(this.scriptNode);
        }
      }
      this.audioInitialized = true;
    }
  }

  // --- Parameter Dispatch & WebView2 Bridge ---
  setParameter(paramId, value, broadcast = true) {
    this.parameters[paramId] = value;

    if (this.workletNode) {
      this.workletNode.port.postMessage({
        type: 'paramChange',
        paramId: paramId,
        value: value
      });
    } else if (this.dspEngine) {
      this.dspEngine.setParameter(paramId, value);
    }

    if (broadcast) {
      // 1. Post to WebView2 if running inside JUCE PluginEditor
      if (window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
        window.chrome.webview.postMessage({
          type: 'paramChange',
          paramId: paramId,
          value: value
        });
      }

      // 2. Dispatch custom event for Web Audio engine
      window.dispatchEvent(new CustomEvent('pf02:paramChange', {
        detail: { paramId, value }
      }));
    }
  }

  _dispatchAudioEvent(eventType, payload) {
    this._ensureAudioContext();

    if (this.workletNode) {
      if (eventType === 'noteOn') {
        this.workletNode.port.postMessage({ type: 'noteOn', note: payload.midiNote, velocity: payload.velocity });
      } else if (eventType === 'noteOff') {
        this.workletNode.port.postMessage({ type: 'noteOff', note: payload.midiNote });
      } else if (eventType === 'pitchBend') {
        this.workletNode.port.postMessage({ type: 'pitchBend', semitones: (payload.cents || 0) / 100.0 });
      } else if (eventType === 'kineticBurst') {
        this.workletNode.port.postMessage({ type: 'kineticBurst', burstType: payload.burstType, velocity: payload.velocity });
      }
    } else if (this.dspEngine) {
      if (eventType === 'noteOn') {
        this.dspEngine.noteOn(payload.midiNote, payload.velocity);
      } else if (eventType === 'noteOff') {
        this.dspEngine.noteOff(payload.midiNote);
      } else if (eventType === 'pitchBend') {
        this.dspEngine.pitchBend((payload.cents || 0) / 100.0);
      } else if (eventType === 'kineticBurst') {
        const v = this.dspEngine.voiceManager.voices[0];
        if (payload.burstType === 'impulse') {
          v.mucosalExciter.triggerSlap(payload.velocity, 2.8);
          v.noteOn(33, payload.velocity);
        } else if (payload.burstType === 'breakaway') {
          v.mucosalExciter.triggerSealBurst(payload.velocity * 2.0);
          v.noteOn(33, payload.velocity);
        } else if (payload.burstType === 'splatter') {
          v.aeroFluidEngine.triggerFlapBurst(payload.velocity, 0.9, 30.0);
          v.noteOn(33, payload.velocity);
        } else if (payload.burstType === 'cavity') {
          v.modalMatrix.jordanField.generateCavityField(1.5, 0.8, 3);
          v.noteOn(33, payload.velocity);
        }
      }
    }

    if (window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
      window.chrome.webview.postMessage({
        type: eventType,
        ...payload
      });
    }

    window.dispatchEvent(new CustomEvent(`pf02:${eventType}`, {
      detail: payload
    }));
  }

  _initWebView2Bridge() {
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.addEventListener('message', (event) => {
        const msg = event.data;
        if (!msg) return;

        if (msg.type === 'telemetryFrame' && this.visualizer) {
          this.visualizer.setTelemetry(msg.frame);
        } else if (msg.type === 'paramUpdate') {
          this.setParameter(msg.paramId, msg.value, false);
          const knob = this.knobs.get(msg.paramId);
          if (knob) knob.setValue(msg.value, false);
        }
      });
    }
  }

  // --- Mock Telemetry Generator for Standalone Visualizer Testing ---
  _initMockTelemetryGenerator() {
    let phase = 0;
    let lorenzX = 0.1, lorenzY = 0.0, lorenzZ = 0.0;
    const dt = 0.012;
    const sigma = 10.0, rho = 28.0, beta = 8.0 / 3.0;

    setInterval(() => {
      // If live engine telemetry is arriving via WebView2, do not override
      if (window.__HAS_LIVE_TELEMETRY__) return;

      phase += 0.08;

      // Lorenz integration
      const dx = sigma * (lorenzY - lorenzX);
      const dy = lorenzX * (rho - lorenzZ) - lorenzY;
      const dz = lorenzX * lorenzY - beta * lorenzZ;
      lorenzX += dx * dt;
      lorenzY += dy * dt;
      lorenzZ += dz * dt;

      // 128-sample waveforms
      const waveL = new Float32Array(128);
      const waveR = new Float32Array(128);
      for (let i = 0; i < 128; i++) {
        const t = phase + i * 0.12;
        waveL[i] = Math.sin(t) * 0.6 + Math.sin(t * 2.8) * 0.25 + (Math.random() - 0.5) * 0.08;
        waveR[i] = Math.sin(t + 0.8) * 0.6 + Math.cos(t * 2.8) * 0.25 + (Math.random() - 0.5) * 0.08;
      }

      // Modal energies
      const modalEnergies = new Float32Array(16);
      const triadFlow = new Float32Array(16);
      for (let i = 0; i < 16; i++) {
        modalEnergies[i] = Math.max(0.05, Math.sin(phase * 0.4 + i * 0.5) * 0.45 + 0.5);
        triadFlow[i] = Math.sin(phase * 0.2 + i * 0.7) * 0.5;
      }

      const zenoFlux = Math.abs(Math.sin(phase * 0.15));
      let activeK = 1;
      if (zenoFlux > 0.75) activeK = 16;
      else if (zenoFlux > 0.50) activeK = 8;
      else if (zenoFlux > 0.25) activeK = 4;

      if (this.visualizer) {
        this.visualizer.setTelemetry({
          waveformL: waveL,
          waveformR: waveR,
          aperture: Math.max(0.1, Math.sin(phase * 0.3) * 1.5 + 1.2),
          colonicPressure: this.parameters.param_colonic_pressure,
          airVelocity: 1.2 + Math.abs(Math.sin(phase * 0.5)) * 2.5,
          bubbleIntensity: this.parameters.param_minnaert_bubble_density * 0.01,
          dropletTriggered: Math.random() < 0.05,
          zenoFlux: zenoFlux,
          activeUnrollK: activeK,
          lorenzX: lorenzX,
          lorenzY: lorenzY,
          lorenzZ: lorenzZ,
          modalEnergies: modalEnergies,
          triadEnergyFlow: triadFlow,
          sphericalNormDrift: 0.0,
          peakLevelDb: -3.5,
          rmsLevelDb: -12.0
        });
      }
    }, 1000 / 60); // 60 FPS
  }
}

// Bootstrap application on DOM ready
document.addEventListener('DOMContentLoaded', () => {
  window.pf02 = new PfApp();
  window.pf02.init();
});
