The attenuation from camera side is still unknown after photon tracing from the lights. To properly render the photon beams, we need to connect the sample point on the photon beam to the camera on which there might encounter other medium or opaque objects.

Accompanied by the splatting operation, to make real-time rendering possible, a pre-computed map is used. In Jarosz's Progressive Photon Beam method, they use Woodcock tracking (a.k.a. Delta Tracking) on a few sample distances per pixel to construct a step-function-like map and evaluate camera-side attenuation with it on the fly.

For our method, we take inspiration of Jarosz's approach with a rasterization-friendly adaptation. We also take inspiration from Kim's Opacity Shadow Map of cutting "complex volume with a set of planes". In our method, we slice the view-frustum of the camera into equal spaced planes. By rendering the depth-buffers of all neighboring pair of planes and construct the map, we can query the transmittance value of any arbitrary position in the scene (similar to Woodcock tracking) later during the splatting phase.

An alternative is to use depth-peeled layer to record medium interaction events as follows:
1. Rasterize the scene with the result bind to a layer in the texture arrays (one for depth, one for current medium)

    In the fragment shader, check for the depth of the previous layer and discard any fragment that is closer or equal to that depth i.e. peel the layer that has been rendered.

    The medium-entry event happens when a fragment is front-facing and conversely, the medium-exit event happens when a fragment is back-facing. Use a bit array to represent the stack of traversed medium with the bit index representing the medium entity and track the medium events by setting and unsetting the corresponding bits. With the medium map, the current medium the camera ray is currently in can be determined by reading out the most significant bit.

2. Repeat Step 1 until there is only background left (no more fragment to render, every layer peeled).

Comparing to the plane cutting method where vertices are dropped during the clipping phase, the depth peeling method has to rasterize all vertices and discard fragments instead which I assume to be more costly.