#version 330 core
in float vAttr0;   // instance_id
in float vAttr1;   // bsdf_kind
in float vAttr2;   // bounce_depth
in vec3  vPower;

// aov_mode matches ViewState::PointAov:
//   0=InstanceId  1=BsdfKind  2=BounceDepth
//   3=PowerColor  4=PowerLuminance  5=PowerNormalized
uniform int   aov_mode;
uniform float maxBounce;
uniform float maxPower;

out vec4 FragColor;

vec3 palette(int id) {
    vec3 p[6] = vec3[6](
        vec3(0.90, 0.30, 0.30),
        vec3(0.30, 0.80, 0.40),
        vec3(0.30, 0.50, 0.90),
        vec3(0.90, 0.80, 0.30),
        vec3(0.70, 0.40, 0.85),
        vec3(0.30, 0.80, 0.85)
    );
    return p[id % 6];
}

vec3 heatmap(float t) {
    t = clamp(t, 0.0, 1.0);
    return mix(vec3(0.0, 0.4, 1.0), vec3(1.0, 0.1, 0.0), t);
}

void main() {
    vec3 col;
    if (aov_mode == 1) {
        col = palette(int(vAttr1));               // BsdfKind
    } else if (aov_mode == 2) {
        col = heatmap(vAttr2 / maxBounce);        // BounceDepth
    } else if (aov_mode == 3) {
        col = vPower;                             // PowerColor
    } else if (aov_mode == 4) {
        float lum = max(vPower.r, max(vPower.g, vPower.b));
        col = vec3(lum);                          // PowerLuminance
    } else if (aov_mode == 5) {
        float lum = max(vPower.r, max(vPower.g, vPower.b));
        col = vec3(maxPower > 0.0 ? lum / maxPower : 0.0); // PowerNormalized
    } else {
        col = palette(int(vAttr0));               // InstanceId (default)
    }
    FragColor = vec4(col, 1.0);
}
