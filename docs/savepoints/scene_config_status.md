# SceneConfig — Savepoint

Branch: `trace`. Prerequisite for AOV debug feature.

## Goal

Replace hardcoded per-scene BSDF/medium setup in `main.cpp` with a JSON sidecar
file that lives next to the model. `main.cpp` becomes scene-agnostic.

## JSON Format

File: same path/stem as the model, `.json` extension.
Example: `assets/models/cornell-box/CornellBox-Original_fixed.json`

```json
{
  "light": { "position": [0.0, 1.48, 2.18], "medium": "vacuum" },
  "mediums": {
    "fog": { "sigma_s": 8.0, "sigma_a": 1.0 }
  },
  "bsdfs": {
    "shell": { "kind": "MediumShell" }
  },
  "instances": {
    "tallBox": { "bsdf": "shell", "medium_in": "fog", "medium_out": "vacuum" }
  }
}
```

## Rules

- `vacuum` is a reserved medium name (id 0); cannot be redefined.
- `light` block is required.
- Instance not listed → defaults to diffuse BSDF + vacuum (no error).
- Instance listed but `bsdf`/`medium_in`/`medium_out` omitted → same defaults.
- Unknown bsdf or medium name referenced → **throw**.
- Instance name not found in `RayModel` → **throw**.
- Medium closure check (every medium-shell volume is closed geometry) → **deferred**.

## Supported BSDF kinds

| `kind` string | Status |
|---|---|
| `Diffuse` | supported |
| `MediumShell` | supported |
| `Conductor` | deferred |
| `Dielectric` (+ `ior` field) | deferred |

Note: when `Dielectric` is added, the bsdf block will support an optional `ior` float field.
Diffuse color/albedo is also deferred.

## API

```cpp
// include/scene_config.hpp
class SceneConfig {
public:
    static SceneConfig load(const std::string& model_path);  // derives json path automatically
    void apply(Scene& scene) const;
    PointLight light() const;
};
```

`main.cpp` usage:
```cpp
SceneConfig cfg = SceneConfig::load(argv[1]);
cfg.apply(scene);
PointLight light = cfg.light();
```

The procedural (no-file) path in `main.cpp` is unchanged.

## Tasks

| # | Task | Files |
|---|---|---|
| ~~1~~ | ~~CMake: link `nlohmann_json`~~ ✅ | `CMakeLists.txt`, `external/nlohmann/json.hpp`, `src/json.cpp` |
| ~~2~~ | ~~`SceneConfig`: parse + validate + apply~~ ✅ | `include/scene_config.hpp`, `src/scene_config.cpp` |
| ~~3~~ | ~~`main.cpp`: replace hardcoded setup with `SceneConfig::load`~~ ✅ | `src/main.cpp` |
| ~~4~~ | ~~Write cornell box sidecar JSON~~ ✅ | `assets/models/cornell-box/CornellBox-Original_fixed.json` |

## Notes

- `RayModel` gained `instance_name(id)` and `find_instance(name)` — filled from `aiMesh::mName` in the assimp constructor; empty for procedural models.
- `SceneConfig::apply` returns the `PointLight` directly (medium ID resolved during apply).
- nlohmann/json vendored locally: `external/nlohmann/json.hpp` + `src/json.cpp` (implementation TU), same pattern as tinybvh.

## Status: Done
