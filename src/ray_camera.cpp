#include "ray_camera.hpp"

#include <json.hpp>

#include <cmath>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

tinybvh::Ray PinholeCamera::generate_ray(uint32_t px, uint32_t py) const {
    using tinybvh::bvhvec3;
    using tinybvh::tinybvh_cross;
    using tinybvh::tinybvh_normalize;

    const bvhvec3 forward = tinybvh_normalize(target - eye);
    const bvhvec3 right   = tinybvh_normalize(tinybvh_cross(forward, up));
    const bvhvec3 cam_up  = tinybvh_cross(right, forward);

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float scale  = std::tan(0.5f * fov_y);

    const float nx = (2.0f * (static_cast<float>(px) + 0.5f) / static_cast<float>(width)  - 1.0f) * aspect * scale;
    const float ny = (1.0f - 2.0f * (static_cast<float>(py) + 0.5f) / static_cast<float>(height)) * scale;

    const bvhvec3 dir = tinybvh_normalize(
        bvhvec3{forward.x + nx * right.x + ny * cam_up.x,
                forward.y + nx * right.y + ny * cam_up.y,
                forward.z + nx * right.z + ny * cam_up.z});

    return tinybvh::Ray{eye, dir};
}

std::optional<std::pair<int, int>> PinholeCamera::project(const tinybvh::bvhvec3& world_p) const {
    using tinybvh::bvhvec3;
    using tinybvh::tinybvh_cross;
    using tinybvh::tinybvh_dot;
    using tinybvh::tinybvh_normalize;

    const bvhvec3 forward = tinybvh_normalize(target - eye);
    const bvhvec3 right   = tinybvh_normalize(tinybvh_cross(forward, up));
    const bvhvec3 cam_up  = tinybvh_cross(right, forward);

    const bvhvec3 to_p{world_p.x - eye.x, world_p.y - eye.y, world_p.z - eye.z};
    const float   dist = tinybvh_dot(forward, to_p);
    if (dist <= 0.0f) return std::nullopt;

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float scale  = std::tan(0.5f * fov_y);

    const float ndc_x = tinybvh_dot(right,  to_p) / (dist * aspect * scale);
    const float ndc_y = tinybvh_dot(cam_up, to_p) / (dist * scale);

    const int px = static_cast<int>((ndc_x + 1.0f) * 0.5f * static_cast<float>(width));
    const int py = static_cast<int>((1.0f - ndc_y) * 0.5f * static_cast<float>(height));
    return std::make_pair(px, py);
}

void PinholeCamera::save_json(const std::string& path) const {
    json j;
    j["eye"]    = {eye.x,    eye.y,    eye.z};
    j["target"] = {target.x, target.y, target.z};
    j["up"]     = {up.x,     up.y,     up.z};
    j["fov_y"]  = fov_y;
    j["width"]  = width;
    j["height"] = height;
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot write camera JSON: " + path);
    f << j.dump(4) << "\n";
}

PinholeCamera PinholeCamera::load_json(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot read camera JSON: " + path);
    const json j = json::parse(f);
    auto v3 = [&](const char* k) -> tinybvh::bvhvec3 {
        return {j.at(k)[0].get<float>(), j.at(k)[1].get<float>(), j.at(k)[2].get<float>()};
    };
    return {
        .eye    = v3("eye"),
        .target = v3("target"),
        .up     = v3("up"),
        .fov_y  = j.at("fov_y").get<float>(),
        .width  = j.at("width").get<uint32_t>(),
        .height = j.at("height").get<uint32_t>(),
    };
}
