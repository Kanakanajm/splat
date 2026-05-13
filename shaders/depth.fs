#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2DArray depthTextures;
uniform int layer;
uniform float nearPlane;
uniform float farPlane;

float linearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) /
           (farPlane + nearPlane - z * (farPlane - nearPlane));
}

void main()
{
    float rawDepth = texture(depthTextures, vec3(TexCoord, float(layer))).r;

    float linearDepth = linearizeDepth(rawDepth);
    float gray = linearDepth / farPlane;

    FragColor = vec4(vec3(gray), 1.0);
}
