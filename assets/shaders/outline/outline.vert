#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord0;
layout(location = 4) in uvec4 inJointIndices;
layout(location = 5) in vec4 inJointWeights;

layout(std430, binding = 0)
readonly buffer SkinningPaletteBuffer
{
    mat4 jointMatrices[];
};

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat3 uNormalMatrix;

uniform bool uSkinningEnabled;

uniform float uOutlineWidth;

uniform sampler2D uOutlineWidthMask;

void main()
{
    vec3 localPosition = inPosition;
    vec3 localNormal = inNormal;

    if (uSkinningEnabled)
    {
        const mat4 skinningMatrix =
            jointMatrices[inJointIndices.x] *
                inJointWeights.x +
            jointMatrices[inJointIndices.y] *
                inJointWeights.y +
            jointMatrices[inJointIndices.z] *
                inJointWeights.z +
            jointMatrices[inJointIndices.w] *
                inJointWeights.w;

        localPosition =
            (
                skinningMatrix *
                vec4(inPosition, 1.0)
            ).xyz;

        localNormal =
            normalize(
                mat3(skinningMatrix) *
                inNormal
            );
    }

    const vec3 worldPosition =
        (
            uModel *
            vec4(localPosition, 1.0)
        ).xyz;

    const vec3 worldNormal =
        normalize(
            uNormalMatrix *
            localNormal
        );

    const float widthMask =
        clamp(
            textureLod(
                uOutlineWidthMask,
                inTexCoord0,
                0.0
            ).r,
            0.0,
            1.0
        );

    const float outlineWidth =
        max(uOutlineWidth, 0.0) *
        widthMask;

    const vec3 expandedWorldPosition =
        worldPosition +
        worldNormal *
        outlineWidth;

    gl_Position =
        uViewProjection *
        vec4(
            expandedWorldPosition,
            1.0
        );
}
