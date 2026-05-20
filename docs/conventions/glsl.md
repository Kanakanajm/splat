# Files
Vertex, fragment, geometry and compute shaders file should have respectively `_vert`, `_frag`, `_geom` and `_comp` suffix (ex: `eevee_film_frag.glsl`).
Shader files name must be unique and must be prefixed by the module they are part of (ex: `workbench_material_lib.glsl`, `eevee_film_lib.glsl`).
A shader file must contain one and only one `main()` function.
If a shader file does not contain a `main()` function it is considered as a shader library and must have _lib suffix in its filename.
Put code shared between multiple shaders inside library files.

*Excerpt from https://developer.blender.org/docs/handbook/guidelines/glsl/ (no need to read this if not requested)*