#include <catch2/catch_test_macros.hpp>

#include "capture_utils.hpp"
#include "ray_camera.hpp"

#include <cmath>
#include <filesystem>

// ── generate_checkpoints ─────────────────────────────────────────────────────

TEST_CASE("generate_checkpoints: standard 6-checkpoint run", "[capture_utils]") {
    const auto v = generate_checkpoints(256, 6);
    REQUIRE(v.size() == 6);
    CHECK(v.front() == 1);
    CHECK(v.back()  == 256);
    // Values strictly increase
    for (size_t i = 1; i < v.size(); ++i)
        CHECK(v[i] > v[i-1]);
}

TEST_CASE("generate_checkpoints: equal spacing", "[capture_utils]") {
    // spp=101, n=6 → interval=20, values 1,21,41,61,81,101
    const auto v = generate_checkpoints(101, 6);
    REQUIRE(v.size() == 6);
    CHECK(v[0] == 1);
    CHECK(v[1] == 21);
    CHECK(v[2] == 41);
    CHECK(v[3] == 61);
    CHECK(v[4] == 81);
    CHECK(v[5] == 101);
}

TEST_CASE("generate_checkpoints: n=1 returns only max_spp", "[capture_utils]") {
    const auto v = generate_checkpoints(64, 1);
    REQUIRE(v.size() == 1);
    CHECK(v[0] == 64);
}

TEST_CASE("generate_checkpoints: more checkpoints than spp deduplicates", "[capture_utils]") {
    // spp=3, n=10 → can only produce {1, 2, 3}
    const auto v = generate_checkpoints(3, 10);
    REQUIRE(v.size() == 3);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 3);
}

TEST_CASE("generate_checkpoints: zero/negative inputs return empty", "[capture_utils]") {
    CHECK(generate_checkpoints(0, 6).empty());
    CHECK(generate_checkpoints(256, 0).empty());
}

// ── parse_checkpoint_spps ────────────────────────────────────────────────────

TEST_CASE("parse_checkpoint_spps: standard list", "[capture_utils]") {
    const auto v = parse_checkpoint_spps("1,4,16,64,256");
    REQUIRE(v.size() == 5);
    CHECK(v[0] == 1);
    CHECK(v[1] == 4);
    CHECK(v[2] == 16);
    CHECK(v[3] == 64);
    CHECK(v[4] == 256);
}

TEST_CASE("parse_checkpoint_spps: single value", "[capture_utils]") {
    const auto v = parse_checkpoint_spps("64");
    REQUIRE(v.size() == 1);
    CHECK(v[0] == 64);
}

TEST_CASE("parse_checkpoint_spps: empty string", "[capture_utils]") {
    CHECK(parse_checkpoint_spps("").empty());
}

TEST_CASE("parse_checkpoint_spps: skips non-positive values and invalid tokens", "[capture_utils]") {
    const auto v = parse_checkpoint_spps("0,-1,16,foo,64");
    REQUIRE(v.size() == 2);
    CHECK(v[0] == 16);
    CHECK(v[1] == 64);
}

// ── PinholeCamera JSON round-trip ────────────────────────────────────────────

TEST_CASE("PinholeCamera: JSON save/load round-trip", "[ray_camera]") {
    const PinholeCamera cam{
        .eye    = {1.5f, 2.0f, -3.0f},
        .target = {0.0f, 0.5f,  0.0f},
        .up     = {0.0f, 1.0f,  0.0f},
        .fov_y  = 1.0471975f,   // pi/3
        .width  = 512u,
        .height = 384u,
    };

    std::filesystem::create_directories(TEST_OUTPUT_DIR);
    const std::string path = std::string(TEST_OUTPUT_DIR) + "/cam_roundtrip.json";

    cam.save_json(path);
    const PinholeCamera loaded = PinholeCamera::load_json(path);

    CHECK(loaded.eye.x    == cam.eye.x);
    CHECK(loaded.eye.y    == cam.eye.y);
    CHECK(loaded.eye.z    == cam.eye.z);
    CHECK(loaded.target.x == cam.target.x);
    CHECK(loaded.target.y == cam.target.y);
    CHECK(loaded.target.z == cam.target.z);
    CHECK(loaded.up.x     == cam.up.x);
    CHECK(loaded.up.y     == cam.up.y);
    CHECK(loaded.up.z     == cam.up.z);
    CHECK(std::abs(loaded.fov_y - cam.fov_y) < 1e-6f);
    CHECK(loaded.width    == cam.width);
    CHECK(loaded.height   == cam.height);
}

TEST_CASE("PinholeCamera: load_json throws on missing file", "[ray_camera]") {
    CHECK_THROWS(PinholeCamera::load_json("/nonexistent/path/camera.json"));
}
