#pragma once

#include "Parameters.h"
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <optional>

namespace pf02::parameters {

struct Preset {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    std::array<float, kNumParameters> params {};

    Preset();
    Preset(std::string id, std::string name, std::string category, std::string description);

    [[nodiscard]] float getParam(ParameterId paramId) const noexcept {
        const auto idx = static_cast<size_t>(paramId);
        return (idx < kNumParameters) ? params[idx] : 0.0f;
    }

    void setParam(ParameterId paramId, float value) noexcept {
        const auto idx = static_cast<size_t>(paramId);
        if (idx < kNumParameters) {
            params[idx] = value;
        }
    }

    [[nodiscard]] float getParam(std::string_view idStr) const noexcept;
    void setParam(std::string_view idStr, float value) noexcept;
};

class PresetManager {
public:
    static constexpr size_t kNumFactoryPresets = 16;

    // Factory preset queries
    static const std::array<Preset, kNumFactoryPresets>& getFactoryPresets() noexcept;
    static const Preset* getPresetById(std::string_view id) noexcept;
    static const Preset* getPresetByIndex(size_t index) noexcept;

    // RFC 8259 JSON Patch Serialization & Deserialization
    static std::string exportPresetToJson(const Preset& preset, bool pretty = true);
    static bool importPresetFromJson(const std::string& json, Preset& outPreset);

    static std::string exportBankToJson(const std::vector<Preset>& presets, bool pretty = true);
    static bool importBankFromJson(const std::string& json, std::vector<Preset>& outPresets);

private:
    static std::array<Preset, kNumFactoryPresets> initFactoryPresets();
};

} // namespace pf02::parameters
