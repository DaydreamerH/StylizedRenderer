#version 450 core

layout(location = 0) in vec3 vertexNormal;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

void main()
{
    const vec3 normal = normalize(vertexNormal);

    const vec3 displayColor = normal * 0.5 + 0.5;

    outColor = vec4(displayColor, 1.0);

    outNormal = vec4(displayColor, 1.0);
}
