#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat3 uNormalMatrix;

uniform int uOutlineWidthMode;
uniform float uOutlineWidth;
uniform vec2 uViewportSize;

const int OUTLINE_WIDTH_MODE_WORLD = 0;
const float EPSILON = 1.0e-8;

void main()
{
    const vec3 worldPosition =
        (uModel * vec4(inPosition, 1.0)).xyz;

    const vec3 worldNormal =
        normalize(uNormalMatrix * inNormal);

    const float outlineWidth =
        max(uOutlineWidth, 0.0);

    if (uOutlineWidthMode == OUTLINE_WIDTH_MODE_WORLD)
    {
        const vec3 expandedWorldPosition =
            worldPosition + worldNormal * outlineWidth;

        gl_Position = uViewProjection * vec4(expandedWorldPosition, 1.0);
    }
    else
    {
        vec4 clipPosition =
            uViewProjection * vec4(worldPosition, 1.0);

        const vec4 normalClipPosition =
            uViewProjection *
            vec4(worldPosition + worldNormal, 1.0);

        const vec2 viewportSize =
            max(uViewportSize, vec2(1.0));

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
                (normalPositionNdc - positionNdc) *
                viewportSize *
                0.5;

            const float directionLengthSquared =
                dot(projectedDirectionPixels,
                    projectedDirectionPixels);

            if (directionLengthSquared > EPSILON)
            {
                const vec2 screenDirection =
                    projectedDirectionPixels *
                    inversesqrt(directionLengthSquared);

                const vec2 pixelOffset = screenDirection * outlineWidth;

                const vec2 ndcOffset =
                    pixelOffset * 2.0 / viewportSize;

                clipPosition.xy += ndcOffset * clipPosition.w;
            }
        }

        gl_Position = clipPosition;
    }
}
