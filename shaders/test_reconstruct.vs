#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;

out vec3 vWorldPos;

void main()
{
    vWorldPos = aPos;  // model = identity, so world pos == local pos
    gl_Position = projection * view * vec4(aPos, 1.0);
}
