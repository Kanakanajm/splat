# Medium Photon Beam Splatting
Photon beams stored are directly splatted onto the screen by building a camera facing billboard.
Suppose the view plane's normal (the direction that the camera is looking at) we call it `w_n` and a photon beam direction `v`
we will get the third axis by `u = v x (-w_n)`. The length of the billboard is the length of the photon beam. 
We pass in the start and end point of each beam into the geometry shader and emit a quad with the start and end point extended in the direction of `u` (and `-u`) with a distance of `r` set by user.

After construction such billboards, we rasterize them and blend their radiance contributes. 
At the fragment shader, we evaluate the equation `k_r(u_t) * scattering_coefficient * photon_power * Tr(v_t) * Tr(w_t) * phase_func(w, v) / sin(w, v)` where `k_r` is the kernel function, `u_t` is the distance of the point on the billboard (project the fragment back) to the line (in direction of `u`), `Tr(w_t)` is the transmittance towards the camera side calculated with the help of transmittance map (see `docs/project/opacity.md`), `Tr(v_t)` is the transmittance along the photon beam (from the start of the beam) for homogenous medium this is trivial and the phase function (isotropic for now).

The depth test is disabled and blending is used to accumulate the radiance.

The camera-side transmittance is calculated with depth-peeling for now on branch `depthpeel`. It is not directly usable since the `main.cpp` there is to test things out.

# TODOs
1. Organize `depthpeel` branch to export the functionality related to transmittance map which will be uploaded as a depth texture array and medium texture array. The evaluation details please refer to `docs/project/opacity.md`
3. Construct camera-aligned billboard in a geometry shader
4. Finish off the rendering pipeline with the final radiance estimate.