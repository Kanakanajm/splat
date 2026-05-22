#version 330 core
in float vMediumId;
in float vT;
in float vBounceDepth;
in float vLength;

uniform int   aov_mode;  // matches ViewState::BeamAov: 0=MediumId 1=T 2=BounceDepth 3=Length
uniform float maxBounce;
uniform float maxLength;

out vec4 FragColor;

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

// Blue (0) → cyan → green → yellow → red (1)
vec3 heatmap(float t) {
    t = clamp(t, 0.0, 1.0);
    return vec3(
        clamp(2.0 * t - 0.5, 0.0, 1.0),
        clamp(t < 0.5 ? 2.0 * t : 2.0 - 2.0 * t, 0.0, 1.0),
        clamp(1.0 - 2.0 * t, 0.0, 1.0)
    );
}

void main() {
    vec3 color;
    if (aov_mode == 1) {
        // T: gradient along beam from start (blue) to end (red)
        color = heatmap(vT);
    } else if (aov_mode == 2) {
        // BounceDepth: heatmap normalized by maxBounce
        color = heatmap(maxBounce > 0.0 ? vBounceDepth / maxBounce : 0.0);
    } else if (aov_mode == 3) {
        // Length: heatmap normalized by maxLength
        color = heatmap(maxLength > 0.0 ? vLength / maxLength : 0.0);
    } else {
        // MediumId: per-medium palette color
        color = medium_color(int(vMediumId));
    }
    FragColor = vec4(color, 1.0);
}
