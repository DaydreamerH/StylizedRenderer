#version 450 core

layout(location = 0) in vec2 vertexTextureCoordinate;

layout(location = 0) out vec4 outColor;

uniform sampler2D uHdrColor;
uniform sampler2D uOutlineMask;
uniform sampler2D uDepth;
uniform int uDebugView;

uniform int uScreenOutlineEnabled;
uniform vec3 uScreenOutlineColor;
uniform float uScreenOutlineWidth;
uniform float uDepthThreshold;

uniform float uNearPlane;
uniform float uFarPlane;

uniform sampler2D uNormal;
uniform float uNormalThreshold;

const int DEBUG_VIEW_FINAL = 0;
const int DEBUG_VIEW_SURFACE_NORMAL = 1;
const int DEBUG_VIEW_LINEAR_DEPTH = 2;
const int DEBUG_VIEW_SHELL_OUTLINE_MASK = 3;
const int DEBUG_VIEW_SCREEN_EDGE = 4;
const int DEBUG_VIEW_COMBINED_OUTLINE = 5;

float linearizeDepth(const float depth)
{
    const float ndcDepth =
        depth * 2.0 - 1.0;

    const float numerator =
        2.0 *
        uNearPlane *
        uFarPlane;

    const float denominator =
        uFarPlane +
        uNearPlane -
        ndcDepth *
        (uFarPlane - uNearPlane);

    return numerator / max(denominator, 1.0e-6);
}

float calculateDepthEdge(const vec2 textureCoordinate)
{
    if (uScreenOutlineEnabled == 0)
    {
        return 0.0;
    }

    const float centerDepthSample =
        texture(uDepth, textureCoordinate).r;

    if (centerDepthSample >= 1.0 - 1.0e-6)
    {
        return 0.0;
    }

    const float centerDepth =
        linearizeDepth(centerDepthSample);

    const vec2 texelSize =
        1.0 /
        vec2(textureSize(uDepth, 0));

    const float sampleWidth =
        max(uScreenOutlineWidth, 1.0);

    float maximumDifference = 0.0;

    for (int offsetY = -1; offsetY <= 1; ++offsetY)
    {
        for (int offsetX = -1; offsetX <= 1; ++offsetX)
        {
            if (offsetX == 0 && offsetY == 0)
            {
                continue;
            }

            const vec2 sampleCoordinate =
                textureCoordinate +
                vec2(float(offsetX), float(offsetY)) *
                texelSize * sampleWidth;

            const float neighborDepth =
                linearizeDepth(
                    texture(uDepth, sampleCoordinate).r
                );

            const float relativeDifference =
                abs(neighborDepth - centerDepth) /
                max(centerDepth, 1.0e-4);

            maximumDifference =
                max(maximumDifference, relativeDifference);
        }
    }

    const float threshold =
        max(uDepthThreshold, 1.0e-6);

    return smoothstep(threshold, threshold * 2.0, maximumDifference);
}

float calculateNormalEdge(const vec2 textureCoordinate)
{
    if (uScreenOutlineEnabled == 0)
    {
        return 0.0;
    }

    const vec4 centerSample =
        texture(uNormal, textureCoordinate);

    if (centerSample.a <= 1.0e-4)
    {
        return 0.0;
    }

    const vec3 centerNormal =
        normalize(centerSample.rgb * 2.0 - 1.0);

    const vec2 texelSize =
        1.0 /
        vec2(textureSize(uNormal, 0));

    const float sampleWidth =
        max(uScreenOutlineWidth, 1.0);

    float maximumDifference = 0.0;

    for (int offsetY = -1; offsetY <= 1; ++offsetY)
    {
        for (int offsetX = -1; offsetX <= 1; ++offsetX)
        {
            if (offsetX == 0 && offsetY == 0)
            {
                continue;
            }

            const vec2 sampleCoordinate =
                textureCoordinate +
                vec2(float(offsetX), float(offsetY)) *
                texelSize *
                sampleWidth;

            const vec4 neighborSample =
                texture(uNormal, sampleCoordinate);

            if (neighborSample.a <= 1.0e-4)
            {
                continue;
            }

            const vec3 neighborNormal =
                normalize(neighborSample.rgb * 2.0 - 1.0);

            const float difference =
                1.0 -
                clamp(
                    dot(centerNormal, neighborNormal),
                    -1.0,
                    1.0);

            maximumDifference =
                max(maximumDifference, difference);
        }
    }

    const float threshold =
        max(uNormalThreshold, 1.0e-6);

    return smoothstep(
        threshold,
        threshold * 2.0,
        maximumDifference);
}

void main()
{
    const vec4 hdrColor =
        texture(uHdrColor, vertexTextureCoordinate);

    const vec4 outlineMask =
        texture(uOutlineMask, vertexTextureCoordinate);

    const float shellCoverage =
        clamp(outlineMask.a, 0.0, 1.0);

    const float depthEdge =
        calculateDepthEdge(vertexTextureCoordinate);

    const float normalEdge =
        calculateNormalEdge(vertexTextureCoordinate);

    const float screenCoverage =
        max(depthEdge, normalEdge);

    const float combinedCoverage =
        max(shellCoverage, screenCoverage);

    if (uDebugView == DEBUG_VIEW_SURFACE_NORMAL)
    {
        const vec4 normalSample =
            texture(uNormal, vertexTextureCoordinate);

        outColor = vec4(
            normalSample.a > 1.0e-4
                ? normalSample.rgb
                : vec3(0.0),
            1.0);

        return;
    }

    if (uDebugView == DEBUG_VIEW_LINEAR_DEPTH)
    {
        const float depthSample =
            texture(uDepth, vertexTextureCoordinate).r;

        const float linearDepth =
            depthSample >= 1.0 - 1.0e-6
                ? uFarPlane
                : linearizeDepth(depthSample);

        const float normalizedDepth =
            clamp(
                (linearDepth - uNearPlane) /
                    max(uFarPlane - uNearPlane, 1.0e-6),
                0.0,
                1.0);

        outColor =
            vec4(vec3(normalizedDepth), 1.0);

        return;
    }

    if (uDebugView == DEBUG_VIEW_SHELL_OUTLINE_MASK)
    {
        outColor =
            vec4(vec3(shellCoverage), 1.0);

        return;
    }

    if (uDebugView == DEBUG_VIEW_SCREEN_EDGE)
    {
        outColor =
            vec4(vec3(screenCoverage), 1.0);

        return;
    }

    if (uDebugView == DEBUG_VIEW_COMBINED_OUTLINE)
    {
        outColor =
            vec4(vec3(combinedCoverage), 1.0);

        return;
    }

    const vec3 outlineColor =
        shellCoverage > 1.0e-4
        ? outlineMask.rgb
        : uScreenOutlineColor;

    const vec3 compositedColor =
        mix(
            hdrColor.rgb,
            outlineColor,
            combinedCoverage
        );

    outColor =
        vec4(compositedColor, hdrColor.a);
}
