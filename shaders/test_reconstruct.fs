#version 330 core

in vec3 vWorldPos;

uniform vec2 resolution;
uniform mat4 invViewProj;

layout (location = 0) out vec4 outReconstructed;
layout (location = 1) out vec4 outExpected;

// Exact copy of reconstructWorldPos from flat_trans.fs
vec3 reconstructWorldPos(float z) {
    vec2 ndc_xy = (gl_FragCoord.xy / resolution) * 2.0 - 1.0;
    float z_ndc = z * 2.0 - 1.0;
    vec4 clipPos = vec4(ndc_xy, z_ndc, 1.0);
    vec4 worldPos = invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

void main()
{
    outReconstructed = vec4(reconstructWorldPos(gl_FragCoord.z), 1.0);
    outExpected      = vec4(vWorldPos, 1.0);
}
