# Capture Pass — Savepoint

Branch: `capture` (merged to `main` via PR #14). Depends on splat_v1 and trace_v2 being complete.

## Goal

Enable offline high-photon-count renders by splitting a large photon trace into multiple
sub-passes and accumulating results into an EXR image file. The interactive window stays
responsive; capture is a separate one-shot mode.

## Architecture

### Capture mode flow

1. **CLI / UI**: user selects total photon count N and sub-pass batch size M.
2. **Sub-pass loop** (runs N/M iterations):
   - Trace M photons via `PhotonTracer`.
   - Upload photon VBOs to GPU.
   - Render geometry + splat pass into HDR FBO.
   - Accumulate HDR FBO into a persistent accumulation FBO (additive).
3. **Finalize**: divide accumulation buffer by (N/M) → write RGB channels to EXR via `tinyexr`.

### Key design decisions

- **Additive accumulation, divide at end** — avoids precision loss from per-pass 1/N division;
  one division at export time.
- **`photon_tracer.cpp` reset** — `PhotonTracer` clears beam/point vectors before each sub-pass
  so memory stays bounded at M photons.
- **tinyexr for EXR output** — single-header library added to `external/`; already used by the
  existing image buffer.

### Files Changed

| File | Change |
|---|---|
| `include/debug_ui.hpp` | `CaptureState` struct: total N, batch M, output path, trigger flag |
| `include/photon_tracer.hpp` | `reset()` method to clear stored photons between sub-passes |
| `src/debug_ui.cpp` | Capture panel in ImGui (path, N, M, Start button) |
| `src/main.cpp` | Capture loop: sub-pass trace → upload → render → accumulate → EXR export |
| `src/photon_tracer.cpp` | `reset()` implementation |
| `src/scene_gl.cpp` | `accumulate_fbo` helper; additive accumulation pass |
| `external/tinyexr.{h,cpp}` | Added tinyexr library |

## Status: Complete — merged to main (PR #14)
