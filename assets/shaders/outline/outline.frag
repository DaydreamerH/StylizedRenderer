#version 450 core

layout(location = 0) out vec4 outOutlineMask;

uniform vec3 uOutlineColor;

void main()
{
    outOutlineMask =
        vec4(
            uOutlineColor,
            1.0);
}