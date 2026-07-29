#version 330 core

uniform int instanceId;

out uint fragInstanceId;

void main() {
    fragInstanceId = uint(instanceId);
}
