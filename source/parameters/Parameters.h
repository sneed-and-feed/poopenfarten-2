#pragma once

#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

#if defined(PF02_HAS_JUCE_PROCESSORS) && PF02_HAS_JUCE_PROCESSORS
    #include <juce_audio_processors/juce_audio_processors.h>
    #define PF02_HAS_JUCE 1
    #define PF02_HAS_JUCE_LAYOUT 1
#elif __has_include(<juce_audio_basics/juce_audio_basics.h>)
    #include <juce_audio_basics/juce_audio_basics.h>
    #define PF02_HAS_JUCE 1
    #define PF02_HAS_JUCE_LAYOUT 0
#else
    #define PF02_HAS_JUCE 0
    #define PF02_HAS_JUCE_LAYOUT 0
#endif


namespace pf02::parameters {

// ============================================================================
// Strongly-Typed Enums
// ============================================================================
enum class ParameterType {
    Float,
    Int,
    Bool,
    Choice
};

enum class GeometricManifold : int {
    SigmoidHorn    = 0, // Poincaré negative-curvature visceral tract
    ChladniRing    = 1, // Biharmonic 2D annular sphincter plate
    GlottalColonic = 2, // Coupled vocal/visceral formant squelch cascade
    PorcelainBowl  = 3  // 2D Jordan-enclosure cavity standing waves
};

enum class DampingProfile : int {
    MucosalTissue  = 0, // Steep damping, heavy viscous loss
    Porcelain      = 1, // High-Q, sustained crystalline upper overtones
    WetElastomer   = 2, // Nonlinear viscoelastic squelch
    BellBronze     = 3, // Resonant metallic harmonic bloom
    StiffCartilage = 4  // Crisp percussive slapdown
};

enum class PorcelainBoundaryMode : int {
    Dry          = 0,
    Standard     = 1,
    WaterCoupled = 2,
    Tiled        = 3
};

enum class ZenoUnrollOverride : int {
    Auto = 0,
    K1   = 1,
    K4   = 2,
    K8   = 3,
    K16  = 4
};

enum class DimensionDMode : int {
    ModeI   = 0,
    ModeII  = 1,
    ModeIII = 2,
    ModeIV  = 3
};

enum class VoicingMode : int {
    MonoLegato   = 0,
    Poly8        = 1,
    StereoUnison = 2
};

// ============================================================================
// Parameter Indices & Count
// ============================================================================
enum class ParameterId : int {
    // Deck 01: Karnopp-Schleimhaut & Druckventil
    ColonicPressure = 0,
    MucosalTension,
    RestingAperture,
    KarnoppStiction,
    StribeckBreakaway,
    TissueHardness,
    ImpactRestitution,
    VactrolSagTau,

    // Deck 02: Beltrami-Strahl & Minnaert-Phasenfluid
    BeltramiVortexPitch,
    ReynoldsTurbulence,
    MultiphaseMoisture,
    FluidViscosity,
    MinnaertBubbleDensity,
    PoissonDropletRate,
    EuclideanPeristalsisK,
    EuclideanPeristalsisN,

    // Deck 03: SO(16) Triaden-Resonator & Porzellan-Feld
    TriadCouplingGain,
    So16SkewGenerator,
    SphericalRetraction,
    HouseholderCoupling,
    GeometricManifold,
    DampingProfile,
    PorcelainCavityMix,
    PorcelainBoundaryMode,

    // Deck 04: Zenon-Amygdala & Lorenz-Attraktor
    ZenoUnrollOverride,
    ZenoConflictThreshold,
    LorenzOrbitRate,
    LorenzChaosDepth,

    // Deck 05: Dimension-D BBD & Hermite-Begrenzer
    DimensionDEnable,
    DimensionDMode,
    DimensionDMix,
    ColonicSaturationDrive,
    VisceralSubReinforce,

    // Deck 06: Spatial Dispersion & Master Voicing
    GoldenStereoSpread,
    VoicingMode,
    GlidePortamentoMs,
    MasterVolumeDb,
    PowerActive,

    // Performance Macros
    MacroGutSqueeze,
    MacroFluidMoisture,

    Count = 40
};

inline constexpr size_t kNumParameters = static_cast<size_t>(ParameterId::Count);

// ============================================================================
// Parameter Constant String IDs
// ============================================================================
namespace ParamIDs {
    // Deck 01
    inline constexpr std::string_view colonic_pressure        = "param_colonic_pressure";
    inline constexpr std::string_view mucosal_tension         = "param_mucosal_tension";
    inline constexpr std::string_view resting_aperture        = "param_resting_aperture";
    inline constexpr std::string_view karnopp_stiction        = "param_karnopp_stiction";
    inline constexpr std::string_view stribeck_breakaway      = "param_stribeck_breakaway";
    inline constexpr std::string_view tissue_hardness         = "param_tissue_hardness";
    inline constexpr std::string_view impact_restitution      = "param_impact_restitution";
    inline constexpr std::string_view vactrol_sag_tau         = "param_vactrol_sag_tau";

    // Deck 02
    inline constexpr std::string_view beltrami_vortex_pitch   = "param_beltrami_vortex_pitch";
    inline constexpr std::string_view reynolds_turbulence     = "param_reynolds_turbulence";
    inline constexpr std::string_view multiphase_moisture     = "param_multiphase_moisture";
    inline constexpr std::string_view fluid_viscosity         = "param_fluid_viscosity";
    inline constexpr std::string_view minnaert_bubble_density = "param_minnaert_bubble_density";
    inline constexpr std::string_view poisson_droplet_rate    = "param_poisson_droplet_rate";
    inline constexpr std::string_view euclidean_peristalsis_k = "param_euclidean_peristalsis_k";
    inline constexpr std::string_view euclidean_peristalsis_n = "param_euclidean_peristalsis_n";

    // Deck 03
    inline constexpr std::string_view triad_coupling_gain     = "param_triad_coupling_gain";
    inline constexpr std::string_view so16_skew_generator     = "param_so16_skew_generator";
    inline constexpr std::string_view spherical_retraction    = "param_spherical_retraction";
    inline constexpr std::string_view householder_coupling    = "param_householder_coupling";
    inline constexpr std::string_view geometric_manifold      = "param_geometric_manifold";
    inline constexpr std::string_view damping_profile         = "param_damping_profile";
    inline constexpr std::string_view porcelain_cavity_mix    = "param_porcelain_cavity_mix";
    inline constexpr std::string_view porcelain_boundary_mode = "param_porcelain_boundary_mode";

    // Deck 04
    inline constexpr std::string_view zeno_unroll_override    = "param_zeno_unroll_override";
    inline constexpr std::string_view zeno_conflict_threshold = "param_zeno_conflict_threshold";
    inline constexpr std::string_view lorenz_orbit_rate       = "param_lorenz_orbit_rate";
    inline constexpr std::string_view lorenz_chaos_depth      = "param_lorenz_chaos_depth";

    // Deck 05
    inline constexpr std::string_view dimension_d_enable      = "param_dimension_d_enable";
    inline constexpr std::string_view dimension_d_mode        = "param_dimension_d_mode";
    inline constexpr std::string_view dimension_d_mix         = "param_dimension_d_mix";
    inline constexpr std::string_view colonic_saturation_drive= "param_colonic_saturation_drive";
    inline constexpr std::string_view visceral_sub_reinforce  = "param_visceral_sub_reinforce";

    // Deck 06
    inline constexpr std::string_view golden_stereo_spread    = "param_golden_stereo_spread";
    inline constexpr std::string_view voicing_mode            = "param_voicing_mode";
    inline constexpr std::string_view glide_portamento_ms     = "param_glide_portamento_ms";
    inline constexpr std::string_view master_volume_db        = "param_master_volume_db";
    inline constexpr std::string_view power_active            = "param_power_active";

    // Performance Macros
    inline constexpr std::string_view macro_gut_squeeze       = "macro_gut_squeeze";
    inline constexpr std::string_view macro_fluid_moisture    = "macro_fluid_moisture";
}

// ============================================================================
// Parameter Definition Structure
// ============================================================================
struct ParameterDef {
    ParameterId      id;
    std::string_view idString;
    std::string_view webProperty;
    std::string_view name;
    float            minValue;
    float            maxValue;
    float            defaultValue;
    std::string_view unit;
    ParameterType    type;
    int              deck; // 1..6, or 0 for macro
    const char* const* choiceStrings { nullptr };
    int              numChoices { 0 };
};

namespace Choices {
    inline const char* const Manifold[] = {
        "Sigmoid Horn", "Chladni Ring", "Glottal-Colonic", "Porcelain Cavity"
    };
    inline const char* const Damping[] = {
        "Mucosal Tissue", "Porcelain", "Wet Elastomer", "Bell Bronze", "Stiff Cartilage"
    };
    inline const char* const Boundary[] = {
        "Dry", "Standard", "Water-Coupled", "Tiled"
    };
    inline const char* const ZenoUnroll[] = {
        "Auto (Zeno)", "K=1", "K=4", "K=8", "K=16"
    };
    inline const char* const DimensionD[] = {
        "Mode I", "Mode II", "Mode III", "Mode IV"
    };
    inline const char* const Voicing[] = {
        "Mono-Legato", "Poly 8-Voice", "Stereo Unison"
    };
}

inline const std::array<ParameterDef, kNumParameters>& getAllParameterDefs() noexcept {
    static const std::array<ParameterDef, kNumParameters> kDefs = {{
        // Deck 01
        { ParameterId::ColonicPressure, ParamIDs::colonic_pressure, "colonicPressure", "Kolondruck / Colonic Pressure", 0.0f, 1.0f, 0.70f, "", ParameterType::Float, 1, nullptr, 0 },
        { ParameterId::MucosalTension, ParamIDs::mucosal_tension, "mucosalTension", "Schleimhautspannung / Mucosal Tension", 0.0f, 1.0f, 0.50f, "", ParameterType::Float, 1, nullptr, 0 },
        { ParameterId::RestingAperture, ParamIDs::resting_aperture, "restingAperture", "Ruheöffnung / Resting Aperture", 0.1f, 5.0f, 1.0f, "mm", ParameterType::Float, 1, nullptr, 0 },
        { ParameterId::KarnoppStiction, ParamIDs::karnopp_stiction, "karnoppStiction", "Karnopp-Haftreibung / Mucosal Stiction", 0.0f, 1.0f, 0.65f, "", ParameterType::Float, 1, nullptr, 0 },
        { ParameterId::StribeckBreakaway, ParamIDs::stribeck_breakaway, "stribeckBreakaway", "Stribeck-Abreißgeschw. / Breakaway Vel", 0.1f, 10.0f, 2.2f, "m/s", ParameterType::Float, 1, nullptr, 0 },
        { ParameterId::TissueHardness, ParamIDs::tissue_hardness, "tissueHardness", "Gewebehärte / Contact Hardness", 1.2f, 3.0f, 2.1f, "alpha", ParameterType::Float, 1, nullptr, 0 },
        { ParameterId::ImpactRestitution, ParamIDs::impact_restitution, "impactRestitution", "Stoßdämpfung / Tissue Slap Restitution", 0.0f, 1.0f, 0.45f, "", ParameterType::Float, 1, nullptr, 0 },
        { ParameterId::VactrolSagTau, ParamIDs::vactrol_sag_tau, "vactrolSagTau", "Vactrol-Absackzeit / Pressure Droop Tau", 20.0f, 500.0f, 85.0f, "ms", ParameterType::Float, 1, nullptr, 0 },

        // Deck 02
        { ParameterId::BeltramiVortexPitch, ParamIDs::beltrami_vortex_pitch, "beltramiVortexPitch", "Beltrami-Drall / Vortex Pitch", 0.1f, 10.0f, 2.8f, "rad/m", ParameterType::Float, 2, nullptr, 0 },
        { ParameterId::ReynoldsTurbulence, ParamIDs::reynolds_turbulence, "reynoldsTurbulence", "Reynolds-Turbulenz / Jet Turbulence", 0.0f, 1.0f, 0.40f, "", ParameterType::Float, 2, nullptr, 0 },
        { ParameterId::MultiphaseMoisture, ParamIDs::multiphase_moisture, "multiphaseMoisture", "Phasenfeuchte / Multiphase Moisture", 0.0f, 1.0f, 0.35f, "", ParameterType::Float, 2, nullptr, 0 },
        { ParameterId::FluidViscosity, ParamIDs::fluid_viscosity, "fluidViscosity", "Fluidviskosität / Mucosal Viscosity", 0.0f, 1.0f, 0.30f, "", ParameterType::Float, 2, nullptr, 0 },
        { ParameterId::MinnaertBubbleDensity, ParamIDs::minnaert_bubble_density, "minnaertBubbleDensity", "Minnaert-Blasendichte / Bubble Density", 0.0f, 100.0f, 40.0f, "%", ParameterType::Float, 2, nullptr, 0 },
        { ParameterId::PoissonDropletRate, ParamIDs::poisson_droplet_rate, "poissonDropletRate", "Poisson-Tropfenrate / Droplet Rate", 0.0f, 60.0f, 8.5f, "Hz", ParameterType::Float, 2, nullptr, 0 },
        { ParameterId::EuclideanPeristalsisK, ParamIDs::euclidean_peristalsis_k, "euclideanPeristalsisK", "Peristaltik-Impulse / Euclidean Pulses", 1.0f, 16.0f, 5.0f, "pulses", ParameterType::Int, 2, nullptr, 0 },
        { ParameterId::EuclideanPeristalsisN, ParamIDs::euclidean_peristalsis_n, "euclideanPeristalsisN", "Peristaltik-Schritte / Euclidean Steps", 1.0f, 16.0f, 16.0f, "steps", ParameterType::Int, 2, nullptr, 0 },

        // Deck 03
        { ParameterId::TriadCouplingGain, ParamIDs::triad_coupling_gain, "triadCouplingGain", "Triaden-Kopplung / Triad Gain", 0.0f, 2.0f, 0.85f, "", ParameterType::Float, 3, nullptr, 0 },
        { ParameterId::So16SkewGenerator, ParamIDs::so16_skew_generator, "so16SkewGenerator", "SO(16)-Generator / Lie Generator", 0.0f, 1.0f, 0.50f, "", ParameterType::Float, 3, nullptr, 0 },
        { ParameterId::SphericalRetraction, ParamIDs::spherical_retraction, "sphericalRetraction", "Sphärische Retraktion / Norm Retraction", 0.0f, 1.0f, 1.0f, "", ParameterType::Bool, 3, nullptr, 0 },
        { ParameterId::HouseholderCoupling, ParamIDs::householder_coupling, "householderCoupling", "Householder-Streuung / Scattering Depth", 0.0f, 1.0f, 0.45f, "", ParameterType::Float, 3, nullptr, 0 },
        { ParameterId::GeometricManifold, ParamIDs::geometric_manifold, "geometricManifold", "Geometrie-Mannigfaltigkeit / Manifold", 0.0f, 3.0f, 2.0f, "", ParameterType::Choice, 3, Choices::Manifold, 4 },
        { ParameterId::DampingProfile, ParamIDs::damping_profile, "dampingProfile", "Dämpfungsprofil / Damping Profile", 0.0f, 4.0f, 0.0f, "", ParameterType::Choice, 3, Choices::Damping, 5 },
        { ParameterId::PorcelainCavityMix, ParamIDs::porcelain_cavity_mix, "porcelainCavityMix", "Porzellan-Resonanz / Cavity Mix", 0.0f, 100.0f, 35.0f, "%", ParameterType::Float, 3, nullptr, 0 },
        { ParameterId::PorcelainBoundaryMode, ParamIDs::porcelain_boundary_mode, "porcelainBoundaryMode", "Grenzflächen-Modus / Boundary Profile", 0.0f, 3.0f, 1.0f, "", ParameterType::Choice, 3, Choices::Boundary, 4 },

        // Deck 04
        { ParameterId::ZenoUnrollOverride, ParamIDs::zeno_unroll_override, "zenoUnrollOverride", "Zenon-Entfaltung / Unroll Override", 0.0f, 4.0f, 0.0f, "", ParameterType::Choice, 4, Choices::ZenoUnroll, 5 },
        { ParameterId::ZenoConflictThreshold, ParamIDs::zeno_conflict_threshold, "zenoConflictThreshold", "Zenon-Konfliktschwelle / Conflict Sens", 0.1f, 1.0f, 0.55f, "", ParameterType::Float, 4, nullptr, 0 },
        { ParameterId::LorenzOrbitRate, ParamIDs::lorenz_orbit_rate, "lorenzOrbitRate", "Lorenz-Umlauf / Lorenz Orbit Rate", 0.01f, 12.0f, 0.65f, "Hz", ParameterType::Float, 4, nullptr, 0 },
        { ParameterId::LorenzChaosDepth, ParamIDs::lorenz_chaos_depth, "lorenzChaosDepth", "Lorenz-Chaos-Tiefe / Lorenz Chaos Depth", 0.0f, 100.0f, 25.0f, "%", ParameterType::Float, 4, nullptr, 0 },

        // Deck 05
        { ParameterId::DimensionDEnable, ParamIDs::dimension_d_enable, "dimensionDEnable", "Dimension-D Aktiv / BBD Chorus Enable", 0.0f, 1.0f, 1.0f, "", ParameterType::Bool, 5, nullptr, 0 },
        { ParameterId::DimensionDMode, ParamIDs::dimension_d_mode, "dimensionDMode", "Dimension-D Modus / Chorus Mode", 0.0f, 3.0f, 1.0f, "", ParameterType::Choice, 5, Choices::DimensionD, 4 },
        { ParameterId::DimensionDMix, ParamIDs::dimension_d_mix, "dimensionDMix", "Dimension-D Anteil / Chorus Mix", 0.0f, 100.0f, 40.0f, "%", ParameterType::Float, 5, nullptr, 0 },
        { ParameterId::ColonicSaturationDrive, ParamIDs::colonic_saturation_drive, "colonicSaturationDrive", "Kolonsättigung / Colonic Drive", 0.0f, 100.0f, 30.0f, "%", ParameterType::Float, 5, nullptr, 0 },
        { ParameterId::VisceralSubReinforce, ParamIDs::visceral_sub_reinforce, "visceralSubReinforce", "Sub-Bass-Verstärkung / Visceral Sub", -36.0f, 6.0f, 0.0f, "dB", ParameterType::Float, 5, nullptr, 0 },

        // Deck 06
        { ParameterId::GoldenStereoSpread, ParamIDs::golden_stereo_spread, "goldenStereoSpread", "Stereo-Goldenschnitt / Golden Spread", 0.0f, 100.0f, 75.0f, "%", ParameterType::Float, 6, nullptr, 0 },
        { ParameterId::VoicingMode, ParamIDs::voicing_mode, "voicingMode", "Stimmen-Modus / Voicing Mode", 0.0f, 2.0f, 0.0f, "", ParameterType::Choice, 6, Choices::Voicing, 3 },
        { ParameterId::GlidePortamentoMs, ParamIDs::glide_portamento_ms, "glidePortamentoMs", "Gleitzeit / Glide Portamento", 0.0f, 500.0f, 45.0f, "ms", ParameterType::Float, 6, nullptr, 0 },
        { ParameterId::MasterVolumeDb, ParamIDs::master_volume_db, "masterVolumeDb", "Gesamtlautstärke / Master Trim", -36.0f, 6.0f, 0.0f, "dB", ParameterType::Float, 6, nullptr, 0 },
        { ParameterId::PowerActive, ParamIDs::power_active, "powerActive", "Netzschalter / Master Power", 0.0f, 1.0f, 1.0f, "", ParameterType::Bool, 6, nullptr, 0 },

        // Performance Macros
        { ParameterId::MacroGutSqueeze, ParamIDs::macro_gut_squeeze, "macroGutSqueeze", "Bauchpresse / Gut Squeeze", 0.0f, 1.0f, 0.50f, "", ParameterType::Float, 0, nullptr, 0 },
        { ParameterId::MacroFluidMoisture, ParamIDs::macro_fluid_moisture, "macroFluidMoisture", "Schleimhautfeuchte / Fluid Moisture", 0.0f, 1.0f, 0.50f, "", ParameterType::Float, 0, nullptr, 0 }
    }};
    return kDefs;
}

inline ParameterId getParameterIdByName(std::string_view name) noexcept {
    const auto& defs = getAllParameterDefs();
    for (size_t i = 0; i < defs.size(); ++i) {
        if (defs[i].idString == name || defs[i].webProperty == name) {
            return static_cast<ParameterId>(i);
        }
    }
    return ParameterId::ColonicPressure; // Default fallback
}

inline const ParameterDef* getParameterDef(ParameterId id) noexcept {
    const auto index = static_cast<size_t>(id);
    if (index < kNumParameters) {
        return &getAllParameterDefs()[index];
    }
    return nullptr;
}

// ============================================================================
// JUCE APVTS ParameterLayout Generation
// ============================================================================
#if PF02_HAS_JUCE_LAYOUT
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto& defs = getAllParameterDefs();

    for (const auto& def : defs) {
        const juce::String idStr(def.idString.data(), def.idString.size());
        const juce::String nameStr(def.name.data(), def.name.size());
        const juce::ParameterID paramId(idStr, 1);

        switch (def.type) {
            case ParameterType::Float: {
                layout.add(std::make_unique<juce::AudioParameterFloat>(
                    paramId,
                    nameStr,
                    juce::NormalisableRange<float>(def.minValue, def.maxValue),
                    def.defaultValue,
                    juce::AudioParameterFloatAttributes().withLabel(juce::String(def.unit.data(), def.unit.size()))
                ));
                break;
            }
            case ParameterType::Int: {
                layout.add(std::make_unique<juce::AudioParameterInt>(
                    paramId,
                    nameStr,
                    static_cast<int>(def.minValue),
                    static_cast<int>(def.maxValue),
                    static_cast<int>(def.defaultValue),
                    juce::AudioParameterIntAttributes().withLabel(juce::String(def.unit.data(), def.unit.size()))
                ));
                break;
            }

            case ParameterType::Bool: {
                layout.add(std::make_unique<juce::AudioParameterBool>(
                    paramId,
                    nameStr,
                    def.defaultValue >= 0.5f
                ));
                break;
            }
            case ParameterType::Choice: {
                juce::StringArray choiceArray;
                for (int c = 0; c < def.numChoices; ++c) {
                    choiceArray.add(def.choiceStrings[c]);
                }
                layout.add(std::make_unique<juce::AudioParameterChoice>(
                    paramId,
                    nameStr,
                    choiceArray,
                    static_cast<int>(def.defaultValue)
                ));
                break;
            }
        }
    }

    return layout;
}
#endif

} // namespace pf02::parameters
