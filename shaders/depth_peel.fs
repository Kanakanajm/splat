#version 330 core

in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (location = 1) out int FragMediumId;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D previousDepth;
uniform float peelEpsilon;
uniform int mediumId;

void main()
{
    vec2 screenUv = gl_FragCoord.xy / vec2(textureSize(previousDepth, 0));
    float firstLayerDepth = texture(previousDepth, screenUv).r;
    float currentDepth = gl_FragCoord.z;

    if (currentDepth <= firstLayerDepth + peelEpsilon) {
        discard;
    }

    FragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.2);
    FragMediumId = gl_FrontFacing ? mediumId : -mediumId;
}
