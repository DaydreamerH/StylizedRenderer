#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord0;
layout(location = 4) in uvec4 inJointIndices;
layout(location = 5) in vec4 inJointWeights;

layout(location = 0) out vec3 vertexNormal;
layout(location = 1) out vec2 vertexTexCoord0;
layout(location = 2) out vec3 vertexWorldPosition;
layout(location = 3) out vec4 vertexWorldTangent;

layout(std430, binding = 0)
readonly buffer SkinningPaletteBuffer
{
    mat4 jointMatrices[];
};

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat3 uNormalMatrix;

uniform bool uSkinningEnabled;

mat4 calculateSkinningMatrix()
{
    return
        inJointWeights.x *
            jointMatrices[inJointIndices.x] +
        inJointWeights.y *
            jointMatrices[inJointIndices.y] +
        inJointWeights.z *
            jointMatrices[inJointIndices.z] +
        inJointWeights.w *
            jointMatrices[inJointIndices.w];
}

void main()
{
    vec3 localPosition = inPosition;
    vec3 localNormal = inNormal;
    vec3 localTangent = inTangent.xyz;

    if (uSkinningEnabled)
    {
        const mat4 skinningMatrix =
            calculateSkinningMatrix();

        localPosition =
            (
                skinningMatrix *
                vec4(inPosition, 1.0)
            ).xyz;

        const mat3 skinningDirectionMatrix =
            mat3(skinningMatrix);

        localNormal =
            skinningDirectionMatrix *
            inNormal;

        localTangent =
            skinningDirectionMatrix *
            inTangent.xyz;
    }

    const vec4 worldPosition =
        uModel *
        vec4(localPosition, 1.0);

    const vec3 worldNormal =
        normalize(
            uNormalMatrix *
            localNormal);

    const vec3 transformedTangent =
        mat3(uModel) *
        localTangent;

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
