#pragma once

#include "bsdf.hpp"
#include "point_light.hpp"

#include <string>
#include <unordered_map>

class Scene;

// Loads a JSON sidecar file (same path as the model, .json extension) and
// applies BSDF/medium assignments to a Scene. Instance names are validated
// against the model at apply() time; unknown names throw std::runtime_error.
class SceneConfig {
public:
    static SceneConfig load(const std::string& model_path);

    // Populates scene's bsdf/medium tables and per-instance assignments.
    // Returns the configured PointLight. Throws on any inconsistency.
    PointLight apply(Scene& scene) const;

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

    std::unordered_map<std::string, BsdfCfg>     bsdfs_;
    std::unordered_map<std::string, MediumCfg>   mediums_;
    std::unordered_map<std::string, InstanceCfg>  instances_;
    tinybvh::bvhvec3 light_pos_{};
    tinybvh::bvhvec3 light_power_ = {1.0f, 1.0f, 1.0f};
    tinybvh::bvhvec3 light_emit_dir_{};  // zero = full sphere
    std::string      light_medium_;       // empty = vacuum
};
