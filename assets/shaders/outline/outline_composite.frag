#version 450 core

layout(location = 0) in vec2 vertexTextureCoordinate;

layout(location = 0) out vec4 outColor;

uniform sampler2D uHdrColor;
uniform sampler2D uOutlineMask;
uniform sampler2D uScreenEdgeMask;
uniform sampler2D uDepth;
uniform int uDebugView;

uniform vec3 uScreenOutlineColor;

uniform float uNearPlane;
uniform float uFarPlane;

uniform sampler2D uNormal;

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

float screenEdgeCoverage()
{
    const ivec2 extent =
        textureSize(uScreenEdgeMask, 0);

    const ivec2 centerCoordinate =
        clamp(
            ivec2(gl_FragCoord.xy),
            ivec2(0),
            extent - ivec2(1));

    const float centerCoverage =
        texelFetch(
            uScreenEdgeMask,
            centerCoordinate,
            0).r;

    // Keep the raw edge signal continuous. A binary neighbor count turns a
    // barely supported line into a visibly hard cutoff as the camera moves.
    // The two confidence windows below instead fade fragments into the scene
    // color through the final compositing mix.
    float neighborCoverage = 0.0;
    float directionalCoverage = 0.0;

    const int offsets[8][2] = int[8][2](
        int[2](-1, -1),
        int[2]( 0, -1),
        int[2]( 1, -1),
        int[2](-1,  0),
        int[2]( 1,  0),
        int[2](-1,  1),
        int[2]( 0,  1),
        int[2]( 1,  1));

    float samples[8];
    for (int index = 0; index < 8; ++index)
    {
        const ivec2 coordinate =
            clamp(
                centerCoordinate +
                    ivec2(
                        offsets[index][0],
                        offsets[index][1]),
                ivec2(0),
                extent - ivec2(1));

        samples[index] = texelFetch(
            uScreenEdgeMask,
            coordinate,
            0).r;

        neighborCoverage += samples[index];
    }

    directionalCoverage = max(
        directionalCoverage,
        min(samples[3], samples[4]));
    directionalCoverage = max(
        directionalCoverage,
        min(samples[1], samples[6]));
    directionalCoverage = max(
        directionalCoverage,
        min(samples[0], samples[7]));
    directionalCoverage = max(
        directionalCoverage,
        min(samples[2], samples[5]));

    const float hasDirectionalSupport =
        smoothstep(
            0.12,
            0.45,
            directionalCoverage);

    const float hasCurvedSupport =
        smoothstep(
            0.75,
            1.75,
            neighborCoverage);

    const float supportCoverage =
        max(hasDirectionalSupport, hasCurvedSupport);

    return centerCoverage * supportCoverage;
}

void main()
{
    const float screenCoverage =
        screenEdgeCoverage();

    const vec4 hdrColor =
        texture(uHdrColor, vertexTextureCoordinate);

    const vec4 outlineMask =
        texture(uOutlineMask, vertexTextureCoordinate);

    const float shellCoverage =
        clamp(outlineMask.a, 0.0, 1.0);

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
