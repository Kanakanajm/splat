#version 330 core
#extension GL_ARB_gpu_shader5 : enable

out vec4 FragColor;

uniform sampler2DArray depthMap;
uniform usampler2DArray mediumMap;
uniform float far;
uniform float near;

uniform int numLayers;

uniform vec2 resolution;
uniform mat4 invViewProj;
uniform vec3 cameraWorldPos;

float attenuation(int id)
{
    if (id == 1) return 0.2;
    if (id == 2) return 0.6;
    if (id == 3) return 1.0;
    if (id == -1) return 0.0; // vacuum
    return 0.0; // bad!
}

vec3 reconstructWorldPos(float z) {
    // TODO: check if x,y in window space offset by 0.5?
    vec2 ndc_xy = ((gl_FragCoord.xy - vec2(0.5)) / resolution) * 2.0 - 1.0;

    float z_ndc = z * 2.0 - 1.0; // z range from 0 to 1

    vec4 clipPos = vec4(ndc_xy, z_ndc, 1.0);
    vec4 worldPos = invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;  // perspective divide
}

void main()
{
    uint lastMediumMask = 0u;
    int lastMediumId = -1;
    vec3 lastDepthWorld = cameraWorldPos;
    float transmittance = 1.0;
    
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
        lastMediumMask = texelFetch(mediumMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        lastMediumId = findMSB(lastMediumMask);
    }

    FragColor = vec4(transmittance, transmittance, transmittance, 0.8);

}
