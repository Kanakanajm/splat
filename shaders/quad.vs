#version 330 core

// Fullscreen triangle at the far plane (NDC z=1.0).
// No vertex buffer needed — positions are derived from gl_VertexID.
void main() {
    vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(pos[gl_VertexID], 1.0, 1.0);
}
