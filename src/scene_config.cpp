#include "scene_config.hpp"

#include "scene.hpp"

#include <json.hpp>

#include <algorithm>
#include <cfloat>
#include <cmath>
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

    // --- light / lights / env_light ------------------------------------------
    if (!doc.contains("light") && !doc.contains("lights") && !doc.contains("env_light"))
        throw std::runtime_error("SceneConfig: missing 'light', 'lights', or 'env_light' block");

    auto parse_point_light = [&](const auto& l, const std::string& ctx) {
        SceneConfig::PointLightCfg pl;
        pl.pos   = parse_vec3(l.at("position"));
        if (l.contains("power"))  pl.power  = parse_vec3(l["power"]);
        if (l.contains("medium")) {
            pl.medium = l["medium"].template get<std::string>();
            require_medium(pl.medium, ctx);
        }
        cfg.point_lights_.push_back(std::move(pl));
    };

    if (doc.contains("light"))
        parse_point_light(doc["light"], "light");

    if (doc.contains("lights")) {
        for (size_t i = 0; i < doc["lights"].size(); ++i) {
            const auto& l = doc["lights"][i];
            const std::string ctx  = "lights[" + std::to_string(i) + "]";
            const std::string kind = l.contains("kind") ? l["kind"].get<std::string>() : "point";
            if (kind == "point")
                parse_point_light(l, ctx);
            else if (kind == "area") {
                AreaLightCfg al;
                al.instance = l.at("instance").get<std::string>();
                if (l.contains("emission")) al.emission = parse_vec3(l["emission"]);
                cfg.area_lights_.push_back(std::move(al));
            } else
                throw std::runtime_error("SceneConfig: " + ctx + ": unknown light kind '" + kind + "'");
        }
    }

    if (doc.contains("env_light")) {
        EnvLightCfg ec;
        const auto& el = doc["env_light"];
        if (el.contains("color"))      ec.color = parse_vec3(el["color"]);
        else if (el.contains("power")) ec.color = parse_vec3(el["power"]);
        cfg.env_light_ = ec;
    }

    return cfg;
}

// ---- SceneConfig::apply -----------------------------------------------------

std::vector<Light> SceneConfig::apply(Scene& scene) const {
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

    std::vector<Light> lights;

    for (const auto& pl : point_lights_) {
        const uint32_t light_mid = resolve_medium(pl.medium);
        lights.push_back(PointLight{pl.pos, pl.power, light_mid});
    }

    for (const auto& al_cfg : area_lights_) {
        const auto id_opt = model.find_instance(al_cfg.instance);
        if (!id_opt)
            throw std::runtime_error("SceneConfig: area light instance '" + al_cfg.instance + "' not found in model");
        const uint32_t iid = *id_opt;

        AreaLight al;
        al.emission   = al_cfg.emission;
        al.total_area = 0.0f;
        bool normal_set = false;

        const auto& tris_buf = model.triangles();
        for (uint32_t p = 0; p < model.triangle_count(); ++p) {
            if (model.instance_id(p) != iid) continue;
            AreaLight::Tri t;
            t.v0 = {tris_buf[p*3+0].x, tris_buf[p*3+0].y, tris_buf[p*3+0].z};
            t.v1 = {tris_buf[p*3+1].x, tris_buf[p*3+1].y, tris_buf[p*3+1].z};
            t.v2 = {tris_buf[p*3+2].x, tris_buf[p*3+2].y, tris_buf[p*3+2].z};
            const tinybvh::bvhvec3 e1{t.v1.x-t.v0.x, t.v1.y-t.v0.y, t.v1.z-t.v0.z};
            const tinybvh::bvhvec3 e2{t.v2.x-t.v0.x, t.v2.y-t.v0.y, t.v2.z-t.v0.z};
            const tinybvh::bvhvec3 cr = tinybvh::tinybvh_cross(e1, e2);
            const float len = std::sqrt(cr.x*cr.x + cr.y*cr.y + cr.z*cr.z);
            al.total_area += 0.5f * len;
            if (!normal_set && len > 1e-8f) {
                al.normal    = tinybvh::tinybvh_normalize(cr);
                normal_set   = true;
            }
            al.tris.push_back(t);
            al.prim_indices.push_back(p);
        }

        if (al.tris.empty())
            throw std::runtime_error("SceneConfig: area light instance '" + al_cfg.instance + "' has no triangles");

        lights.push_back(std::move(al));
    }

    if (env_light_) {
        // Compute AABB of all triangle vertices, then derive a bounding sphere with 10% margin.
        const auto& tris = scene.model().triangles();
        float ax = FLT_MAX, ay = FLT_MAX, az = FLT_MAX;
        float bx = -FLT_MAX, by = -FLT_MAX, bz = -FLT_MAX;
        for (const auto& v : tris) {
            ax = std::min(ax, v.x); ay = std::min(ay, v.y); az = std::min(az, v.z);
            bx = std::max(bx, v.x); by = std::max(by, v.y); bz = std::max(bz, v.z);
        }
        const tinybvh::bvhvec3 center{(ax + bx) * 0.5f, (ay + by) * 0.5f, (az + bz) * 0.5f};
        const float dx = (bx - ax) * 0.5f, dy = (by - ay) * 0.5f, dz = (bz - az) * 0.5f;
        const float radius = std::sqrt(dx * dx + dy * dy + dz * dz) * 1.1f;
        lights.push_back(EnvLight{env_light_->color, center, radius, 0u});
    }

    return lights;
}
