#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aInstanceId;
layout(location = 2) in float aBsdfKind;
layout(location = 3) in float aBounceDepth;
layout(location = 4) in vec3  aPower;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform float pointSize;

out float vAttr0;   // instance_id
out float vAttr1;   // bsdf_kind
out float vAttr2;   // bounce_depth
out vec3  vPower;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    gl_PointSize = pointSize;
    vAttr0 = aInstanceId;
    vAttr1 = aBsdfKind;
    vAttr2 = aBounceDepth;
    vPower = aPower;
}
