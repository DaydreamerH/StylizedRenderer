#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 4) in uvec4 inJointIndices;
layout(location = 5) in vec4 inJointWeights;

layout(std430, binding = 0)
readonly buffer SkinningPaletteBuffer
{
    mat4 jointMatrices[];
};

uniform mat4 uLightViewProjection;
uniform mat4 uModel;

uniform bool uSkinningEnabled;

void main()
{
    vec3 localPosition = inPosition;

    if (uSkinningEnabled)
    {
        const mat4 skinningMatrix =
            jointMatrices[inJointIndices.x] * inJointWeights.x +
            jointMatrices[inJointIndices.y] * inJointWeights.y +
            jointMatrices[inJointIndices.z] * inJointWeights.z +
            jointMatrices[inJointIndices.w] * inJointWeights.w;

        localPosition =
            (
                skinningMatrix *
                vec4(inPosition, 1.0)
            ).xyz;
    }

    gl_Position =
        uLightViewProjection *
        uModel *
        vec4(localPosition, 1.0);
}