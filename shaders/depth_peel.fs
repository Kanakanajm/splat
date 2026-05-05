#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D previousDepth;
uniform float peelEpsilon;

void main()
{
    vec2 screenUv = gl_FragCoord.xy / vec2(textureSize(previousDepth, 0));
    float firstLayerDepth = texture(previousDepth, screenUv).r;
    float currentDepth = gl_FragCoord.z;

    if (currentDepth <= firstLayerDepth + peelEpsilon) {
        discard;
    }

    FragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.2);
}
