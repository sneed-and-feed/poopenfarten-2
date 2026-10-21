#include "PresetManager.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstring>

namespace pf02::parameters {

Preset::Preset() {
    const auto& defs = getAllParameterDefs();
    for (size_t i = 0; i < kNumParameters; ++i) {
        params[i] = defs[i].defaultValue;
    }
}

Preset::Preset(std::string idIn, std::string nameIn, std::string catIn, std::string descIn)
    : id(std::move(idIn))
    , name(std::move(nameIn))
    , category(std::move(catIn))
    , description(std::move(descIn)) {
    const auto& defs = getAllParameterDefs();
    for (size_t i = 0; i < kNumParameters; ++i) {
        params[i] = defs[i].defaultValue;
    }
}

float Preset::getParam(std::string_view idStr) const noexcept {
    const auto pId = getParameterIdByName(idStr);
    return getParam(pId);
}

void Preset::setParam(std::string_view idStr, float value) noexcept {
    const auto pId = getParameterIdByName(idStr);
    setParam(pId, value);
}

// ============================================================================
// Factory Preset Definitions (16 Curated Presets)
// ============================================================================
std::array<Preset, PresetManager::kNumFactoryPresets> PresetManager::initFactoryPresets() {
    std::array<Preset, kNumFactoryPresets> presets;

    // Helper lambda to assign parameters by ID
    auto setP = [](Preset& p, std::string_view key, float val) {
        p.setParam(key, val);
    };

    // 1. PF02_DEFAULT_DARM_TRIADE
    {
        Preset& p = presets[0];
        p.id = "PF02_DEFAULT_DARM_TRIADE";
        p.name = "CALIBRATED DEFAULT · Triaden-Grundzustand";
        p.category = "Standard Benchmark";
        p.description = "Balanced mucosal relaxation valve, standard colonic pressure, moderate so(16) triad coupling, rich authentic guttural resonance.";
        setP(p, ParamIDs::colonic_pressure, 0.70f);
        setP(p, ParamIDs::mucosal_tension, 0.50f);
        setP(p, ParamIDs::resting_aperture, 1.0f);
        setP(p, ParamIDs::karnopp_stiction, 0.65f);
        setP(p, ParamIDs::stribeck_breakaway, 2.2f);
        setP(p, ParamIDs::tissue_hardness, 2.1f);
        setP(p, ParamIDs::impact_restitution, 0.45f);
        setP(p, ParamIDs::vactrol_sag_tau, 85.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 2.8f);
        setP(p, ParamIDs::reynolds_turbulence, 0.40f);
        setP(p, ParamIDs::multiphase_moisture, 0.35f);
        setP(p, ParamIDs::fluid_viscosity, 0.30f);
        setP(p, ParamIDs::minnaert_bubble_density, 40.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 8.5f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 5.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 0.85f);
        setP(p, ParamIDs::so16_skew_generator, 0.50f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.45f);
        setP(p, ParamIDs::geometric_manifold, 2.0f);
        setP(p, ParamIDs::damping_profile, 0.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 35.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.55f);
        setP(p, ParamIDs::lorenz_orbit_rate, 0.65f);
        setP(p, ParamIDs::lorenz_chaos_depth, 25.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 1.0f);
        setP(p, ParamIDs::dimension_d_mix, 40.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 30.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, 0.0f);
        setP(p, ParamIDs::golden_stereo_spread, 75.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 45.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.50f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.50f);
    }

    // 2. GUTTURAL_SUB_INFRASONIC
    {
        Preset& p = presets[1];
        p.id = "GUTTURAL_SUB_INFRASONIC";
        p.name = "INFRASONISCHER BRUMMER · Infrasonic Bowel";
        p.category = "Sub Bass";
        p.description = "Subharmonic period-doubling down to 7 Hz, high colonic pressure head, maximum visceral sub reinforcement, loose mucosal tension.";
        setP(p, ParamIDs::colonic_pressure, 0.95f);
        setP(p, ParamIDs::mucosal_tension, 0.22f);
        setP(p, ParamIDs::resting_aperture, 2.2f);
        setP(p, ParamIDs::karnopp_stiction, 0.80f);
        setP(p, ParamIDs::stribeck_breakaway, 1.4f);
        setP(p, ParamIDs::tissue_hardness, 1.6f);
        setP(p, ParamIDs::impact_restitution, 0.70f);
        setP(p, ParamIDs::vactrol_sag_tau, 160.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 1.2f);
        setP(p, ParamIDs::reynolds_turbulence, 0.25f);
        setP(p, ParamIDs::multiphase_moisture, 0.20f);
        setP(p, ParamIDs::fluid_viscosity, 0.60f);
        setP(p, ParamIDs::minnaert_bubble_density, 15.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 3.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 3.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 0.40f);
        setP(p, ParamIDs::so16_skew_generator, 0.20f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.60f);
        setP(p, ParamIDs::geometric_manifold, 2.0f);
        setP(p, ParamIDs::damping_profile, 0.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 15.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 0.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.40f);
        setP(p, ParamIDs::lorenz_orbit_rate, 0.20f);
        setP(p, ParamIDs::lorenz_chaos_depth, 10.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 0.0f);
        setP(p, ParamIDs::dimension_d_mix, 25.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 55.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, 5.0f);
        setP(p, ParamIDs::golden_stereo_spread, 50.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 60.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.85f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.25f);
    }

    // 3. STRIEBECK_STICK_SLIP_TEAR
    {
        Preset& p = presets[2];
        p.id = "STRIEBECK_STICK_SLIP_TEAR";
        p.name = "KARNOPP-ABREISS-FLUTTER · Bowed Mucosa";
        p.category = "Friction & Flutter";
        p.description = "Karnopp velocity deadband stiction, violent Stribeck breakaway transients, dry crackling breaks and mucosal squeaks.";
        setP(p, ParamIDs::colonic_pressure, 0.60f);
        setP(p, ParamIDs::mucosal_tension, 0.85f);
        setP(p, ParamIDs::resting_aperture, 0.4f);
        setP(p, ParamIDs::karnopp_stiction, 0.98f);
        setP(p, ParamIDs::stribeck_breakaway, 6.5f);
        setP(p, ParamIDs::tissue_hardness, 2.8f);
        setP(p, ParamIDs::impact_restitution, 0.20f);
        setP(p, ParamIDs::vactrol_sag_tau, 45.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 5.5f);
        setP(p, ParamIDs::reynolds_turbulence, 0.65f);
        setP(p, ParamIDs::multiphase_moisture, 0.05f);
        setP(p, ParamIDs::fluid_viscosity, 0.10f);
        setP(p, ParamIDs::minnaert_bubble_density, 5.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 0.5f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 7.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 1.10f);
        setP(p, ParamIDs::so16_skew_generator, 0.70f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.50f);
        setP(p, ParamIDs::geometric_manifold, 1.0f);
        setP(p, ParamIDs::damping_profile, 2.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 20.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 0.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.75f);
        setP(p, ParamIDs::lorenz_orbit_rate, 1.20f);
        setP(p, ParamIDs::lorenz_chaos_depth, 45.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 2.0f);
        setP(p, ParamIDs::dimension_d_mix, 30.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 40.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, -6.0f);
        setP(p, ParamIDs::golden_stereo_spread, 80.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 15.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.70f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.05f);
    }

    // 4. BELTRAMI_HELICAL_VORTEX
    {
        Preset& p = presets[3];
        p.id = "BELTRAMI_HELICAL_VORTEX";
        p.name = "BELTRAMI-WIRBELSTRAHL · Aeroacoustic Jet";
        p.category = "Aeroacoustic Jet";
        p.description = "High Reynolds number turbulent jet, strong Beltrami vortex wavenumber lambda, whistle and flutter hiss.";
        setP(p, ParamIDs::colonic_pressure, 0.85f);
        setP(p, ParamIDs::mucosal_tension, 0.40f);
        setP(p, ParamIDs::resting_aperture, 1.5f);
        setP(p, ParamIDs::karnopp_stiction, 0.35f);
        setP(p, ParamIDs::stribeck_breakaway, 1.8f);
        setP(p, ParamIDs::tissue_hardness, 1.8f);
        setP(p, ParamIDs::impact_restitution, 0.40f);
        setP(p, ParamIDs::vactrol_sag_tau, 60.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 8.5f);
        setP(p, ParamIDs::reynolds_turbulence, 0.90f);
        setP(p, ParamIDs::multiphase_moisture, 0.25f);
        setP(p, ParamIDs::fluid_viscosity, 0.15f);
        setP(p, ParamIDs::minnaert_bubble_density, 20.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 12.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 4.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 0.90f);
        setP(p, ParamIDs::so16_skew_generator, 0.85f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.40f);
        setP(p, ParamIDs::geometric_manifold, 0.0f);
        setP(p, ParamIDs::damping_profile, 0.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 30.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.60f);
        setP(p, ParamIDs::lorenz_orbit_rate, 2.50f);
        setP(p, ParamIDs::lorenz_chaos_depth, 35.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 1.0f);
        setP(p, ParamIDs::dimension_d_mix, 45.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 35.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, -3.0f);
        setP(p, ParamIDs::golden_stereo_spread, 85.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 30.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.65f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.30f);
    }

    // 5. MINNAERT_HYDRO_SPLATTER
    {
        Preset& p = presets[4];
        p.id = "MINNAERT_HYDRO_SPLATTER";
        p.name = "MINNAERT-HYDRO-PLOPP · Multiphase Splatter";
        p.category = "Multiphase Fluid";
        p.description = "Gas-liquid bubble bank, Poisson droplet splatter rain, high moisture, wet squelch formant filtering.";
        setP(p, ParamIDs::colonic_pressure, 0.75f);
        setP(p, ParamIDs::mucosal_tension, 0.45f);
        setP(p, ParamIDs::resting_aperture, 1.8f);
        setP(p, ParamIDs::karnopp_stiction, 0.50f);
        setP(p, ParamIDs::stribeck_breakaway, 2.0f);
        setP(p, ParamIDs::tissue_hardness, 1.9f);
        setP(p, ParamIDs::impact_restitution, 0.60f);
        setP(p, ParamIDs::vactrol_sag_tau, 110.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 3.2f);
        setP(p, ParamIDs::reynolds_turbulence, 0.45f);
        setP(p, ParamIDs::multiphase_moisture, 0.95f);
        setP(p, ParamIDs::fluid_viscosity, 0.75f);
        setP(p, ParamIDs::minnaert_bubble_density, 95.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 38.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 9.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 0.75f);
        setP(p, ParamIDs::so16_skew_generator, 0.40f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.35f);
        setP(p, ParamIDs::geometric_manifold, 2.0f);
        setP(p, ParamIDs::damping_profile, 2.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 50.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 2.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.50f);
        setP(p, ParamIDs::lorenz_orbit_rate, 0.45f);
        setP(p, ParamIDs::lorenz_chaos_depth, 30.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 2.0f);
        setP(p, ParamIDs::dimension_d_mix, 55.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 25.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, 2.0f);
        setP(p, ParamIDs::golden_stereo_spread, 80.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 50.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.60f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.95f);
    }

    // 6. PORCELAIN_JORDAN_RESONANCE
    {
        Preset& p = presets[5];
        p.id = "PORCELAIN_JORDAN_RESONANCE";
        p.name = "PORZELLAN-JORDAN-BECKEN · Ceramic Cavity";
        p.category = "Ceramic Cavity";
        p.description = "2D Jordan-enclosure cavity standing waves, porcelain bowl reflection profile, ceramic viscoelastic damping.";
        setP(p, ParamIDs::colonic_pressure, 0.65f);
        setP(p, ParamIDs::mucosal_tension, 0.55f);
        setP(p, ParamIDs::resting_aperture, 1.2f);
        setP(p, ParamIDs::karnopp_stiction, 0.60f);
        setP(p, ParamIDs::stribeck_breakaway, 2.5f);
        setP(p, ParamIDs::tissue_hardness, 2.6f);
        setP(p, ParamIDs::impact_restitution, 0.30f);
        setP(p, ParamIDs::vactrol_sag_tau, 90.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 2.4f);
        setP(p, ParamIDs::reynolds_turbulence, 0.30f);
        setP(p, ParamIDs::multiphase_moisture, 0.30f);
        setP(p, ParamIDs::fluid_viscosity, 0.25f);
        setP(p, ParamIDs::minnaert_bubble_density, 30.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 6.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 5.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 0.95f);
        setP(p, ParamIDs::so16_skew_generator, 0.55f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.70f);
        setP(p, ParamIDs::geometric_manifold, 3.0f);
        setP(p, ParamIDs::damping_profile, 1.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 90.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 3.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.55f);
        setP(p, ParamIDs::lorenz_orbit_rate, 0.50f);
        setP(p, ParamIDs::lorenz_chaos_depth, 20.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 3.0f);
        setP(p, ParamIDs::dimension_d_mix, 45.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 20.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, -2.0f);
        setP(p, ParamIDs::golden_stereo_spread, 90.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 40.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.45f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.35f);
    }

    // 7. SO16_SPHERICAL_ROAR
    {
        Preset& p = presets[6];
        p.id = "SO16_SPHERICAL_ROAR";
        p.name = "SO(16)-TRIADEN-SINGULARITÄT · Triad Roar";
        p.category = "Extreme Resonator";
        p.description = "Maximum so(16) Lie-triad coupling, SlowFastTriadCascade K=16, exact spherical norm retraction with zero radial drift, aggressive formant roar.";
        setP(p, ParamIDs::colonic_pressure, 0.90f);
        setP(p, ParamIDs::mucosal_tension, 0.65f);
        setP(p, ParamIDs::resting_aperture, 0.8f);
        setP(p, ParamIDs::karnopp_stiction, 0.75f);
        setP(p, ParamIDs::stribeck_breakaway, 3.5f);
        setP(p, ParamIDs::tissue_hardness, 2.4f);
        setP(p, ParamIDs::impact_restitution, 0.50f);
        setP(p, ParamIDs::vactrol_sag_tau, 75.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 4.2f);
        setP(p, ParamIDs::reynolds_turbulence, 0.60f);
        setP(p, ParamIDs::multiphase_moisture, 0.40f);
        setP(p, ParamIDs::fluid_viscosity, 0.35f);
        setP(p, ParamIDs::minnaert_bubble_density, 50.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 15.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 7.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 1.90f);
        setP(p, ParamIDs::so16_skew_generator, 0.95f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.80f);
        setP(p, ParamIDs::geometric_manifold, 1.0f);
        setP(p, ParamIDs::damping_profile, 4.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 40.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 4.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.30f);
        setP(p, ParamIDs::lorenz_orbit_rate, 1.80f);
        setP(p, ParamIDs::lorenz_chaos_depth, 60.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 2.0f);
        setP(p, ParamIDs::dimension_d_mix, 50.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 65.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, 3.0f);
        setP(p, ParamIDs::golden_stereo_spread, 95.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 35.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.90f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.50f);
    }

    // 8. ZENON_AMYGDALA_CHURN
    {
        Preset& p = presets[7];
        p.id = "ZENON_AMYGDALA_CHURN";
        p.name = "ZENON-AMYGDALA-TURBULENZ · Adaptive Churn";
        p.category = "Chaotic Morph";
        p.description = "Zeno-Amygdala controller dynamically hopping between K=1, 4, 8, 16 unroll depths as pressure fluctuates, chaotic RK4 Lorenz detuning.";
        setP(p, ParamIDs::colonic_pressure, 0.78f);
        setP(p, ParamIDs::mucosal_tension, 0.48f);
        setP(p, ParamIDs::resting_aperture, 1.1f);
        setP(p, ParamIDs::karnopp_stiction, 0.70f);
        setP(p, ParamIDs::stribeck_breakaway, 2.8f);
        setP(p, ParamIDs::tissue_hardness, 2.2f);
        setP(p, ParamIDs::impact_restitution, 0.48f);
        setP(p, ParamIDs::vactrol_sag_tau, 95.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 3.8f);
        setP(p, ParamIDs::reynolds_turbulence, 0.55f);
        setP(p, ParamIDs::multiphase_moisture, 0.45f);
        setP(p, ParamIDs::fluid_viscosity, 0.30f);
        setP(p, ParamIDs::minnaert_bubble_density, 45.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 10.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 6.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 1.25f);
        setP(p, ParamIDs::so16_skew_generator, 0.65f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.55f);
        setP(p, ParamIDs::geometric_manifold, 2.0f);
        setP(p, ParamIDs::damping_profile, 0.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 45.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.35f);
        setP(p, ParamIDs::lorenz_orbit_rate, 3.50f);
        setP(p, ParamIDs::lorenz_chaos_depth, 75.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 1.0f);
        setP(p, ParamIDs::dimension_d_mix, 40.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 45.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, 0.0f);
        setP(p, ParamIDs::golden_stereo_spread, 75.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 40.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.70f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.55f);
    }

    // 9. CHLADNI_ANNULAR_SPHINCTER
    {
        Preset& p = presets[8];
        p.id = "CHLADNI_ANNULAR_SPHINCTER";
        p.name = "CHLADNI-RING-PERKUSSION · Elastic Plate";
        p.category = "Kinetic Percussion";
        p.description = "2D Chladni annular sphincter ring plate modes, crisp percussive tissue slap, high contact hardness alpha.";
        setP(p, ParamIDs::colonic_pressure, 0.80f);
        setP(p, ParamIDs::mucosal_tension, 0.75f);
        setP(p, ParamIDs::resting_aperture, 0.5f);
        setP(p, ParamIDs::karnopp_stiction, 0.85f);
        setP(p, ParamIDs::stribeck_breakaway, 4.5f);
        setP(p, ParamIDs::tissue_hardness, 2.9f);
        setP(p, ParamIDs::impact_restitution, 0.15f);
        setP(p, ParamIDs::vactrol_sag_tau, 35.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 2.0f);
        setP(p, ParamIDs::reynolds_turbulence, 0.35f);
        setP(p, ParamIDs::multiphase_moisture, 0.15f);
        setP(p, ParamIDs::fluid_viscosity, 0.15f);
        setP(p, ParamIDs::minnaert_bubble_density, 10.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 2.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 5.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 0.90f);
        setP(p, ParamIDs::so16_skew_generator, 0.45f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.65f);
        setP(p, ParamIDs::geometric_manifold, 1.0f);
        setP(p, ParamIDs::damping_profile, 4.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 25.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 0.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.60f);
        setP(p, ParamIDs::lorenz_orbit_rate, 0.80f);
        setP(p, ParamIDs::lorenz_chaos_depth, 15.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 0.0f);
        setP(p, ParamIDs::dimension_d_mix, 20.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 50.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, -4.0f);
        setP(p, ParamIDs::golden_stereo_spread, 70.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 10.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.75f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.15f);
    }

    // 10. SIGMOID_HYPERBOLIC_HORN
    {
        Preset& p = presets[9];
        p.id = "SIGMOID_HYPERBOLIC_HORN";
        p.name = "SIGMOIDALES HYPERBELHORN · Visceral Horn";
        p.category = "Visceral Horn";
        p.description = "Poincaré negative curvature horn acoustic dispersion, hollow sub-frequency standing waves.";
        setP(p, ParamIDs::colonic_pressure, 0.72f);
        setP(p, ParamIDs::mucosal_tension, 0.42f);
        setP(p, ParamIDs::resting_aperture, 1.6f);
        setP(p, ParamIDs::karnopp_stiction, 0.55f);
        setP(p, ParamIDs::stribeck_breakaway, 2.1f);
        setP(p, ParamIDs::tissue_hardness, 2.0f);
        setP(p, ParamIDs::impact_restitution, 0.45f);
        setP(p, ParamIDs::vactrol_sag_tau, 90.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 3.5f);
        setP(p, ParamIDs::reynolds_turbulence, 0.50f);
        setP(p, ParamIDs::multiphase_moisture, 0.30f);
        setP(p, ParamIDs::fluid_viscosity, 0.25f);
        setP(p, ParamIDs::minnaert_bubble_density, 35.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 7.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 4.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 0.80f);
        setP(p, ParamIDs::so16_skew_generator, 0.60f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.40f);
        setP(p, ParamIDs::geometric_manifold, 0.0f);
        setP(p, ParamIDs::damping_profile, 0.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 30.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.55f);
        setP(p, ParamIDs::lorenz_orbit_rate, 0.60f);
        setP(p, ParamIDs::lorenz_chaos_depth, 25.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 1.0f);
        setP(p, ParamIDs::dimension_d_mix, 35.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 30.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, 1.0f);
        setP(p, ParamIDs::golden_stereo_spread, 80.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 45.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.50f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.40f);
    }

    // 11. DIMENSION_STEREO_FLUTTER
    {
        Preset& p = presets[10];
        p.id = "DIMENSION_STEREO_FLUTTER";
        p.name = "DIMENSION-D-RAUMFLUTTER · Spatial Chorus";
        p.category = "Spatial Chorus";
        p.description = "Roland Dimension D tri-phase BBD chorus Mode IV, wide Golden-Ratio stereo dispersion, immune to mono cancellation.";
        setP(p, ParamIDs::colonic_pressure, 0.68f);
        setP(p, ParamIDs::mucosal_tension, 0.52f);
        setP(p, ParamIDs::resting_aperture, 1.2f);
        setP(p, ParamIDs::karnopp_stiction, 0.60f);
        setP(p, ParamIDs::stribeck_breakaway, 2.3f);
        setP(p, ParamIDs::tissue_hardness, 2.1f);
        setP(p, ParamIDs::impact_restitution, 0.50f);
        setP(p, ParamIDs::vactrol_sag_tau, 80.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 2.9f);
        setP(p, ParamIDs::reynolds_turbulence, 0.42f);
        setP(p, ParamIDs::multiphase_moisture, 0.40f);
        setP(p, ParamIDs::fluid_viscosity, 0.30f);
        setP(p, ParamIDs::minnaert_bubble_density, 45.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 9.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 5.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 0.88f);
        setP(p, ParamIDs::so16_skew_generator, 0.52f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.45f);
        setP(p, ParamIDs::geometric_manifold, 2.0f);
        setP(p, ParamIDs::damping_profile, 2.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 40.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.55f);
        setP(p, ParamIDs::lorenz_orbit_rate, 0.70f);
        setP(p, ParamIDs::lorenz_chaos_depth, 20.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 3.0f);
        setP(p, ParamIDs::dimension_d_mix, 80.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 25.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, 0.0f);
        setP(p, ParamIDs::golden_stereo_spread, 100.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 50.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.50f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.50f);
    }

    // 12. EUCLIDEAN_PERISTALSIS_5_16
    {
        Preset& p = presets[11];
        p.id = "EUCLIDEAN_PERISTALSIS_5_16";
        p.name = "EUKLIDISCHE PERISTALTIK · Rhythmic Motor";
        p.category = "Polyrhythmic Motor";
        p.description = "5/16 Bjorklund Euclidean pulse sequencer driving rhythmic peristaltic bursts.";
        setP(p, ParamIDs::colonic_pressure, 0.74f);
        setP(p, ParamIDs::mucosal_tension, 0.58f);
        setP(p, ParamIDs::resting_aperture, 0.9f);
        setP(p, ParamIDs::karnopp_stiction, 0.70f);
        setP(p, ParamIDs::stribeck_breakaway, 2.6f);
        setP(p, ParamIDs::tissue_hardness, 2.2f);
        setP(p, ParamIDs::impact_restitution, 0.45f);
        setP(p, ParamIDs::vactrol_sag_tau, 70.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 3.0f);
        setP(p, ParamIDs::reynolds_turbulence, 0.40f);
        setP(p, ParamIDs::multiphase_moisture, 0.35f);
        setP(p, ParamIDs::fluid_viscosity, 0.30f);
        setP(p, ParamIDs::minnaert_bubble_density, 40.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 8.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 5.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 0.85f);
        setP(p, ParamIDs::so16_skew_generator, 0.50f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.45f);
        setP(p, ParamIDs::geometric_manifold, 2.0f);
        setP(p, ParamIDs::damping_profile, 0.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 35.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.55f);
        setP(p, ParamIDs::lorenz_orbit_rate, 0.65f);
        setP(p, ParamIDs::lorenz_chaos_depth, 25.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 1.0f);
        setP(p, ParamIDs::dimension_d_mix, 40.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 30.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, 0.0f);
        setP(p, ParamIDs::golden_stereo_spread, 75.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 45.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.55f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.45f);
    }

    // 13. BUCHLA_OPTICAL_VENTING
    {
        Preset& p = presets[12];
        p.id = "BUCHLA_OPTICAL_VENTING";
        p.name = "BUCHLA-OPTO-ABSACKUNG · Optical Vactrol Sag";
        p.category = "Dynamic Pluck";
        p.description = "High Buchla 292 optical vactrol sag, natural 40% emission deceleration droop over sustained notes.";
        setP(p, ParamIDs::colonic_pressure, 0.65f);
        setP(p, ParamIDs::mucosal_tension, 0.60f);
        setP(p, ParamIDs::resting_aperture, 0.7f);
        setP(p, ParamIDs::karnopp_stiction, 0.68f);
        setP(p, ParamIDs::stribeck_breakaway, 2.4f);
        setP(p, ParamIDs::tissue_hardness, 2.3f);
        setP(p, ParamIDs::impact_restitution, 0.35f);
        setP(p, ParamIDs::vactrol_sag_tau, 350.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 2.2f);
        setP(p, ParamIDs::reynolds_turbulence, 0.35f);
        setP(p, ParamIDs::multiphase_moisture, 0.25f);
        setP(p, ParamIDs::fluid_viscosity, 0.25f);
        setP(p, ParamIDs::minnaert_bubble_density, 25.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 5.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 3.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 0.75f);
        setP(p, ParamIDs::so16_skew_generator, 0.40f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.50f);
        setP(p, ParamIDs::geometric_manifold, 2.0f);
        setP(p, ParamIDs::damping_profile, 0.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 30.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.60f);
        setP(p, ParamIDs::lorenz_orbit_rate, 0.50f);
        setP(p, ParamIDs::lorenz_chaos_depth, 20.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 1.0f);
        setP(p, ParamIDs::dimension_d_mix, 30.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 25.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, -2.0f);
        setP(p, ParamIDs::golden_stereo_spread, 70.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 30.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.40f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.30f);
    }

    // 14. UNISON_QUAD_DETUNE_ROAR
    {
        Preset& p = presets[13];
        p.id = "UNISON_QUAD_DETUNE_ROAR";
        p.name = "STEREO-UNISONO-GEBRÜLL · Quad Unison Detune";
        p.category = "Stereo Unison";
        p.description = "4-voice stereo unison with +/-7 and +/-14 cent detune offsets and phase decorrelation, colossal wall of guttural sound.";
        setP(p, ParamIDs::colonic_pressure, 0.82f);
        setP(p, ParamIDs::mucosal_tension, 0.55f);
        setP(p, ParamIDs::resting_aperture, 1.3f);
        setP(p, ParamIDs::karnopp_stiction, 0.65f);
        setP(p, ParamIDs::stribeck_breakaway, 2.8f);
        setP(p, ParamIDs::tissue_hardness, 2.2f);
        setP(p, ParamIDs::impact_restitution, 0.50f);
        setP(p, ParamIDs::vactrol_sag_tau, 85.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 3.5f);
        setP(p, ParamIDs::reynolds_turbulence, 0.50f);
        setP(p, ParamIDs::multiphase_moisture, 0.45f);
        setP(p, ParamIDs::fluid_viscosity, 0.35f);
        setP(p, ParamIDs::minnaert_bubble_density, 50.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 14.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 7.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 1.15f);
        setP(p, ParamIDs::so16_skew_generator, 0.65f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.60f);
        setP(p, ParamIDs::geometric_manifold, 2.0f);
        setP(p, ParamIDs::damping_profile, 0.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 45.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.50f);
        setP(p, ParamIDs::lorenz_orbit_rate, 1.10f);
        setP(p, ParamIDs::lorenz_chaos_depth, 35.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 2.0f);
        setP(p, ParamIDs::dimension_d_mix, 60.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 40.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, 2.0f);
        setP(p, ParamIDs::golden_stereo_spread, 90.0f);
        setP(p, ParamIDs::voicing_mode, 2.0f); // Stereo Unison!
        setP(p, ParamIDs::glide_portamento_ms, 35.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.70f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.60f);
    }

    // 15. BRONZE_VISCERAL_BELL
    {
        Preset& p = presets[14];
        p.id = "BRONZE_VISCERAL_BELL";
        p.name = "METALLISCHE DARMGLOCKE · Bronze Visceral";
        p.category = "Metallic Hybrid";
        p.description = "Bell Bronze damping profile coupled with visceral mucosal exciter, ringing metallic bowel harmonics.";
        setP(p, ParamIDs::colonic_pressure, 0.68f);
        setP(p, ParamIDs::mucosal_tension, 0.65f);
        setP(p, ParamIDs::resting_aperture, 0.8f);
        setP(p, ParamIDs::karnopp_stiction, 0.72f);
        setP(p, ParamIDs::stribeck_breakaway, 3.2f);
        setP(p, ParamIDs::tissue_hardness, 2.7f);
        setP(p, ParamIDs::impact_restitution, 0.25f);
        setP(p, ParamIDs::vactrol_sag_tau, 55.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 3.0f);
        setP(p, ParamIDs::reynolds_turbulence, 0.30f);
        setP(p, ParamIDs::multiphase_moisture, 0.20f);
        setP(p, ParamIDs::fluid_viscosity, 0.20f);
        setP(p, ParamIDs::minnaert_bubble_density, 20.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 4.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 4.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 1.05f);
        setP(p, ParamIDs::so16_skew_generator, 0.55f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.75f);
        setP(p, ParamIDs::geometric_manifold, 0.0f);
        setP(p, ParamIDs::damping_profile, 3.0f); // Bell Bronze
        setP(p, ParamIDs::porcelain_cavity_mix, 30.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.55f);
        setP(p, ParamIDs::lorenz_orbit_rate, 0.75f);
        setP(p, ParamIDs::lorenz_chaos_depth, 30.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 1.0f);
        setP(p, ParamIDs::dimension_d_mix, 35.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 25.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, -2.0f);
        setP(p, ParamIDs::golden_stereo_spread, 85.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 25.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 0.55f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.20f);
    }

    // 16. KINETIC_EXPLOSIVE_BURST
    {
        Preset& p = presets[15];
        p.id = "KINETIC_EXPLOSIVE_BURST";
        p.name = "KINETISCHER SPHINKTERSCHLAG · Seal Burst";
        p.category = "Transient Artillery";
        p.description = "Ultra-tight resting aperture, maximum colonic drive, violent seal-burst transients on every key onset.";
        setP(p, ParamIDs::colonic_pressure, 1.00f);
        setP(p, ParamIDs::mucosal_tension, 0.90f);
        setP(p, ParamIDs::resting_aperture, 0.15f);
        setP(p, ParamIDs::karnopp_stiction, 0.99f);
        setP(p, ParamIDs::stribeck_breakaway, 8.5f);
        setP(p, ParamIDs::tissue_hardness, 2.9f);
        setP(p, ParamIDs::impact_restitution, 0.65f);
        setP(p, ParamIDs::vactrol_sag_tau, 40.0f);
        setP(p, ParamIDs::beltrami_vortex_pitch, 5.0f);
        setP(p, ParamIDs::reynolds_turbulence, 0.75f);
        setP(p, ParamIDs::multiphase_moisture, 0.20f);
        setP(p, ParamIDs::fluid_viscosity, 0.40f);
        setP(p, ParamIDs::minnaert_bubble_density, 35.0f);
        setP(p, ParamIDs::poisson_droplet_rate, 18.0f);
        setP(p, ParamIDs::euclidean_peristalsis_k, 8.0f);
        setP(p, ParamIDs::euclidean_peristalsis_n, 16.0f);
        setP(p, ParamIDs::triad_coupling_gain, 1.60f);
        setP(p, ParamIDs::so16_skew_generator, 0.80f);
        setP(p, ParamIDs::spherical_retraction, 1.0f);
        setP(p, ParamIDs::householder_coupling, 0.85f);
        setP(p, ParamIDs::geometric_manifold, 1.0f);
        setP(p, ParamIDs::damping_profile, 0.0f);
        setP(p, ParamIDs::porcelain_cavity_mix, 40.0f);
        setP(p, ParamIDs::porcelain_boundary_mode, 1.0f);
        setP(p, ParamIDs::zeno_unroll_override, 0.0f);
        setP(p, ParamIDs::zeno_conflict_threshold, 0.30f);
        setP(p, ParamIDs::lorenz_orbit_rate, 1.50f);
        setP(p, ParamIDs::lorenz_chaos_depth, 50.0f);
        setP(p, ParamIDs::dimension_d_enable, 1.0f);
        setP(p, ParamIDs::dimension_d_mode, 2.0f);
        setP(p, ParamIDs::dimension_d_mix, 45.0f);
        setP(p, ParamIDs::colonic_saturation_drive, 75.0f);
        setP(p, ParamIDs::visceral_sub_reinforce, 5.0f);
        setP(p, ParamIDs::golden_stereo_spread, 90.0f);
        setP(p, ParamIDs::voicing_mode, 0.0f);
        setP(p, ParamIDs::glide_portamento_ms, 10.0f);
        setP(p, ParamIDs::master_volume_db, 0.0f);
        setP(p, ParamIDs::power_active, 1.0f);
        setP(p, ParamIDs::macro_gut_squeeze, 1.00f);
        setP(p, ParamIDs::macro_fluid_moisture, 0.30f);
    }

    return presets;
}

const std::array<Preset, PresetManager::kNumFactoryPresets>& PresetManager::getFactoryPresets() noexcept {
    static const std::array<Preset, kNumFactoryPresets> kPresets = initFactoryPresets();
    return kPresets;
}

const Preset* PresetManager::getPresetById(std::string_view id) noexcept {
    const auto& presets = getFactoryPresets();
    for (const auto& p : presets) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

const Preset* PresetManager::getPresetByIndex(size_t index) noexcept {
    const auto& presets = getFactoryPresets();
    return (index < presets.size()) ? &presets[index] : nullptr;
}

// ============================================================================
// RFC 8259 JSON String Helpers & Serialization
// ============================================================================
static void escapeJsonString(std::ostream& os, std::string_view s) {
    os << '"';
    for (char c : s) {
        switch (c) {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\b': os << "\\b";  break;
            case '\f': os << "\\f";  break;
            case '\n': os << "\\n";  break;
            case '\r': os << "\\r";  break;
            case '\t': os << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    os << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
                } else {
                    os << c;
                }
                break;
        }
    }
    os << '"';
}

std::string PresetManager::exportPresetToJson(const Preset& preset, bool pretty) {
    std::ostringstream ss;
    const std::string nl = pretty ? "\n" : "";
    const std::string ind1 = pretty ? "  " : "";
    const std::string ind2 = pretty ? "    " : "";
    const std::string sp = pretty ? " " : "";

    ss << "{" << nl;
    ss << ind1 << "\"id\":" << sp; escapeJsonString(ss, preset.id); ss << "," << nl;
    ss << ind1 << "\"name\":" << sp; escapeJsonString(ss, preset.name); ss << "," << nl;
    ss << ind1 << "\"category\":" << sp; escapeJsonString(ss, preset.category); ss << "," << nl;
    ss << ind1 << "\"description\":" << sp; escapeJsonString(ss, preset.description); ss << "," << nl;
    ss << ind1 << "\"params\":" << sp << "{" << nl;

    const auto& defs = getAllParameterDefs();
    for (size_t i = 0; i < defs.size(); ++i) {
        ss << ind2;
        escapeJsonString(ss, defs[i].idString);
        ss << ":" << sp;

        float val = preset.params[i];
        if (defs[i].type == ParameterType::Int || defs[i].type == ParameterType::Bool || defs[i].type == ParameterType::Choice) {
            ss << static_cast<int>(std::round(val));
        } else {
            // Float output formatted cleanly
            ss << std::fixed << std::setprecision(4) << val;
        }

        if (i + 1 < defs.size()) {
            ss << ",";
        }
        ss << nl;
    }

    ss << ind1 << "}" << nl;
    ss << "}" << nl;
    return ss.str();
}

std::string PresetManager::exportBankToJson(const std::vector<Preset>& presets, bool pretty) {
    std::ostringstream ss;
    const std::string nl = pretty ? "\n" : "";
    const std::string ind1 = pretty ? "  " : "";
    const std::string sp = pretty ? " " : "";

    ss << "{" << nl;
    ss << ind1 << "\"instrument\":" << sp << "\"PF-02 · Darm-Akustischer & Ultrametrischer Triaden-Synthesizer\"," << nl;
    ss << ind1 << "\"version\":" << sp << "\"2.0.0\"," << nl;
    ss << ind1 << "\"format\":" << sp << "\"RFC 8259 JSON Patch Library\"," << nl;
    ss << ind1 << "\"numPresets\":" << sp << presets.size() << "," << nl;
    ss << ind1 << "\"presets\":" << sp << "[" << nl;

    for (size_t i = 0; i < presets.size(); ++i) {
        std::string pStr = exportPresetToJson(presets[i], pretty);
        if (pretty) {
            // Indent the preset by 4 spaces
            std::istringstream pis(pStr);
            std::string line;
            while (std::getline(pis, line)) {
                if (!line.empty()) {
                    ss << "    " << line << "\n";
                }
            }
            if (i + 1 < presets.size()) {
                ss.seekp(-1, std::ios_base::end);
                ss << ",\n";
            }
        } else {
            ss << pStr;
            if (i + 1 < presets.size()) ss << ",";
        }
    }

    ss << ind1 << "]" << nl;
    ss << "}" << nl;
    return ss.str();
}

// ============================================================================
// Lightweight RFC 8259 JSON Parser
// ============================================================================
namespace json {

enum class TokenType {
    EndOfFile,
    CurlyOpen,
    CurlyClose,
    SquareOpen,
    SquareClose,
    Colon,
    Comma,
    String,
    Number,
    Boolean,
    Null
};

struct Token {
    TokenType type { TokenType::EndOfFile };
    std::string stringVal;
    double numberVal { 0.0 };
    bool boolVal { false };
};

class Lexer {
public:
    explicit Lexer(std::string_view src) : mSrc(src), mPos(0) {}

    Token nextToken() {
        skipWhitespace();
        if (mPos >= mSrc.size()) {
            return { TokenType::EndOfFile, {}, 0.0, false };
        }

        char c = mSrc[mPos++];
        switch (c) {
            case '{': return { TokenType::CurlyOpen, {}, 0.0, false };
            case '}': return { TokenType::CurlyClose, {}, 0.0, false };
            case '[': return { TokenType::SquareOpen, {}, 0.0, false };
            case ']': return { TokenType::SquareClose, {}, 0.0, false };
            case ':': return { TokenType::Colon, {}, 0.0, false };
            case ',': return { TokenType::Comma, {}, 0.0, false };
            case '"': return parseString();
            default:
                if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
                    --mPos;
                    return parseNumber();
                }
                if (c == 't' || c == 'f') {
                    --mPos;
                    return parseBoolean();
                }
                if (c == 'n') {
                    --mPos;
                    return parseNull();
                }
                break;
        }
        return { TokenType::EndOfFile, {}, 0.0, false };
    }

private:
    void skipWhitespace() {
        while (mPos < mSrc.size()) {
            char c = mSrc[mPos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++mPos;
            } else if (c == '/' && mPos + 1 < mSrc.size() && mSrc[mPos + 1] == '/') {
                // Line comment
                mPos += 2;
                while (mPos < mSrc.size() && mSrc[mPos] != '\n') ++mPos;
            } else {
                break;
            }
        }
    }

    Token parseString() {
        std::string s;
        while (mPos < mSrc.size()) {
            char c = mSrc[mPos++];
            if (c == '"') {
                return { TokenType::String, s, 0.0, false };
            }
            if (c == '\\' && mPos < mSrc.size()) {
                char esc = mSrc[mPos++];
                switch (esc) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u':
                        if (mPos + 4 <= mSrc.size()) {
                            // Skip 4 hex digits
                            mPos += 4;
                            s += '?';
                        }
                        break;
                    default: s += esc; break;
                }
            } else {
                s += c;
            }
        }
        return { TokenType::String, s, 0.0, false };
    }

    Token parseNumber() {
        size_t start = mPos;
        if (mSrc[mPos] == '-') ++mPos;
        while (mPos < mSrc.size() && (std::isdigit(static_cast<unsigned char>(mSrc[mPos])) || mSrc[mPos] == '.' || mSrc[mPos] == 'e' || mSrc[mPos] == 'E' || mSrc[mPos] == '+' || mSrc[mPos] == '-')) {
            ++mPos;
        }
        std::string numStr(mSrc.substr(start, mPos - start));
        char* endPtr = nullptr;
        double val = std::strtod(numStr.c_str(), &endPtr);
        return { TokenType::Number, {}, val, false };
    }

    Token parseBoolean() {
        if (mSrc.substr(mPos, 4) == "true") {
            mPos += 4;
            return { TokenType::Boolean, {}, 1.0, true };
        }
        if (mSrc.substr(mPos, 5) == "false") {
            mPos += 5;
            return { TokenType::Boolean, {}, 0.0, false };
        }
        ++mPos;
        return { TokenType::EndOfFile, {}, 0.0, false };
    }

    Token parseNull() {
        if (mSrc.substr(mPos, 4) == "null") {
            mPos += 4;
            return { TokenType::Null, {}, 0.0, false };
        }
        ++mPos;
        return { TokenType::EndOfFile, {}, 0.0, false };
    }

    std::string_view mSrc;
    size_t mPos;
};

} // namespace json

bool PresetManager::importPresetFromJson(const std::string& jsonStr, Preset& outPreset) {
    json::Lexer lexer(jsonStr);
    auto tok = lexer.nextToken();
    if (tok.type != json::TokenType::CurlyOpen) {
        return false;
    }

    Preset parsed;
    while (true) {
        tok = lexer.nextToken();
        if (tok.type == json::TokenType::CurlyClose || tok.type == json::TokenType::EndOfFile) {
            break;
        }
        if (tok.type == json::TokenType::Comma) {
            continue;
        }
        if (tok.type != json::TokenType::String) {
            return false;
        }

        std::string key = tok.stringVal;
        tok = lexer.nextToken();
        if (tok.type != json::TokenType::Colon) {
            return false;
        }

        if (key == "id") {
            tok = lexer.nextToken();
            if (tok.type == json::TokenType::String) parsed.id = tok.stringVal;
        } else if (key == "name") {
            tok = lexer.nextToken();
            if (tok.type == json::TokenType::String) parsed.name = tok.stringVal;
        } else if (key == "category") {
            tok = lexer.nextToken();
            if (tok.type == json::TokenType::String) parsed.category = tok.stringVal;
        } else if (key == "description") {
            tok = lexer.nextToken();
            if (tok.type == json::TokenType::String) parsed.description = tok.stringVal;
        } else if (key == "params") {
            tok = lexer.nextToken();
            if (tok.type != json::TokenType::CurlyOpen) return false;
            while (true) {
                tok = lexer.nextToken();
                if (tok.type == json::TokenType::CurlyClose || tok.type == json::TokenType::EndOfFile) break;
                if (tok.type == json::TokenType::Comma) continue;
                if (tok.type != json::TokenType::String) return false;
                std::string paramName = tok.stringVal;
                tok = lexer.nextToken();
                if (tok.type != json::TokenType::Colon) return false;
                tok = lexer.nextToken();
                float val = 0.0f;
                if (tok.type == json::TokenType::Number) {
                    val = static_cast<float>(tok.numberVal);
                } else if (tok.type == json::TokenType::Boolean) {
                    val = tok.boolVal ? 1.0f : 0.0f;
                }
                parsed.setParam(paramName, val);
            }
        } else {
            // Check if top-level key is directly a param ID
            tok = lexer.nextToken();
            if (tok.type == json::TokenType::Number) {
                parsed.setParam(key, static_cast<float>(tok.numberVal));
            } else if (tok.type == json::TokenType::Boolean) {
                parsed.setParam(key, tok.boolVal ? 1.0f : 0.0f);
            } else if (tok.type == json::TokenType::CurlyOpen) {
                // Skip unknown sub-object
                int depth = 1;
                while (depth > 0 && tok.type != json::TokenType::EndOfFile) {
                    tok = lexer.nextToken();
                    if (tok.type == json::TokenType::CurlyOpen) ++depth;
                    else if (tok.type == json::TokenType::CurlyClose) --depth;
                }
            } else if (tok.type == json::TokenType::SquareOpen) {
                // Skip unknown sub-array
                int depth = 1;
                while (depth > 0 && tok.type != json::TokenType::EndOfFile) {
                    tok = lexer.nextToken();
                    if (tok.type == json::TokenType::SquareOpen) ++depth;
                    else if (tok.type == json::TokenType::SquareClose) --depth;
                }
            }
        }
    }

    outPreset = parsed;
    return true;
}

bool PresetManager::importBankFromJson(const std::string& jsonStr, std::vector<Preset>& outPresets) {
    json::Lexer lexer(jsonStr);
    auto tok = lexer.nextToken();
    if (tok.type != json::TokenType::CurlyOpen) return false;

    std::vector<Preset> bank;
    while (true) {
        tok = lexer.nextToken();
        if (tok.type == json::TokenType::CurlyClose || tok.type == json::TokenType::EndOfFile) break;
        if (tok.type == json::TokenType::Comma) continue;
        if (tok.type != json::TokenType::String) return false;

        std::string key = tok.stringVal;
        tok = lexer.nextToken();
        if (tok.type != json::TokenType::Colon) return false;

        if (key == "presets") {
            tok = lexer.nextToken();
            if (tok.type != json::TokenType::SquareOpen) return false;

            while (true) {
                tok = lexer.nextToken();
                if (tok.type == json::TokenType::SquareClose || tok.type == json::TokenType::EndOfFile) break;
                if (tok.type == json::TokenType::Comma) continue;
                if (tok.type != json::TokenType::CurlyOpen) return false;

                // Read individual preset object
                Preset p;
                while (true) {
                    tok = lexer.nextToken();
                    if (tok.type == json::TokenType::CurlyClose || tok.type == json::TokenType::EndOfFile) break;
                    if (tok.type == json::TokenType::Comma) continue;
                    if (tok.type != json::TokenType::String) return false;

                    std::string pKey = tok.stringVal;
                    tok = lexer.nextToken();
                    if (tok.type != json::TokenType::Colon) return false;

                    if (pKey == "id") {
                        tok = lexer.nextToken();
                        if (tok.type == json::TokenType::String) p.id = tok.stringVal;
                    } else if (pKey == "name") {
                        tok = lexer.nextToken();
                        if (tok.type == json::TokenType::String) p.name = tok.stringVal;
                    } else if (pKey == "category") {
                        tok = lexer.nextToken();
                        if (tok.type == json::TokenType::String) p.category = tok.stringVal;
                    } else if (pKey == "description") {
                        tok = lexer.nextToken();
                        if (tok.type == json::TokenType::String) p.description = tok.stringVal;
                    } else if (pKey == "params") {
                        tok = lexer.nextToken();
                        if (tok.type != json::TokenType::CurlyOpen) return false;
                        while (true) {
                            tok = lexer.nextToken();
                            if (tok.type == json::TokenType::CurlyClose || tok.type == json::TokenType::EndOfFile) break;
                            if (tok.type == json::TokenType::Comma) continue;
                            if (tok.type != json::TokenType::String) return false;
                            std::string paramName = tok.stringVal;
                            tok = lexer.nextToken();
                            if (tok.type != json::TokenType::Colon) return false;
                            tok = lexer.nextToken();
                            float val = 0.0f;
                            if (tok.type == json::TokenType::Number) val = static_cast<float>(tok.numberVal);
                            else if (tok.type == json::TokenType::Boolean) val = tok.boolVal ? 1.0f : 0.0f;
                            p.setParam(paramName, val);
                        }
                    } else {
                        // Skip
                        tok = lexer.nextToken();
                    }
                }
                bank.push_back(p);
            }
        } else {
            // Skip other top-level keys
            tok = lexer.nextToken();
        }
    }

    if (!bank.empty()) {
        outPresets = std::move(bank);
        return true;
    }
    return false;
}

} // namespace pf02::parameters
