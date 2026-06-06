# Depth Peel (Camera-Side Transmittance) — Savepoint

Branch: `vol-splat`. Ported from `splat-opa` inventory repo (`/home/jackie/Desktop/master_thesis/splat-opa`).
Scope defined in `docs/project/opacity.md` and `docs/project/splat.md` § TODO 1–2.

## Goal

Build the camera-side transmittance map needed by the beam splatting pass.
For each pixel, the transmittance from the camera to any world-space depth
is approximated by depth-peeling the scene geometry into two `GL_TEXTURE_2D_ARRAY`s:

- **`peel_depth_array_`** (`DEPTH_COMPONENT24`): non-linear clip-space depth of each surface layer.
- **`peel_medium_array_`** (`R32UI`): medium bitmask after crossing that surface.
  - Bit `(1 << medium_id)` is set when entering a medium (front face) and cleared on exit.
  - `findMSB(mask)` gives the active medium ID between layers.

## Architecture

### Shaders

| File | Role |
|---|---|
| `shaders/geom.vs` | Reused as the vertex shader for both peel passes |
| `shaders/depth_peel_init.fs` | Layer 0: renders all geometry, writes medium bitmask (no depth comparison) |
| `shaders/depth_peel.fs` | Layers 1+: discards fragments ≤ previous layer depth, propagates bitmask from previous layer |

**`depth_peel_init.fs`** — pure medium mask write, no peeling:
```glsl
uint bit = 1u << uint(mediumId);
FragMediumId = gl_FrontFacing ? bit : 0u;
```

**`depth_peel.fs`** — peel + propagate:
```glsl
if (gl_FragCoord.z <= prevDepth + peelEpsilon) discard;
uint prevMask = texture(previousMedia, ...).r;
uint bit = 1u << uint(mediumId);
FragMediumId = gl_FrontFacing ? (prevMask | bit) : (prevMask & ~bit);
```

### Scene GL additions

```cpp
void         Scene::init_depth_peel(int width, int height, int max_layers = 8);
void         Scene::draw_geometry_peel(Shader& shader) const;
unsigned int Scene::peel_fbo()          const;
unsigned int Scene::peel_depth_array()  const;
unsigned int Scene::peel_medium_array() const;
int          Scene::peel_max_layers()   const;
```

`draw_geometry_peel` loops over all instances and sets `mediumId = instance_medium_in_[i]`
per draw call (reuses `geom_vao_`). No face culling — both front and back faces are needed.

### Render loop (`main.cpp`)

A depth peel block runs **before** the main geometry/point/beam passes each frame:
1. Layer 0 via `depthPeelInitShader` — no depth test against previous layer.
2. Layers 1..N via `depthPeelShader` — `GL_ANY_SAMPLES_PASSED` query stops when scene is exhausted.
3. Resulting `peelLayerCount` will be passed as `numLayers` to the beam splatting shader.

### Transmittance query (future beam shader)

To evaluate `Tr(w_t)` at a fragment, the beam shader will walk the peel layers:
```
transmittance = 1; last_medium = -1 (vacuum); last_dist = camera
for each layer i:
    if layer_depth >= fragment_depth:
        transmittance *= exp(-sigma_t[last_medium] * (fragment_dist - last_dist))
        break
    transmittance *= exp(-sigma_t[last_medium] * (layer_dist - last_dist))
    last_medium = findMSB(medium_mask[i])
```

## Design Decisions

**Single color attachment (no color array)** — splat-opa stored a color array per layer for
debugging. Dropped here; only depth and medium-mask are needed for transmittance.

**Reuse `geom.vs`** — the vertex shader outputs `vFragPos` and `vNormal` which go unused in
the peel fragment shaders. GLSL allows mismatched VS outputs; they are silently discarded.

**All instances drawn (including vacuum objects)** — objects with `medium_in = 0` (vacuum)
set bit 0. `findMSB(1) = 0` → `sigma_t[0] = 0` → no attenuation. Correct and harmless.

**Feedback loop** — `peel_depth_array_` is both attached to the FBO (layer N write) and
bound as a sampler (layer N-1 read). OpenGL 3.3 feedback-loop detection is per-image
(object + level + layer); different layers of the same array texture are safe in practice.

## Tasks

| # | Task | Files |
|---|---|---|
| ~~1~~ | ~~Depth peel shaders~~ ✅ | `shaders/depth_peel_init.fs`, `shaders/depth_peel.fs` |
| ~~2~~ | ~~`Scene::init_depth_peel` + `draw_geometry_peel` + getters~~ ✅ | `include/scene.hpp`, `src/scene_gl.cpp` |
| ~~3~~ | ~~Peel render loop in `main.cpp`~~ ✅ | `src/main.cpp` |
| ~~4~~ | ~~CPU-mirror tests for mask update and transmittance walk~~ ✅ | `tests/depth_peel_test.cpp` |

## Tests

8 tests in `tests/depth_peel_test.cpp` under `[depth_peel]` tag (tests 59–66, all passing):

- `update_mask` enter/exit single medium
- `update_mask` nested media (Matryoshka, 4 events)
- `medium_id=0` maps to vacuum
- Exit without prior entry → mask stays 0
- Transmittance = 1 before any surface
- Beer-Lambert through a single homogeneous slab (at mid-point and past exit)
- Transmittance through nested media (4 analytical checkpoints)
- Pure vacuum → T = 1

## Status: Complete (4/4 tasks done)

Next: billboard geometry shader + fragment evaluation of beam radiance estimate (`Tr(w_t)` integration).
