#pragma once

#include "bsdf.hpp"
#include "env_light.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Scene;

// Loads a JSON sidecar file (same path as the model, .json extension) and
// applies BSDF/medium assignments to a Scene. Instance names are validated
// against the model at apply() time; unknown names throw std::runtime_error.
class SceneConfig {
public:
    static SceneConfig load(const std::string& model_path);

    // Populates scene's bsdf/medium tables and per-instance assignments.
    // Returns all configured lights (one per "light"/"env_light" block present).
    // Throws on any inconsistency.
    std::vector<Light> apply(Scene& scene) const;

private:
    struct BsdfCfg {
        BsdfKind         kind;
        tinybvh::bvhvec3 color               = {0.8f, 0.8f, 0.8f};
        tinybvh::bvhvec3 transmittance_color = {1.0f, 1.0f, 1.0f};
    };
    struct MediumCfg  { float sigma_s; float sigma_a; };
    struct InstanceCfg {
        std::string bsdf;       // empty = default diffuse
        std::string medium_in;  // empty = vacuum
        std::string medium_out; // empty = vacuum
    };
    struct EnvLightCfg {
        tinybvh::bvhvec3 color = {1.0f, 1.0f, 1.0f};
    };

    std::unordered_map<std::string, BsdfCfg>     bsdfs_;
    std::unordered_map<std::string, MediumCfg>   mediums_;
    std::unordered_map<std::string, InstanceCfg>  instances_;
    struct PointLightCfg {
        tinybvh::bvhvec3 pos{};
        tinybvh::bvhvec3 power = {1.0f, 1.0f, 1.0f};
        std::string      medium;
    };
    struct AreaLightCfg {
        std::string      instance;
        tinybvh::bvhvec3 emission = {1.0f, 1.0f, 1.0f};
    };
    std::vector<PointLightCfg>    point_lights_;
    std::optional<EnvLightCfg>    env_light_;
    std::vector<AreaLightCfg>     area_lights_;
};
