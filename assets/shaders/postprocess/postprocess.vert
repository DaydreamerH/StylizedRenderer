#version 450 core

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTextureCoordinate;

layout(location = 0) out vec2 vertexTextureCoordinate;

void main()
{
    vertexTextureCoordinate =
        inTextureCoordinate;

    gl_Position =
        vec4(
            inPosition,
            0.0,
            1.0);
}