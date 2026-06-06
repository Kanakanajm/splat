#version 330 core
out vec4 FragColor;

uniform sampler2D splatBuffer;
uniform vec2      resolution;

void main() {
    vec2 uv    = gl_FragCoord.xy / resolution;
    vec3 hdr   = texture(splatBuffer, uv).rgb;
    // Linear → sRGB gamma correction (approximate, gamma 2.2)
    FragColor = vec4(pow(max(hdr, vec3(0.0)), vec3(1.0 / 2.2)), 1.0);
}
