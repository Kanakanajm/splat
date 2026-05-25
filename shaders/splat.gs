#version 330 core

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 vsNormal[];
in vec3 vsIncomingDir[];
in vec3 vsPower[];
in vec3 vsBsdfColor[];

uniform mat4  view;
uniform mat4  projection;
uniform float h;  // kernel bandwidth

out vec2  vUV;
out vec3  vPower;
out vec3  vBsdfColor;
out float vCosTheta;

void main() {
    vec3 worldPos = gl_in[0].gl_Position.xyz;
    vec3 n        = normalize(vsNormal[0]);

    // Cosine of angle between incoming photon direction and surface normal.
    // normal is oriented so dot(incomingDir, normal) < 0, hence negation.
    float cosTheta = max(0.0, -dot(normalize(vsIncomingDir[0]), n));

    // Orthonormal tangent frame in the surface plane.
    vec3 t1;
    if (abs(n.x) < 0.9)
        t1 = normalize(cross(n, vec3(1.0, 0.0, 0.0)));
    else
        t1 = normalize(cross(n, vec3(0.0, 1.0, 0.0)));
    vec3 t2 = cross(n, t1);

    // Equilateral triangle circumradius that encloses kernel support h.
    // g = (1/2 + sqrt(2)) * h  (Sturzlinger & Bastos, Fig. 1)
    float g    = (0.5 + sqrt(2.0)) * h;
    float sq3h = sqrt(3.0) * 0.5;

    // (t1, t2) offsets for the three vertices.
    vec2 ofs[3];
    ofs[0] = vec2( g,          0.0      );
    ofs[1] = vec2(-g * 0.5,    g * sq3h );
    ofs[2] = vec2(-g * 0.5,   -g * sq3h );

    mat4 VP = projection * view;

    for (int i = 0; i < 3; ++i) {
        vec3 p  = worldPos + ofs[i].x * t1 + ofs[i].y * t2;
        gl_Position = VP * vec4(p, 1.0);
        // UV: center of kernel → (0.5, 0.5); scale = 1/(2h).
        vUV        = vec2(0.5) + ofs[i] / (2.0 * h);
        vPower     = vsPower[0];
        vBsdfColor = vsBsdfColor[0];
        vCosTheta  = cosTheta;
        EmitVertex();
    }
    EndPrimitive();
}
