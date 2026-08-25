#version 450 core

layout(location = 0) in vec2 vertexTextureCoordinate;

layout(location = 0) out vec4 outColor;

uniform sampler2D uDepth;
uniform sampler2D uNormal;
uniform sampler2D uMaterialId;

uniform int uScreenOutlineEnabled;

uniform float uNearPlane;
uniform float uFarPlane;

struct ScreenOutlinePolicy
{
    vec4 colorAndWidth;
    vec4 thresholds;
    uvec4 metadata;
};

layout(std430, binding = 1)
readonly buffer ScreenOutlinePolicyBuffer
{
    ScreenOutlinePolicy policies[];
};

const uint POLICY_ENABLED = 1U << 0U;
const uint POLICY_DEPTH_ENABLED = 1U << 1U;
const uint POLICY_NORMAL_ENABLED = 1U << 2U;
const uint POLICY_DETECT_SELF_DEPTH = 1U << 3U;
const uint POLICY_DETECT_SELF_NORMAL = 1U << 4U;

vec4 sampleMaterialId(const vec2 textureCoordinate)
{
    const ivec2 extent =
        textureSize(uMaterialId, 0);

    const ivec2 coordinate =
        clamp(
            ivec2(
                textureCoordinate *
                vec2(extent)),
            ivec2(0),
            extent - ivec2(1));

    // Material IDs are categorical data, not colors. texelFetch prevents
    // linear filtering from blending two neighboring identities.
    return texelFetch(
        uMaterialId,
        coordinate,
        0);
}

uint decodePolicyIndex(const vec4 value)
{
    const uvec3 bytes =
        uvec3(round(value.rgb * 255.0));

    return bytes.x |
        (bytes.y << 8U) |
        (bytes.z << 16U);
}

bool policyHasFlag(
    const ScreenOutlinePolicy policy,
    const uint flag)
{
    return (policy.metadata.y & flag) != 0U;
}

bool materialIdComesFirst(
    const vec4 left,
    const vec4 right)
{
    const ivec3 leftId =
        ivec3(round(left.rgb * 255.0));

    const ivec3 rightId =
        ivec3(round(right.rgb * 255.0));

    if (leftId.z != rightId.z)
    {
        return leftId.z < rightId.z;
    }

    if (leftId.y != rightId.y)
    {
        return leftId.y < rightId.y;
    }

    return leftId.x < rightId.x;
}

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

bool ownsBoundary(
    const float centerDepth,
    const float neighborDepth,
    const vec4 centerMaterialId,
    const vec4 neighborMaterialId)
{
    // A foreground pixel exclusively owns an object/background silhouette.
    if (neighborMaterialId.a <= 1.0e-4)
    {
        return true;
    }

    const float depthEpsilon =
        max(
            min(centerDepth, neighborDepth) * 1.0e-4,
            1.0e-5);

    // At an overlap, only the nearer surface writes the outline. This turns
    // the former two-sided edge pair into one stable screen-space pixel.
    if (centerDepth < neighborDepth - depthEpsilon)
    {
        return true;
    }

    if (centerDepth > neighborDepth + depthEpsilon)
    {
        return false;
    }

    // Coplanar material boundaries have no depth owner. Pick a deterministic
    // group-ID side so their width does not double or switch with the camera.
    return materialIdComesFirst(
        centerMaterialId,
        neighborMaterialId);
}

float calculateDepthEdge(const vec2 textureCoordinate)
{
    const float centerDepthSample =
        texture(uDepth, textureCoordinate).r;

    if (centerDepthSample >= 1.0 - 1.0e-6)
    {
        return 0.0;
    }

    const float centerDepth =
        linearizeDepth(centerDepthSample);

    const vec4 centerMaterialId =
        sampleMaterialId(textureCoordinate);

    const uint centerPolicyIndex =
        decodePolicyIndex(centerMaterialId);

    if (centerPolicyIndex == 0U)
    {
        return 0.0;
    }

    const ScreenOutlinePolicy centerPolicy =
        policies[centerPolicyIndex];

    if (!policyHasFlag(centerPolicy, POLICY_ENABLED) ||
        !policyHasFlag(centerPolicy, POLICY_DEPTH_ENABLED))
    {
        return 0.0;
    }

    const vec2 texelSize =
        1.0 /
        vec2(textureSize(uDepth, 0));

    const float sampleWidth =
        max(centerPolicy.colorAndWidth.w, 1.0);

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

            const vec4 neighborMaterialId =
                sampleMaterialId(sampleCoordinate);

            const uint neighborPolicyIndex =
                decodePolicyIndex(neighborMaterialId);

            if (neighborPolicyIndex == centerPolicyIndex &&
                !policyHasFlag(
                    centerPolicy,
                    POLICY_DETECT_SELF_DEPTH))
            {
                // This material disables outlines caused by its own depth
                // discontinuities. Object/background silhouettes still pass
                // because the background has no valid material identity.
                continue;
            }

            const float neighborDepthSample =
                texture(uDepth, sampleCoordinate).r;

            const float neighborDepth =
                linearizeDepth(
                    neighborDepthSample
                );

            if (!ownsBoundary(
                    centerDepth,
                    neighborDepth,
                    centerMaterialId,
                    neighborMaterialId))
            {
                continue;
            }

            const float relativeDifference =
                abs(neighborDepth - centerDepth) /
                max(centerDepth, 1.0e-4);

            maximumDifference =
                max(maximumDifference, relativeDifference);
        }
    }

    const float threshold =
        max(centerPolicy.thresholds.x, 1.0e-6);

    return smoothstep(
        threshold,
        threshold * 2.0,
        maximumDifference);
}

float calculateNormalEdge(const vec2 textureCoordinate)
{
    const vec4 centerSample =
        texture(uNormal, textureCoordinate);

    if (centerSample.a <= 1.0e-4)
    {
        return 0.0;
    }

    const vec3 centerNormal =
        normalize(centerSample.rgb * 2.0 - 1.0);

    const float centerDepth =
        linearizeDepth(
            texture(uDepth, textureCoordinate).r);

    const vec4 centerMaterialId =
        sampleMaterialId(textureCoordinate);

    const uint centerPolicyIndex =
        decodePolicyIndex(centerMaterialId);

    if (centerPolicyIndex == 0U)
    {
        return 0.0;
    }

    const ScreenOutlinePolicy centerPolicy =
        policies[centerPolicyIndex];

    if (!policyHasFlag(centerPolicy, POLICY_ENABLED) ||
        !policyHasFlag(centerPolicy, POLICY_NORMAL_ENABLED))
    {
        return 0.0;
    }

    const vec2 texelSize =
        1.0 /
        vec2(textureSize(uNormal, 0));

    const float sampleWidth =
        max(centerPolicy.colorAndWidth.w, 1.0);

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

            const vec4 neighborMaterialId =
                sampleMaterialId(sampleCoordinate);

            const uint neighborPolicyIndex =
                decodePolicyIndex(neighborMaterialId);

            if (neighborPolicyIndex == centerPolicyIndex)
            {
                if (!policyHasFlag(
                        centerPolicy,
                        POLICY_DETECT_SELF_NORMAL))
                {
                    continue;
                }
            }
            else if (neighborPolicyIndex != 0U &&
                     centerPolicy.metadata.x ==
                         policies[neighborPolicyIndex].metadata.x)
            {
                // Different materials in one explicit group retain their
                // depth edge above, but suppress normal-only discontinuities.
                continue;
            }

            const float neighborDepth =
                linearizeDepth(
                    texture(uDepth, sampleCoordinate).r);

            if (!ownsBoundary(
                    centerDepth,
                    neighborDepth,
                    centerMaterialId,
                    neighborMaterialId))
            {
                continue;
            }

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
        max(centerPolicy.thresholds.y, 1.0e-6);

    return smoothstep(
        threshold,
        threshold * 2.0,
        maximumDifference);
}

void main()
{
    float depthCoverage = 0.0;
    float normalCoverage = 0.0;

    if (uScreenOutlineEnabled != 0)
    {
        depthCoverage =
            calculateDepthEdge(vertexTextureCoordinate);
        normalCoverage =
            calculateNormalEdge(vertexTextureCoordinate);
    }

    const float coverage =
        max(depthCoverage, normalCoverage);

    outColor = vec4(
        coverage,
        depthCoverage,
        normalCoverage,
        1.0);
}
