#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location - 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord0;

layout(location = 0) out vec3 vertexNormal;
layout(location = 1) out vec2 vertexTexCoord0;

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat3 uNormalMatrix;

void main()
{
    vertexNormal = normalize(uNormalMatrix * inNormal);
    vertexTexCoord0 = inTexCoord0;
    gl_Position = uViewProjection * uModel * vec4(inPosition, 1.0);
}