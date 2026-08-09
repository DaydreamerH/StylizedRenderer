#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord0;

layout(location = 0) out vec3 vertexNormal;
layout(location = 1) out vec2 vertexTexCoord0;
layout(location = 2) out vec3 vertexWorldPosition;
layout(location = 3) out vec4 vertexWorldTangent;

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat3 uNormalMatrix;

void main()
{
    const vec4 worldPosition =
        uModel *
        vec4(inPosition, 1.0);

    const vec3 worldNormal =
        normalize(
            uNormalMatrix *
            inNormal);

    const vec3 transformedTangent =
        mat3(uModel) *
        inTangent.xyz;

    const vec3 orthogonalTangent =
        transformedTangent -
        worldNormal *
        dot(
            worldNormal,
            transformedTangent);

    const float tangentLengthSquared =
        dot(
            orthogonalTangent,
            orthogonalTangent);

    vec3 worldTangent;

    if (tangentLengthSquared > 1.0e-8)
    {
        worldTangent =
            orthogonalTangent *
            inversesqrt(
                tangentLengthSquared);
    }
    else
    {
        const vec3 referenceAxis =
            abs(worldNormal.z) < 0.999
                ? vec3(0.0, 0.0, 1.0)
                : vec3(0.0, 1.0, 0.0);

        worldTangent =
            normalize(
                cross(
                    referenceAxis,
                    worldNormal));
    }

    const float tangentSign =
        inTangent.w < 0.0
            ? -1.0
            : 1.0;

    vertexNormal =
        worldNormal;

    vertexTexCoord0 =
        inTexCoord0;

    vertexWorldPosition =
        worldPosition.xyz;

    vertexWorldTangent =
        vec4(
            worldTangent,
            tangentSign);

    gl_Position =
        uViewProjection *
        worldPosition;
}
