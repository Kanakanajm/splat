#include "scene_config.hpp"

#include "scene.hpp"

#include <json.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

// ---- helpers ----------------------------------------------------------------

static BsdfKind parse_bsdf_kind(const std::string& s) {
    if (s == "Diffuse")     return BsdfKind::Diffuse;
    if (s == "Conductor")   return BsdfKind::Conductor;
    if (s == "Dielectric")  return BsdfKind::Dielectric;
    if (s == "MediumShell") return BsdfKind::MediumShell;
    throw std::runtime_error("SceneConfig: unknown bsdf kind '" + s + "'");
}

static tinybvh::bvhvec3 parse_vec3(const json& arr) {
    return {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
}

// ---- SceneConfig::load ------------------------------------------------------

SceneConfig SceneConfig::load(const std::string& model_path) {
    const auto json_path =
        std::filesystem::path(model_path).replace_extension(".json").string();

    std::ifstream f(json_path);
    if (!f)
        throw std::runtime_error("SceneConfig: cannot open '" + json_path + "'");

    json doc;
    try { doc = json::parse(f); }
    catch (const json::exception& e) {
        throw std::runtime_error(std::string("SceneConfig: JSON parse error: ") + e.what());
    }

    SceneConfig cfg;

    // --- mediums -------------------------------------------------------------
    if (doc.contains("mediums")) {
        for (auto& [name, val] : doc["mediums"].items()) {
            if (name == "vacuum")
                throw std::runtime_error("SceneConfig: 'vacuum' is a reserved medium name");
            cfg.mediums_[name] = MediumCfg{
                val.at("sigma_s").get<float>(),
                val.at("sigma_a").get<float>()
            };
        }
    }

    // --- bsdfs ---------------------------------------------------------------
    if (doc.contains("bsdfs")) {
        for (auto& [name, val] : doc["bsdfs"].items()) {
            BsdfCfg bc{parse_bsdf_kind(val.at("kind").get<std::string>())};
            if (val.contains("color"))               bc.color               = parse_vec3(val["color"]);
            if (val.contains("transmittance_color")) bc.transmittance_color = parse_vec3(val["transmittance_color"]);
            cfg.bsdfs_[name] = bc;
        }
    }

    // --- instances -----------------------------------------------------------
    auto require_medium = [&](const std::string& name, const std::string& ctx) {
        if (!name.empty() && name != "vacuum" && !cfg.mediums_.count(name))
            throw std::runtime_error("SceneConfig: " + ctx + ": medium '" + name + "' not defined");
    };
    auto require_bsdf = [&](const std::string& name, const std::string& ctx) {
        if (!name.empty() && !cfg.bsdfs_.count(name))
            throw std::runtime_error("SceneConfig: " + ctx + ": bsdf '" + name + "' not defined");
    };

    if (doc.contains("instances")) {
        for (auto& [name, val] : doc["instances"].items()) {
            InstanceCfg ic;
            if (val.contains("bsdf"))       ic.bsdf       = val["bsdf"].get<std::string>();
            if (val.contains("medium_in"))  ic.medium_in  = val["medium_in"].get<std::string>();
            if (val.contains("medium_out")) ic.medium_out = val["medium_out"].get<std::string>();

            require_bsdf  (ic.bsdf,       "instance '" + name + "'");
            require_medium(ic.medium_in,  "instance '" + name + "'");
            require_medium(ic.medium_out, "instance '" + name + "'");

            cfg.instances_[name] = std::move(ic);
        }
    }

    // --- light ---------------------------------------------------------------
    if (!doc.contains("light"))
        throw std::runtime_error("SceneConfig: missing required 'light' block");

    const auto& l = doc["light"];
    cfg.light_pos_      = parse_vec3(l.at("position"));
    if (l.contains("power"))    cfg.light_power_    = parse_vec3(l["power"]);
    if (l.contains("medium")) {
        cfg.light_medium_ = l["medium"].get<std::string>();
        require_medium(cfg.light_medium_, "light");
    }

    return cfg;
}

// ---- SceneConfig::apply -----------------------------------------------------

PointLight SceneConfig::apply(Scene& scene) const {
    // Assign stable integer IDs (deterministic: sorted by name for reproducibility).
    std::unordered_map<std::string, uint32_t> bsdf_ids;
    std::unordered_map<std::string, uint32_t> medium_ids;
    medium_ids["vacuum"] = 0u;

    uint32_t next_bsdf = 1u;
    for (auto& [name, cfg] : bsdfs_) {
        bsdf_ids[name] = next_bsdf;
        scene.set_bsdf(next_bsdf, Bsdf{cfg.kind, 1.0f, cfg.color, cfg.transmittance_color});
        ++next_bsdf;
    }

    uint32_t next_medium = 1u;
    for (auto& [name, cfg] : mediums_) {
        medium_ids[name] = next_medium;
        scene.set_medium(next_medium, Medium{cfg.sigma_s, cfg.sigma_a});
        ++next_medium;
    }

    auto resolve_bsdf = [&](const std::string& name) -> uint32_t {
        if (name.empty()) return 0u;
        return bsdf_ids.at(name);
    };
    auto resolve_medium = [&](const std::string& name) -> uint32_t {
        if (name.empty() || name == "vacuum") return 0u;
        return medium_ids.at(name);
    };

    const RayModel& model = scene.model();
    for (auto& [name, ic] : instances_) {
        auto id_opt = model.find_instance(name);
        if (!id_opt)
            throw std::runtime_error("SceneConfig: instance '" + name + "' not found in model");
        const uint32_t iid = *id_opt;
        scene.set_instance_bsdf(iid, resolve_bsdf(ic.bsdf));
        scene.set_instance_medium(iid, resolve_medium(ic.medium_in), resolve_medium(ic.medium_out));
    }

    const uint32_t light_mid = resolve_medium(light_medium_);
    return PointLight{ light_pos_, light_power_, light_mid };
}
