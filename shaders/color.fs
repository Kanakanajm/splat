#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D texture1;
uniform sampler2D texture2;

uniform vec3 objectColor;
uniform vec3 lightColor;

void main() {
    float ambientStrength = 0.1;
    // there can also be ambient material and ambient light
    // here we take the same lightColor as diffuse
    vec3 ambient = ambientStrength * lightColor;

    vec3 result = ambient * objectColor;

    FragColor = vec4(result, 1.0);
}