#version 330 core

in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (location = 1) out uint FragMediumId;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2DArray previousDepths;
uniform usampler2DArray previousMedia;
uniform int previousLayerIdx;
uniform float peelEpsilon;
uniform int mediumId;

void main()
{
    vec2 screenUv = gl_FragCoord.xy / vec2(textureSize(previousDepths, 0));
    float previousLayerDepth = texture(previousDepths, vec3(screenUv, float(previousLayerIdx))).r;
    float currentDepth = gl_FragCoord.z;

    if (currentDepth <= previousLayerDepth + peelEpsilon) {
        discard;
    }

    FragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.2);

    uint previousMediumMask = texture(previousMedia, vec3(screenUv, float(previousLayerIdx))).r;
    uint bit = 1u << mediumId;
    
    uint newMask  = gl_FrontFacing
    ? previousMediumMask |  bit   // enter
    : previousMediumMask & ~bit;  // exit

    FragMediumId = newMask;
}
