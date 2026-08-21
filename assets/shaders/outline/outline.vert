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

uniform int uOutlineWidthMode;
uniform float uOutlineWidth;
uniform vec2 uViewportSize;

uniform sampler2D uOutlineWidthMask;

const int OUTLINE_WIDTH_MODE_WORLD = 0;
const float EPSILON = 1.0e-8;

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

    if (uOutlineWidthMode ==
        OUTLINE_WIDTH_MODE_WORLD)
    {
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
    else
    {
        vec4 clipPosition =
            uViewProjection *
            vec4(
                worldPosition,
                1.0
            );

        const vec4 normalClipPosition =
            uViewProjection *
            vec4(
                worldPosition +
                    worldNormal,
                1.0
            );

        const vec2 viewportSize =
            max(
                uViewportSize,
                vec2(1.0)
            );

        if (abs(clipPosition.w) > EPSILON &&
            abs(normalClipPosition.w) > EPSILON)
        {
            const vec2 positionNdc =
                clipPosition.xy /
                clipPosition.w;

            const vec2 normalPositionNdc =
                normalClipPosition.xy /
                normalClipPosition.w;

            const vec2 projectedDirectionPixels =
                (
                    normalPositionNdc -
                    positionNdc
                ) *
                viewportSize *
                0.5;

            const float directionLengthSquared =
                dot(
                    projectedDirectionPixels,
                    projectedDirectionPixels
                );

            if (directionLengthSquared > EPSILON)
            {
                const vec2 screenDirection =
                    projectedDirectionPixels *
                    inversesqrt(
                        directionLengthSquared
                    );

                const vec2 pixelOffset =
                    screenDirection *
                    outlineWidth;

                const vec2 ndcOffset =
                    pixelOffset *
                    2.0 /
                    viewportSize;

                clipPosition.xy +=
                    ndcOffset *
                    clipPosition.w;
            }
        }

        gl_Position = clipPosition;
    }
}
