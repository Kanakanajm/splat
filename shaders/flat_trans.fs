#version 330 core
#extension GL_ARB_gpu_shader5 : enable

out vec4 FragColor;

uniform sampler2DArray depthMap;
uniform usampler2DArray mediumMap;
uniform float depthScale;
uniform int numLayers;

float attenuation(int id)
{
    if (id == 1) return 0.3;
    if (id == 2) return 0.6;
    if (id == 3) return 1.0;
    if (id == -1) return 0.0; // vacuum
    return 0.0; // bad!
}

void main()
{
    uint lastMediumMask = 0u;
    int lastMediumId = -1;
    float lastDepth = 0.0;
    float transmittance = 1.0;
    for (int i = 0; i < numLayers; ++i)
    {
        float depthValue = texelFetch(depthMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;

        if (depthValue >= gl_FragCoord.z + 0.00001)
        {

            transmittance *= exp(-attenuation(lastMediumId) * (gl_FragCoord.z - lastDepth) * depthScale);
            break;
        }

        transmittance *= exp(-attenuation(lastMediumId) * (depthValue - lastDepth) * depthScale);

        lastDepth = depthValue;
        lastMediumMask = texelFetch(mediumMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        lastMediumId = findMSB(lastMediumMask);
    }

    FragColor = vec4(transmittance, transmittance, transmittance, 1.0);

}
