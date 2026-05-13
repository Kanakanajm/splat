#version 330 core
#extension GL_ARB_gpu_shader5 : enable

out vec4 FragColor;



uniform sampler2DArray depthMap;
uniform usampler2DArray mediumMap;
uniform int numLayers;

vec3 palette(int id)
{
    if (id == 1) return vec3(0.95, 0.35, 0.35);
    if (id == 2) return vec3(0.30, 0.85, 0.45);
    if (id == 3) return vec3(0.25, 0.55, 0.95);
    if (id == -1) return vec3(0.08, 0.08, 0.08); // vacuum
    return vec3(0.55); // bad!
}

void main()
{
    uint lastMediumMask = 0u;
    for (int i = 0; i < numLayers; ++i)
    {
        float depthValue = texelFetch(depthMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        if (depthValue >= gl_FragCoord.z + 0.00001)
        {
            break;
        }
        lastMediumMask = texelFetch(mediumMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
    }

    int lastMediumId = findMSB(lastMediumMask);

    vec3 color = palette(lastMediumId);

    FragColor = vec4(color, 0.6);

}
