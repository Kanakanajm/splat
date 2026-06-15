#version 330 core

in float vMediumId;
in float vT;
in float vUt;
in float vBounceDepth;
in float vLength;
in float vSigmaT;
in vec3  vPower;
in vec3  vBeamDir;

// aov_mode matches ViewState::BeamAov:
//   0=MediumId  1=T  2=BounceDepth  3=Length
//   4=BeamPowerStart  5=BeamTransmittancePreview  6=Splat
uniform int   aov_mode;
uniform float maxBounce;
uniform float maxLength;
uniform float beamRadius;
uniform float exposure;

// Camera-side transmittance (depth peel maps)
uniform sampler2DArray  depthMap;
uniform usampler2DArray mediumMap;
uniform int   numPeelLayers;
uniform mat4  invViewProj;
uniform vec3  cameraWorldPos;
uniform vec2  resolution;

// Per-medium extinction and scattering coefficients (indexed by medium_id)
#define MAX_MEDIA 16
uniform float mediaSigmaT[MAX_MEDIA];
uniform float mediaSigmaS[MAX_MEDIA];

uniform vec3 cameraDir;   // normalized camera look direction (world space)

out vec4 FragColor;

#include "medium_stack.glsl"

// ---- helpers ----------------------------------------------------------------

vec3 medium_color(int id) {
    vec3 palette[5] = vec3[5](
        vec3(0.60, 0.60, 0.60),
        vec3(0.40, 0.85, 0.95),
        vec3(0.95, 0.55, 0.30),
        vec3(0.65, 0.95, 0.45),
        vec3(0.85, 0.45, 0.95)
    );
    return palette[id % 5];
}

vec3 heatmap(float t) {
    t = clamp(t, 0.0, 1.0);
    return vec3(
        clamp(2.0 * t - 0.5, 0.0, 1.0),
        clamp(t < 0.5 ? 2.0 * t : 2.0 - 2.0 * t, 0.0, 1.0),
        clamp(1.0 - 2.0 * t, 0.0, 1.0)
    );
}

vec3 reconstructWorldPos(float z) {
    vec2 ndc_xy = ((gl_FragCoord.xy - vec2(0.5)) / resolution) * 2.0 - 1.0;
    vec4 clipPos = vec4(ndc_xy, z * 2.0 - 1.0, 1.0);
    vec4 worldPos = invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

float cameraSideTransmittance() {
    float t         = 1.0;
    vec3  lastPos   = cameraWorldPos;
    int   lastMedId = -1;

    for (int i = 0; i < numPeelLayers; ++i) {
        float layerDepth = texelFetch(depthMap,
            ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        if (layerDepth >= gl_FragCoord.z - 1e-5) break;

        vec3  layerPos = reconstructWorldPos(layerDepth);
        float st = (lastMedId >= 0 && lastMedId < MAX_MEDIA) ? mediaSigmaT[lastMedId] : 0.0;
        t *= exp(-st * length(layerPos - lastPos));

        lastPos = layerPos;

        uint encodedStack = texelFetch(mediumMap,
            ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        uint stk; int top;
        stack_unpack(encodedStack, stk, top);
        lastMedId = stack_empty(top) ? -1 : int(stack_peek(stk, top));
    }

    // Final segment from last peel layer to fragment
    float st    = (lastMedId >= 0 && lastMedId < MAX_MEDIA) ? mediaSigmaT[lastMedId] : 0.0;
    vec3 fragPos = reconstructWorldPos(gl_FragCoord.z);
    t *= exp(-st * length(fragPos - lastPos));

    return t;
}

// ---- main -------------------------------------------------------------------

void main() {
    // --- AOV diagnostic modes (0-5) ---
    if (aov_mode != 6) {
        vec3 color;
        if (aov_mode == 1) {
            color = heatmap(vT);
        } else if (aov_mode == 2) {
            color = heatmap(maxBounce > 0.0 ? vBounceDepth / maxBounce : 0.0);
        } else if (aov_mode == 3) {
            color = heatmap(maxLength > 0.0 ? vLength / maxLength : 0.0);
        } else if (aov_mode == 4) {
            color = exposure * vPower;
        } else if (aov_mode == 5) {
            color = vec3(exp(-vSigmaT * vLength * vT));
        } else {
            color = medium_color(int(vMediumId));
        }
        FragColor = vec4(color, 1.0);
        return;
    }

    // --- AOV 6: full radiance estimate (Splat) ---

    // 1. Epanechnikov 1D kernel: k_r(u) = (3/4r) * max(0, 1 - (u/r)^2)
    float r     = beamRadius;
    float u_n   = vUt / r;
    float k_r   = (3.0 / (4.0 * r)) * max(0.0, 1.0 - u_n * u_n);

    // 2. Transmittance along beam (homogeneous Beer-Lambert)
    float Tr_beam = exp(-vSigmaT * vT * vLength);

    // 3. Camera-side transmittance from depth peel
    float Tr_cam = cameraSideTransmittance();

    // 4. Isotropic phase function: f = 1/(4*pi)
    const float INV_4PI = 0.07957747154594767;

    // 5. sin(w, v): angle between view ray and beam direction
    float cos_wv = abs(dot(normalize(cameraDir), vBeamDir));
    float sin_wv = sqrt(max(0.0, 1.0 - cos_wv * cos_wv));
    sin_wv = max(sin_wv, 1e-4);

    // 6. sigma_s for the beam's medium
    int   medId  = int(vMediumId);
    float sigma_s = (medId >= 0 && medId < MAX_MEDIA) ? mediaSigmaS[medId] : 0.0;

    // Beam radiance estimate (Jarosz et al. 2011, eq. 3)
    vec3 radiance = (exposure * sigma_s * Tr_cam * INV_4PI / sin_wv) * vPower;

    FragColor = vec4(radiance, 1.0);
}
