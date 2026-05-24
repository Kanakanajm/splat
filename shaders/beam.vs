#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aMediumId;
layout(location = 2) in float aT;
layout(location = 3) in float aBounceDepth;
layout(location = 4) in float aLength;
layout(location = 5) in float aSigmaT;
layout(location = 6) in vec3  aPower;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out float vMediumId;
out float vT;
out float vBounceDepth;
out float vLength;
out float vSigmaT;
out vec3  vPower;

void main() {
    gl_Position  = projection * view * model * vec4(aPos, 1.0);
    vMediumId    = aMediumId;
    vT           = aT;
    vBounceDepth = aBounceDepth;
    vLength      = aLength;
    vSigmaT      = aSigmaT;
    vPower       = aPower;
}
