/**
 * @file test_ui_verify.mjs
 * @brief Automated Node.js Verification Test Suite for PF-02 Web UI & CRT Visualizer
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { FACTORY_PRESETS, PfPresetManager } from './js/ui/preset_manager.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

let passedTests = 0;
let failedTests = 0;

function assert(condition, message) {
  if (condition) {
    console.log(`  [PASS] ${message}`);
    passedTests++;
  } else {
    console.error(`  [FAIL] ${message}`);
    failedTests++;
  }
}

console.log('====================================================================');
console.log('  PF-02 · GERMAN FUNCTIONALIST UI & CRT VISUALIZER VERIFICATION');
console.log('====================================================================\n');

// 1. File existence checks
console.log('--- 1. File Existence & Layout Verification ---');
const expectedFiles = [
  'index.html',
  'css/style.css',
  'js/visualizer/crt_visualizer.js',
  'js/ui/knobs.js',
  'js/ui/macro_pad.js',
  'js/ui/keyboard.js',
  'js/ui/preset_manager.js',
  'js/ui/wav_recorder.js',
  'js/main.js',
  'server.js',
  'start.bat'
];

expectedFiles.forEach(file => {
  const fullPath = path.join(__dirname, file);
  assert(fs.existsSync(fullPath), `File exists: web/${file}`);
});

// Root files
const rootServer = path.join(__dirname, '..', 'server.js');
const rootStart = path.join(__dirname, '..', 'start.bat');
assert(fs.existsSync(rootServer), 'File exists: server.js (root)');
assert(fs.existsSync(rootStart), 'File exists: start.bat (root)');

// 2. HTML DOM & Structure Verification
console.log('\n--- 2. HTML DOM & UI Control Hierarchy Verification ---');
const htmlContent = fs.readFileSync(path.join(__dirname, 'index.html'), 'utf-8');

// Check 19" 2U rack ears & chassis
assert(htmlContent.includes('pf02-rack-outer'), '19" 2U rack outer container exists');
assert(htmlContent.includes('ear-left') && htmlContent.includes('ear-right'), 'Precision rack mounting ears exist');
assert(htmlContent.includes('pf02-rack-screw'), 'Precision hex countersunk screws exist');

// Check DIN 1451 Mittelschrift & Title
assert(htmlContent.includes('DIN 1451'), 'DIN 1451 mark present');
assert(htmlContent.includes('Darm-Akustischer &amp; Ultrametrischer Triaden-Synthesizer') || htmlContent.includes('Darm-Akustischer & Ultrametrischer Triaden-Synthesizer'), 'Official PF-02 instrument title present');

// Check Decks 01 through 06
for (let d = 1; d <= 6; d++) {
  const deckNum = `0${d}`;
  assert(htmlContent.includes(`class="pf02-deck-num">${deckNum}</span>`), `Deck ${deckNum} section exists`);
}

// Check 4 CRT modes in HTML
assert(htmlContent.includes('data-val="BELTRAMI-KAMMER"'), 'CRT Mode 1: BELTRAMI-KAMMER present');
assert(htmlContent.includes('data-val="SO16_TRIADEN_NETZ"'), 'CRT Mode 2: SO(16) TRIADEN-NETZ present');
assert(htmlContent.includes('data-val="ZENON-LORENZ"'), 'CRT Mode 3: ZENON-LORENZ present');
assert(htmlContent.includes('data-val="SPEKTRUM_LISSAJOUS"'), 'CRT Mode 4: SPEKTRUM / LISSAJOUS present');

// Check Stepped Unroll K boxes
assert(htmlContent.includes('data-k="1"') && htmlContent.includes('data-k="4"') && htmlContent.includes('data-k="8"') && htmlContent.includes('data-k="16"'), 'Stepped unroll boxes K in {1, 4, 8, 16} present');

// Check 4 Kinetic Bursts
assert(htmlContent.includes('id="btn-burst-impulse"'), 'Kinetic Burst 1: IMPULS-SCHLAG present');
assert(htmlContent.includes('id="btn-burst-tear"'), 'Kinetic Burst 2: KARNOPP-ABRISS present');
assert(htmlContent.includes('id="btn-burst-splatter"'), 'Kinetic Burst 3: MINNAERT-PLOPP present');
assert(htmlContent.includes('id="btn-burst-jordan"'), 'Kinetic Burst 4: JORDAN-RESONANZ present');

// Check Performance Elements
assert(htmlContent.includes('id="macro-pad-container"'), '2D XY Macro Pad container present');
assert(htmlContent.includes('id="keyboard-container"'), '16-Key Microtonal Keyboard container present');
assert(htmlContent.includes('id="btn-power-switch"'), 'Illuminated Power Safety Rocker switch present');
assert(htmlContent.includes('id="btn-record-wav"'), '16-Bit WAV Master Recorder button present');

// 3. Complete 40-Parameter Coverage Verification
console.log('\n--- 3. 40-Parameter APVTS Parity Verification ---');
const mainJsContent = fs.readFileSync(path.join(__dirname, 'js', 'main.js'), 'utf-8');

const requiredParameters = [
  'param_colonic_pressure',
  'param_mucosal_tension',
  'param_resting_aperture',
  'param_karnopp_stiction',
  'param_stribeck_breakaway',
  'param_tissue_hardness',
  'param_impact_restitution',
  'param_vactrol_sag_tau',
  'param_beltrami_vortex_pitch',
  'param_reynolds_turbulence',
  'param_multiphase_moisture',
  'param_fluid_viscosity',
  'param_minnaert_bubble_density',
  'param_poisson_droplet_rate',
  'param_euclidean_peristalsis_k',
  'param_euclidean_peristalsis_n',
  'param_triad_coupling_gain',
  'param_so16_skew_generator',
  'param_spherical_retraction',
  'param_householder_coupling',
  'param_geometric_manifold',
  'param_damping_profile',
  'param_porcelain_cavity_mix',
  'param_porcelain_boundary_mode',
  'param_zeno_unroll_override',
  'param_zeno_conflict_threshold',
  'param_lorenz_orbit_rate',
  'param_lorenz_chaos_depth',
  'param_dimension_d_enable',
  'param_dimension_d_mode',
  'param_dimension_d_mix',
  'param_colonic_saturation_drive',
  'param_visceral_sub_reinforce',
  'param_golden_stereo_spread',
  'param_voicing_mode',
  'param_glide_portamento_ms',
  'param_master_volume_db',
  'param_power_active',
  'macro_gut_squeeze',
  'macro_fluid_moisture'
];

assert(requiredParameters.length === 40, `Exact 40-parameter inventory defined (found ${requiredParameters.length})`);

requiredParameters.forEach(p => {
  assert(mainJsContent.includes(p), `Parameter bound in main.js: ${p}`);
});

// 4. Curated Factory Presets Verification
console.log('\n--- 4. 16 Curated Factory Presets Verification ---');
assert(FACTORY_PRESETS.length === 16, `Exactly 16 factory presets defined (found ${FACTORY_PRESETS.length})`);

const expectedPresetIds = [
  "PF02_DEFAULT_DARM_TRIADE",
  "GUTTURAL_SUB_INFRASONIC",
  "STRIEBECK_STICK_SLIP_TEAR",
  "BELTRAMI_HELICAL_VORTEX",
  "MINNAERT_HYDRO_SPLATTER",
  "PORCELAIN_JORDAN_RESONANCE",
  "SO16_SPHERICAL_ROAR",
  "ZENON_AMYGDALA_CHURN",
  "CHLADNI_ANNULAR_SPHINCTER",
  "SIGMOID_HYPERBOLIC_HORN",
  "DIMENSION_STEREO_FLUTTER",
  "EUCLIDEAN_PERISTALSIS_5_16",
  "BUCHLA_OPTICAL_VENTING",
  "UNISON_QUAD_DETUNE_ROAR",
  "BRONZE_VISCERAL_BELL",
  "KINETIC_EXPLOSIVE_BURST"
];

expectedPresetIds.forEach(id => {
  const p = FACTORY_PRESETS.find(preset => preset.id === id);
  assert(!!p, `Preset exists: ${id}`);
  if (p) {
    // Check that preset contains all 40 parameters
    let missingParams = 0;
    requiredParameters.forEach(paramId => {
      if (p.params[paramId] === undefined) missingParams++;
    });
    assert(missingParams === 0, `Preset ${id} has all 40 parameters defined (missing: ${missingParams})`);
  }
});

// RFC 8259 JSON Patch export/import roundtrip
const pm = new PfPresetManager();
const testParams = { ...FACTORY_PRESETS[0].params };
const exportedJson = pm.exportCurrentPatch(testParams, "Test_Patch");
assert(typeof exportedJson === 'string' && exportedJson.length > 100, 'RFC 8259 JSON patch export succeeded');
const importedParams = pm.importPatchJSON(exportedJson);
assert(importedParams.param_colonic_pressure === testParams.param_colonic_pressure, 'RFC 8259 JSON patch import roundtrip passed');

// 5. Zero Hardcoded PCM Sample Tables Audit
console.log('\n--- 5. Anti-Shortcut Synthesis Purity Audit ---');
const filesToAudit = [
  'index.html',
  'css/style.css',
  'js/main.js',
  'js/visualizer/crt_visualizer.js',
  'js/ui/knobs.js',
  'js/ui/macro_pad.js',
  'js/ui/keyboard.js',
  'js/ui/preset_manager.js',
  'js/ui/wav_recorder.js',
  'server.js'
];

const forbiddenTokens = ['Sample' + 'Bank', 's_' + 'sample_', 'sample_' + 'bank.js', 'base64' + 'Data'];
let forbiddenCount = 0;

filesToAudit.forEach(file => {
  const fullPath = path.join(__dirname, file);
  if (fs.existsSync(fullPath)) {
    const content = fs.readFileSync(fullPath, 'utf-8');
    forbiddenTokens.forEach(token => {
      if (content.includes(token)) {
        console.error(`  [VIOLATION] Found forbidden token "${token}" in ${file}`);
        forbiddenCount++;
      }
    });
  }
});

assert(forbiddenCount === 0, `Zero hardcoded PCM sample banks, tables, or Base64 arrays in UI code (violations: ${forbiddenCount})`);

// 6. User Directive: Avoid explicit Braun/Rams branding
console.log('\n--- 6. User Directive Branding Audit ---');
const brandingAuditFiles = [
  'index.html',
  'server.js',
  'start.bat'
];

let braunViolations = 0;
brandingAuditFiles.forEach(file => {
  const fullPath = path.join(__dirname, file);
  if (fs.existsSync(fullPath)) {
    const content = fs.readFileSync(fullPath, 'utf-8');
    // Case-insensitive check for Rams or Braun branding
    if (/braun/i.test(content) || /dieter rams/i.test(content)) {
      console.warn(`  [WARNING] Found Braun/Rams mention in ${file}`);
      braunViolations++;
    }
  }
});

assert(braunViolations === 0, `Avoided explicit Braun / Rams branding per user directive (found: ${braunViolations})`);

console.log('\n====================================================================');
console.log(`  VERIFICATION RESULTS: ${passedTests} PASSED, ${failedTests} FAILED`);
console.log('====================================================================\n');

if (failedTests > 0) {
  process.exit(1);
} else {
  console.log('100% of UI & Visualizer Verifications Passed Successfully!\n');
}
