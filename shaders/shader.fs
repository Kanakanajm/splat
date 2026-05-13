#version 330 core

in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (location = 1) out uint FragMediumId;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform int mediumId;

void main() {
    FragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.2);

    uint bit      = 1u << mediumId;
    // assuming the camera is always in vacuum, it will only see front faces.
    // TODO: handle camera medium properly
    FragMediumId = bit;
}
