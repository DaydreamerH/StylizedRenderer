#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat3 uNormalMatrix;

uniform float uOutlineWidth;

void main()
{
    const vec3 worldPosition =
        (uModel * vec4(inPosition, 1.0)).xyz;

    const vec3 worldNormal =
        normalize(uNormalMatrix * inNormal);

    const vec3 expandedWorldPosition =
        worldPosition +
        worldNormal *
        max(uOutlineWidth, 0.0);

    gl_Position =
        uViewProjection * vec4(expandedWorldPosition, 1.0);
}