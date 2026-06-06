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

out vec3  gWorldPos;
out float gMediumId;
out float gT;
out float gBounceDepth;
out float gLength;
out float gSigmaT;
out vec3  gPower;

void main() {
    gWorldPos    = (model * vec4(aPos, 1.0)).xyz;
    gMediumId    = aMediumId;
    gT           = aT;
    gBounceDepth = aBounceDepth;
    gLength      = aLength;
    gSigmaT      = aSigmaT;
    gPower       = aPower;
    gl_Position  = projection * view * vec4(gWorldPos, 1.0);
}
