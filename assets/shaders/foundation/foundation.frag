#version 450 core

layout (location = 0) in vec2 vertexUv;

layout (location = 0) out vec4 outColor;

uniform vec4 uTint;

void main()
{
    const vec3 uvColor = vec3(vertexUv.x, vertexUv.y, 0.5);

    outColor = vec4(uvColor, 1.0) * uTint;
}