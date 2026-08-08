#version 450 core

layout(location = 0) in vec3 inPosition;

uniform mat4 uLightViewProjection;
uniform mat4 uModel;

void main()
{
    gl_Position =
        uLightViewProjection *
        uModel *
        vec4(inPosition, 1.0);
}