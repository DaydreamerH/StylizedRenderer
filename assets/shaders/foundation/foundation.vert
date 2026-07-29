#version 450 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUv;

layout (location = 0) out vec2 vertexUv;

uniform mat4 uViewProjection;

void main()
{
    vertexUv = inUv;
    gl_Position = uViewProjection * vec4(inPosition, 1.f);
}