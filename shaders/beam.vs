#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aMediumId;
layout(location = 2) in float aT;
layout(location = 3) in float aBounceDepth;
layout(location = 4) in float aLength;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out float vMediumId;
out float vT;
out float vBounceDepth;
out float vLength;

void main() {
    gl_Position  = projection * view * model * vec4(aPos, 1.0);
    vMediumId    = aMediumId;
    vT           = aT;
    vBounceDepth = aBounceDepth;
    vLength      = aLength;
}
