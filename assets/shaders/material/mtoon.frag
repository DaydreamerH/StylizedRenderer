#version 450 core

layout(location = 0) in vec3 vertexNormal;
layout(location = 1) in vec2 vertexTexCoord0;
layout(location = 2) in vec3 vertexWorldPosition;
layout(location = 3) in vec4 vertexWorldTangent;

layout(location = 0) out vec4 outColor;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;

uniform vec3 uShadeColor;
uniform sampler2D uShadeTexture;
uniform sampler2D uShadingShiftTexture;
uniform float uShadingShiftTextureScale;

uniform float uShadingShift;
uniform float uShadingToony;

uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform float uLightIntensity;

uniform vec3 uEnvironmentSkyColor;
uniform vec3 uEnvironmentGroundColor;
uniform float uEnvironmentIntensity;

uniform float uGiEqualization;

uniform sampler2D uShadowMap;
uniform mat4 uLightViewProjection;
uniform int uShadowEnabled;

uniform sampler2D uNormalTexture;
uniform float uNormalScale;

uniform mat4 uView;

uniform sampler2D uMatcapTexture;
uniform vec3 uMatcapColor;
uniform float uMatcapStrength;

vec3 calculateSurfaceNormal()
{
    const float faceSign =
        gl_FrontFacing
            ? 1.0
            : -1.0;

    const vec3 geometricNormal =
        normalize(vertexNormal) * faceSign;

    const vec3 orthogonalTangent =
        vertexWorldTangent.xyz -
        geometricNormal *
        dot(
            geometricNormal,
            vertexWorldTangent.xyz);

    const float tangentLengthSquared =
        dot(
            orthogonalTangent,
            orthogonalTangent);

    if (tangentLengthSquared <= 1.0e-8)
    {
        return geometricNormal;
    }

    const vec3 tangent =
        orthogonalTangent *
        inversesqrt(
            tangentLengthSquared);

    const vec3 bitangent =
        cross(
            geometricNormal,
            tangent) *
        vertexWorldTangent.w;

    vec3 tangentSpaceNormal =
        texture(uNormalTexture, vertexTexCoord0).xyz * 2.0 - 1.0;

    tangentSpaceNormal.xy *=
        max(
            uNormalScale,
            0.0);

    const float sampledLengthSquared =
        dot(
            tangentSpaceNormal,
            tangentSpaceNormal);

    if (sampledLengthSquared <= 1.0e-8)
    {
        return geometricNormal;
    }

    tangentSpaceNormal *=
        inversesqrt(
            sampledLengthSquared);

    const mat3 tangentFrame =
        mat3(
            tangent,
            bitangent,
            geometricNormal);

    return normalize(
        tangentFrame *
        tangentSpaceNormal);
}

float calculateShadowVisibility(
    const vec3 worldPosition,
    const vec3 normal,
    const vec3 lightDirection
)
{
    if (uShadowEnabled == 0)
        return 1.0;

    const vec4 lightClipPosition =
        uLightViewProjection *
            vec4(worldPosition, 1.0);

    const vec3 lightNdcPosition =
        lightClipPosition.xyz /
        lightClipPosition.w;

    const vec3 shadowCoordinate =
        lightNdcPosition * 0.5 + 0.5;

    if (any(lessThan(
            shadowCoordinate,
            vec3(0.0))) ||
        any(greaterThan(
            shadowCoordinate,
            vec3(1.0))))
    {
        return 1.0;
    }

    const float normalDotLight =
        max(
            dot(normal, lightDirection),
            0.0);

    const float bias =
        max(
            0.0025 *
                (1.0 - normalDotLight),
            0.0005);

    const vec2 texelSize =
        1.0 /
        vec2(textureSize(
            uShadowMap,
            0));

    const float currentDepth =
        shadowCoordinate.z - bias;

    float visibility = 0.0;

    for (int offsetY = -1;
         offsetY <= 1;
         ++offsetY)
    {
        for (int offsetX = -1;
             offsetX <= 1;
             ++offsetX)
        {
            const vec2 sampleCoordinate =
                shadowCoordinate.xy +
                vec2(
                    float(offsetX),
                    float(offsetY)) *
                texelSize;

            const float storedDepth =
                texture(
                    uShadowMap,
                    sampleCoordinate).r;

            visibility +=
                currentDepth <= storedDepth
                    ? 1.0
                    : 0.0;
        }
    }

    return visibility / 9.0;
}

void main()
{
    const vec4 sampledBaseColor =
        texture(uBaseColorTexture, vertexTexCoord0);

    const vec4 baseColor =
        uBaseColorFactor * sampledBaseColor;

    const vec3 normal =
        calculateSurfaceNormal();

    const vec3 lightDirection =
        normalize(-uLightDirection);

    const float normalDotLight =
        dot(normal, lightDirection);

    const float shadingToony =
        clamp(uShadingToony, 0.0, 1.0);

    const float transitionWidth =
        max(1.0 - shadingToony, 0.0001);

    const float sampledShadingShift =
        texture(
            uShadingShiftTexture,
            vertexTexCoord0).r;

    const float finalShadingShift =
        uShadingShift +
        sampledShadingShift *
        uShadingShiftTextureScale;

    const float shadingFactor =
        clamp(
            (normalDotLight + finalShadingShift) / transitionWidth,
            0.0,
            1.0);

    const vec3 lightRadiance =
        uLightColor * max(uLightIntensity, 0.0);

    const vec3 litColor =
        baseColor.rgb * lightRadiance;

    const vec3 sampledShadeColor =
        texture(uShadeTexture, vertexTexCoord0).rgb;

    const vec3 shadeSurfaceColor =
        uShadeColor * sampledShadeColor;

    const vec3 shadeColor =
        shadeSurfaceColor * lightRadiance;
    
    const float shadowVisibility =
        calculateShadowVisibility(
            vertexWorldPosition,
            normal,
            lightDirection);

    const float visibleShadingFactor =
        shadingFactor *
        shadowVisibility;

    const vec3 directColor =
        mix(shadeColor, litColor, visibleShadingFactor);

    const float hemisphereWeight =
        clamp(
            normal.y * 0.5 + 0.5,
            0.0,
            1.0
        );

    const vec3 directionalEnvironment =
        mix(
            uEnvironmentGroundColor,
            uEnvironmentSkyColor,
            hemisphereWeight
        );

    const vec3 uniformEnvironment =
        (uEnvironmentSkyColor + uEnvironmentGroundColor) * 0.5;

    const vec3 environmentRadiance =
        mix(
            directionalEnvironment,
            uniformEnvironment,
            clamp(
                uGiEqualization,
                0.0,
                1.0)
            ) * max(uEnvironmentIntensity, 0.0);

    const vec3 indirectColor =
        baseColor.rgb * environmentRadiance;

    const vec3 viewNormal =
        normalize(mat3(uView) * normal);

    const vec2 matcapUv =
        viewNormal.xy * 0.5 + 0.5;

    const vec3 sampledMatcap =
        texture(uMatcapTexture, matcapUv).rgb;

    const vec3 matcapContribution =
        sampledMatcap * uMatcapColor * max(uMatcapStrength, 0.0);

    const vec3 finalColor = directColor + indirectColor + matcapContribution;

    outColor = vec4(finalColor, baseColor.a);
}
