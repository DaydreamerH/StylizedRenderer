#version 450 core

layout(location = 0) in vec3 vertexNormal;
layout(location = 1) in vec2 vertexTexCoord0;
layout(location = 2) in vec3 vertexWorldPosition;
layout(location = 3) in vec4 vertexWorldTangent;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

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

uniform sampler2DShadow uShadowMap;
uniform mat4 uLightViewProjection;
uniform int uShadowEnabled;

uniform sampler2D uNormalTexture;
uniform float uNormalScale;

uniform mat4 uView;

uniform sampler2D uMatcapTexture;
uniform vec3 uMatcapColor;
uniform float uMatcapStrength;

uniform vec3 uCameraPosition;

uniform sampler2D uRimMaskTexture;
uniform vec3 uRimColor;
uniform float uRimFresnelPower;
uniform float uRimLift;
uniform float uRimLightingMix;

uniform sampler2D uEmissionTexture;
uniform vec3 uEmissionColor;
uniform float uEmissionStrength;

uniform int uMToonDebugView;

uniform int uAlphaMaskEnabled;
uniform float uAlphaCutoff;

vec3 calculateGeometricNormal()
{
    const float faceSign = gl_FrontFacing
        ? 1.0
        : -1.0;

    return normalize(vertexNormal) * faceSign;
}

vec3 calculateSurfaceNormal(const vec3 geometricNormal)
{
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

    vec2 tangentSpaceNormalXY =
        texture(uNormalTexture, vertexTexCoord0).xy * 2.0 - 1.0;

    tangentSpaceNormalXY *=
        max(
            uNormalScale,
            0.0);

    const float tangentSpaceNormalZ =
        sqrt(
            max(
                1.0 - dot(
                    tangentSpaceNormalXY,
                    tangentSpaceNormalXY),
                0.0));

    vec3 tangentSpaceNormal =
        vec3(
            tangentSpaceNormalXY,
            tangentSpaceNormalZ);

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
    const vec3 geometricNormal,
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
            dot(geometricNormal, lightDirection),
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

    const float weights[3] =
        float[](
            1.0,
            2.0,
            1.0
        );

    for (int offsetY = -1; offsetY <= 1; offsetY++)
    {
        for (int offsetX = -1; offsetX <= 1; offsetX++)
        {
            const vec2 sampleCoordinate =
                shadowCoordinate.xy +
                vec2(float(offsetX), float(offsetY)) *
                texelSize;

            const float sampleWeight =
                weights[offsetX + 1] *
                weights[offsetY + 1];

            visibility +=
                texture(uShadowMap,
                    vec3(
                        sampleCoordinate, currentDepth
                    )) * sampleWeight;
        }
    }

    return visibility / 16.0;
}

void main()
{
    const vec4 sampledBaseColor =
        texture(uBaseColorTexture, vertexTexCoord0);

    const vec4 baseColor =
        uBaseColorFactor *
        sampledBaseColor;

    if (uAlphaMaskEnabled != 0 &&
        baseColor.a < uAlphaCutoff)
    {
        discard;
    }

    const vec3 geometricNormal =
        calculateGeometricNormal();

    const vec3 normal =
        calculateSurfaceNormal(
            geometricNormal);

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
            geometricNormal,
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

    const vec3 toCamera =
        uCameraPosition - vertexWorldPosition;

    const float toCameraLengthSquared =
        dot(toCamera, toCamera);

    const vec3 viewDirection =
        toCameraLengthSquared > 1.0e-8
        ? toCamera * inversesqrt(toCameraLengthSquared)
        : normal;

    const float normalDotView =
        max(dot(normal, viewDirection), 0.0);

    const float rimBase =
        clamp(
            1.0 - normalDotView + uRimLift,
            0.0,
            1.0);

    const float rimFactor =
        pow(rimBase,
            max(uRimFresnelPower, 0.0001));

    const float rimMask =
        texture(uRimMaskTexture, vertexTexCoord0).r;

    const vec3 rimLighting =
        mix(
            vec3(1.0),
            lightRadiance * visibleShadingFactor,
            clamp(
                uRimLightingMix,
                0.0,
                1.0
            ));

    const vec3 rimContribution =
        uRimColor * rimMask * rimFactor * rimLighting;

    const vec3 sampledEmission =
        texture(
            uEmissionTexture,
            vertexTexCoord0).rgb;

    const vec3 emissionContribution =
        sampledEmission *
        uEmissionColor *
        max(
            uEmissionStrength,
            0.0);

    const vec3 finalColor =
        directColor +
        indirectColor +
        matcapContribution +
        rimContribution +
        emissionContribution;

    vec3 outputColor = finalColor;

    switch (uMToonDebugView)
    {
    case 1:
        outputColor = baseColor.rgb;
        break;

    case 2:
        outputColor = shadeSurfaceColor;
        break;

    case 3:
        outputColor = vec3(visibleShadingFactor);
        break;

    case 4:
        outputColor = rimContribution;
        break;

    case 5:
        outputColor = matcapContribution;
        break;

    case 6:
        outputColor = emissionContribution;
        break;

    default:
        break;
    }

    outColor = vec4(outputColor, baseColor.a);
    outNormal = vec4(normal * 0.5 + 0.5, 1.0);
}
