# Project Description
A two-pass hybrid (ray tracing & rasterization) 3d engine attempts to make physically based rendering for participating media real-time.

Theoretical Framework taken from Classic Photon Mapping (Jensen) & Photon Beam Methods (Jarosz). Details refer to `docs/literatures/guide.md` (all subsequently mentioned paper/author can be found there, refers to `guide.md` first before any attempt to read the whole paper or search on the web)

**In short**: the first pass traces photon/light paths on the CPU and the second pass render the photon beams by rasterization via OpenGL with consideration of camera-side attenuation.

The pipeline works as follows:
1. Trace photon/light paths stochastically, analogy to the classic path tracing but reversed (trace from lights instead of camera); store media interaction events as photon beams.

    Details refer to `docs/project/trace.md` 

2. Cut the view frustum into a number of slices (or alternatively as depth-peeled layers) and calculate their camera-side attenuation.

    Details refer to `docs/project/opacity.md`
3. Rasterize the stored photon beams as camera-facing billboard quads by sampling the camera-side attenuation maps and combining with the beam's own power.

    Details refer to `docs/project/splat.md`

# Project Folder Structure
- `assets/` contains scene related files like meshes & textures
- `external/` contains external libraries e.g. glad, etc.
- `include/` contains header files
- `src/` contains source implementations
- `shaders` contains GLSL shaders used by the second pass