#version 330 core
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in vec3  gWorldPos[];
in float gMediumId[];
in float gT[];
in float gBounceDepth[];
in float gLength[];
in float gSigmaT[];
in vec3  gPower[];

out float vMediumId;
out float vT;
out float vUt;
out float vBounceDepth;
out float vLength;
out float vSigmaT;
out vec3  vPower;
out vec3  vBeamDir;

uniform mat4  view;
uniform mat4  projection;
uniform vec3  cameraDir;   // camera look direction (w_n), normalized
uniform float beamRadius;

// bdir is set once before emitting all 4 corners of the strip.
vec3 _bdir;

void emitCorner(vec3 worldPos, float t, float ut, int src) {
    gl_Position  = projection * view * vec4(worldPos, 1.0);
    vT           = t;
    vUt          = ut;
    vMediumId    = gMediumId[src];
    vBounceDepth = gBounceDepth[src];
    vLength      = gLength[src];
    vSigmaT      = gSigmaT[src];
    vPower       = gPower[src];
    vBeamDir     = _bdir;
    EmitVertex();
}

void main() {
    vec3 p0 = gWorldPos[0];
    vec3 p1 = gWorldPos[1];

    vec3  bdir = p1 - p0;
    float len  = length(bdir);
    if (len < 1e-7) return;
    bdir /= len;
    _bdir = bdir;

    // u = beam_dir x (-camera_dir): billboard tangent perpendicular to both
    vec3 u = cross(bdir, -normalize(cameraDir));
    if (length(u) < 1e-6) return;
    u = normalize(u);

    float r = beamRadius;

    // Triangle strip order: p0-r*u, p0+r*u, p1-r*u, p1+r*u
    emitCorner(p0 - r * u, gT[0], -r, 0);
    emitCorner(p0 + r * u, gT[0], +r, 0);
    emitCorner(p1 - r * u, gT[1], -r, 1);
    emitCorner(p1 + r * u, gT[1], +r, 1);

    EndPrimitive();
}
