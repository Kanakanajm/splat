#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aIncomingDir;  // reserved for glossy BRDF
layout(location = 3) in vec3 aPower;
layout(location = 4) in vec3 aBsdfColor;

uniform mat4 model;

out vec3 vsNormal;
out vec3 vsIncomingDir;
out vec3 vsPower;
out vec3 vsBsdfColor;

void main() {
    // Pass world-space position to GS; view*projection applied per emitted vertex.
    gl_Position   = model * vec4(aPos, 1.0);
    vsNormal      = mat3(model) * aNormal;
    vsIncomingDir = mat3(model) * aIncomingDir;
    vsPower       = aPower;
    vsBsdfColor   = aBsdfColor;
}
