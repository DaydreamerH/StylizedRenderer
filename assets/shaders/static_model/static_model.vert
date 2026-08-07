#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord0;

layout(location = 0) out vec3 vertexNormal;
layout(location = 1) out vec2 vertexTexCoord0;
layout(location = 2) out vec3 vertexWorldPosition;

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat3 uNormalMatrix;

void main()
{
    const vec4 worldPosition =
        uModel * vec4(inPosition, 1.0);

    vertexNormal = normalize(uNormalMatrix * inNormal);

    vertexTexCoord0 = inTexCoord0;

    vertexWorldPosition = worldPosition.xyz;

    gl_Position = uViewProjection * worldPosition;
}
