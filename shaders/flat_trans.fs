#version 330 core
#extension GL_ARB_gpu_shader5 : enable

out vec4 FragColor;

uniform sampler2DArray depthMap;
uniform usampler2DArray mediumMap;
uniform float far;
uniform int numLayers;

uniform vec2 resolution;
uniform mat4 invViewProj;
uniform vec3 cameraWorldPos;

// 0 = transmittance   1 = world depth from camera   2 = media count
uniform int vizMode;

float attenuation(int id)
{
    if (id == 1) return 0.2;
    if (id == 2) return 0.6;
    if (id == 3) return 1.0;
    if (id == -1) return 0.0; // vacuum
    return 0.0;
}

vec3 reconstructWorldPos(float z) {
    vec2 ndc_xy = (gl_FragCoord.xy / resolution) * 2.0 - 1.0;
    float z_ndc = z * 2.0 - 1.0;
    vec4 clipPos = vec4(ndc_xy, z_ndc, 1.0);
    vec4 worldPos = invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

vec3 countColor(int n)
{
    if (n == 0) return vec3(0.05);
    if (n == 1) return vec3(0.95, 0.35, 0.35);
    if (n == 2) return vec3(0.30, 0.85, 0.45);
    if (n == 3) return vec3(0.25, 0.55, 0.95);
    if (n == 4) return vec3(0.90, 0.80, 0.30);
    return vec3(1.0, 0.0, 1.0); // >4: magenta
}

void main()
{
    int lastMediumId = -1;
    vec3 lastDepthWorld = cameraWorldPos;
    float transmittance = 1.0;
    int count = 0;

    for (int i = 0; i < numLayers; ++i)
    {
        float depthValue = texelFetch(depthMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        vec3 depthWorld = reconstructWorldPos(depthValue);

        if (depthValue >= gl_FragCoord.z + 0.00001)
        {
            vec3 fragWorld = reconstructWorldPos(gl_FragCoord.z);
            transmittance *= exp(-attenuation(lastMediumId) * length(fragWorld - lastDepthWorld));
            break;
        }

        transmittance *= exp(-attenuation(lastMediumId) * length(depthWorld - lastDepthWorld));

        lastDepthWorld = depthWorld;
        uint mediumMask = texelFetch(mediumMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        lastMediumId = findMSB(mediumMask);
        count++;
    }

    if (vizMode == 0)
    {
        FragColor = vec4(vec3(transmittance), 0.8);
    }
    else if (vizMode == 1)
    {
        vec3 fragWorld = reconstructWorldPos(gl_FragCoord.z);
        float dist = length(fragWorld - cameraWorldPos);
        FragColor = vec4(vec3(dist / far), 0.8);
    }
    else
    {
        FragColor = vec4(countColor(count), 0.8);
    }
}
