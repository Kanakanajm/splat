#version 330 core

in vec3 vWorldPos;

out vec3 fragFaceNormal;

void main() {
    // Compute the geometric (flat) face normal from screen-space position derivatives.
    // dFdx/dFdy are constant within a triangle, so cross product is the exact face normal.
    // abs(dot) in the guard test makes the sign irrelevant.
    fragFaceNormal = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
}
