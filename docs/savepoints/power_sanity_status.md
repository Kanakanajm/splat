# Power Sanity Checks — Savepoint

Branch: `trace`. Side quest alongside V2; all tests go in `tests/photon_tracer_test.cpp`.

## Goal

Statistical energy-conservation tests that verify the power tracking implemented in V2
is physically consistent. Each test uses a controlled scene where the expected aggregate
power can be derived analytically, then checks it holds within a tolerance consistent
with the chosen N (typically ±5% at N=100k).

## Test Descriptions

### Group A — Surface Energy Conservation

**SA-1: Total depth-0 surface power = light.power**
- Scene: closed box (2×2×2), all-white diffuse (albedo=1), vacuum, `light.power={1,1,1}`, N=100k
- Assert: `sum(point.power, bounce_depth==0)` ≈ `{1,1,1}` within ±2%
- Rationale: closed box → every photon hits a wall at depth 0; sum = N × (P/N) = P

**SA-2: Depth-k surface power decays by albedo**
- Scene: same closed box, albedo `a` ∈ {0.5, 0.8}, N=100k, `max_depth=5`
- Assert: `sum(depth k+1) / sum(depth k)` ≈ `a` within ±5% for k ∈ {0,1,2,3}
- Rationale: each diffuse bounce multiplies weight by `bsdf.color = a`; RR is unbiased so the ratio holds in expectation
- Note: set `light.power = {N, N, N}` so `init_weight = 1.0` and `prr = 0.95` rather than hitting the 0.05 clamp

### Group B — Medium Beam Energy Conservation

**MB-1: Total depth-0 beam power = light.power**
- Scene: large box (10×10×10), default diffuse walls, `medium 1: σ_s=100, σ_a=0`, light at center with `medium_id=1`, N=100k, `max_depth=1`
- Assert: `sum(beam.power, bounce_depth==0)` ≈ `{1,1,1}` within ±2%
- Rationale: mean free path = 1/σ_t = 0.01 ≪ box half-extent; all photons scatter before reaching walls; sum = N × (P/N) = P
- Note: diffuse walls do not trigger a medium switch (`is_refract=false`), so photons stay in medium 1 throughout

**MB-2: Beam depth ratio decays by σ_s/σ_t**
- Scene: same large box, `σ_s=100`, `σ_a` ∈ {0, 20, 50} → albedos {1.00, 0.83, 0.67}, N=100k, `max_depth=5`
- Assert: `sum(depth k+1 beams) / sum(depth k beams)` ≈ `σ_s/σ_t` within ±5%
- Rationale: `weight *= σ_s/σ_t` at each medium scatter; same RR unbiasedness argument as SA-2
- Note: set `light.power = {N, N, N}` for same reason as SA-2; only sum `beam.power` (exclude points)

### Group C — Combined Conservation

**CB-1: Per-depth combined (beams + points) sum = light.power**
- Scene: closed box (2×2×2), all-white diffuse walls, `medium 1: σ_s=5, σ_a=0` inside (light `medium_id=1`), N=100k, `max_depth=4`
- Assert: for each k ∈ {0,1,2,3}: `sum(beam.power, depth==k) + sum(point.power, depth==k)` ≈ `{1,1,1}` within ±5%
- Rationale: no power loss anywhere (σ_a=0, surface albedo=1, RR unbiased, closed scene) → expected power at every depth equals the initial total flux

## Tasks

| # | Task | Status |
|---|---|---|
| SA-1 | `"surface depth-0 power sum equals light flux"` | ✅ |
| SA-2 | `"surface power ratio per depth equals albedo"` | ✅ |
| MB-1 | `"beam depth-0 power sum equals light flux"` | ✅ |
| MB-2 | `"beam power ratio per depth equals single-scatter albedo"` | ✅ |
| CB-1 | `"combined per-depth power sum conserved in lossless scene"` | ✅ |

## Notes

- All tests use the existing `make_box` helper and `Catch::Approx` / relative tolerance via `REQUIRE(val == Catch::Approx(expected).epsilon(0.05))` or manual relative check.
- `light.power = {N, N, N}` pattern (from the nested-medium test) keeps `init_weight = 1.0` so `prr` reflects the actual path weight rather than being clamped to 0.05.
- MB-1/MB-2 unit tests use `σ_s=100` in a 10×10×10 box (mean free path=0.01) so depth-0 wall hits are astronomically unlikely and all records are beams.
- The companion scene files (`mb1.obj/.json`, `mb2-abs.obj/.json`) use a 10×10×10 box with `σ_s=3` (mean free path=0.33) for visual spread. P(depth-0 wall hit) = exp(−15) ≈ 0. Using a 2×2×2 box would let ~5% of photons reach the wall at depth 0.
- CB-1 is the most comprehensive test: it validates both surface and medium power tracking together in a single scene.

## Visual Verification

Scene files under `assets/models/sanity/energy-conservation/` (`sa1`, `sa2`, `mb1`, `mb2-abs`, `cb1`). All loaded in the app and checked with AOVs. Key observations recorded during review:

- **SA-1** (white box, vacuum): depth-0 point density uniform across the box; PowerNormalized appears dark at early depths because RR-clamped prr=0.95 causes per-photon weight to grow each bounce, making deep survivors brightest — expected, not a bug.
- **SA-2** (grey box, albedo=0.5): clear brightness drop per depth in PowerNormalized; depth-0 is global max (1.0), deeper depths stabilise at 0.5.
- **MB-1** (pure scattering, σ_a=0): all beams uniform brightness across depths. Grey beams appeared initially due to default surface albedo=0.8 — fixed by adding `"white"` BSDF to scene JSON. MB scene OBJs enlarged to 10×10×10 to eliminate depth-0 surface points (P(wall hit at depth 0) ≈ exp(−15) ≈ 0).
- **MB-2** (absorbing, σ_s/σ_t=0.667): visible drop from depth-0 (white) to depth-1 (grey). Depth 1 and 2+ appear similar because per-beam power stabilises at the albedo fixed point (0.667) once weight drops below the 0.95 prr clamp; decay beyond depth 1 is in beam **density** not per-beam brightness.
- **CB-1** (lossless combined): beams and points at all depths same brightness.

Pixel picker (left-click viewport → RGB readout in debug panel) added to `main.cpp` + `debug_ui.cpp` to assist quantitative AOV inspection.

## Status: Complete (5/5)
