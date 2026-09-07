#version 450 core

layout(location = 0) in vec3 vertexNormal;
layout(location = 1) in vec2 vertexTexCoord0;
layout(location = 2) in vec3 vertexWorldPosition;
layout(location = 3) in vec4 vertexWorldTangent;
layout(location = 4) in vec3 vertexSphereNormal;
layout(location = 5) in float vertexSphereWeight;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterialId;

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
uniform float uShadowNormalInfluence;
uniform int uShadowCutoffEnabled;
uniform float uShadowCutoff;

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
uniform vec3 uOutlineMaterialId;

uniform sampler2D uToonRampTexture;

uniform sampler2D uOcclusionTexture;
uniform float uOcclusionStrength;

uniform sampler2D uSpecularTexture;
uniform vec3 uSpecularColor;
uniform float uSpecularStrength;
uniform float uSpecularPower;

uniform sampler2D uFaceSdfTexture;
uniform int uFaceSdfEnabled;
uniform int uFaceSdfFlipHorizontal;
uniform float uFaceSdfOffset;
uniform float uFaceSdfSoftness;
uniform float uFaceSdfStrength;
uniform vec3 uFaceForward;
uniform vec3 uFaceRight;
uniform vec3 uFaceUp;

uniform sampler2D uFaceHairShadowMask;
uniform int uFaceHairShadowEnabled;
uniform mat4 uFaceHairShadowViewProjection;
uniform vec2 uFaceHairShadowUvOffset;
uniform float uFaceHairShadowSoftness;
uniform float uFaceHairShadowStrength;

vec3 calculateGeometricNormal()
{
    const float faceSign = gl_FrontFacing
        ? 1.0
        : -1.0;

    return normalize(vertexNormal) * faceSign;
}

float calculateFaceHairShadowVisibility(
    const vec3 worldPosition)
{
    if (uFaceHairShadowEnabled == 0)
    {
        return 1.0;
    }

    const vec4 clipPosition =
        uFaceHairShadowViewProjection *
        vec4(worldPosition, 1.0);

    if (abs(clipPosition.w) <= 1.0e-8)
    {
        return 1.0;
    }

    const vec3 ndc = clipPosition.xyz / clipPosition.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;

    uv += uFaceHairShadowUvOffset;

    if (any(lessThan(uv, vec2(0.0))) ||
        any(greaterThan(uv, vec2(1.0))))
    {
        return 1.0;
    }

    const vec2 texel =
        1.0 / vec2(textureSize(uFaceHairShadowMask, 0));

    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            visibility += texture(
                uFaceHairShadowMask,
                uv + vec2(x, y) * texel).r;
        }
    }
    visibility /= 9.0;

    const float softness = max(uFaceHairShadowSoftness, 0.0);
    visibility = smoothstep(
        0.5 - softness,
        0.5 + softness,
        visibility);

    return mix(
        1.0,
        visibility,
        clamp(uFaceHairShadowStrength, 0.0, 1.0));
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
    const vec3 surfaceNormal,
    const vec3 lightDirection
)
{
    if (uShadowEnabled == 0)
    {
        return 1.0;
    }

    const vec3 detailNormal =
        surfaceNormal - geometricNormal;

    const float shadowNormalInfluence =
        clamp(
            uShadowNormalInfluence,
            0.0,
            1.0);

    const float shadowNormalOffsetStrength =
        0.008 * shadowNormalInfluence;

    const vec3 perturbedWorldPosition =
        worldPosition +
        detailNormal * shadowNormalOffsetStrength;

    const vec4 lightClipPosition =
        uLightViewProjection *
            vec4(perturbedWorldPosition, 1.0);

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
            dot(surfaceNormal, lightDirection),
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

    const float weights[5] =
        float[](
            1.0,
            2.0,
            3.0,
            2.0,
            1.0
        );

    // A 5x5 separable tent filter hides individual shadow-map texels while
    // retaining a noticeably crisper transition than a broad box filter.
    for (int offsetY = -2; offsetY <= 2; offsetY++)
    {
        for (int offsetX = -2; offsetX <= 2; offsetX++)
        {
            const vec2 sampleCoordinate =
                shadowCoordinate.xy +
                vec2(float(offsetX), float(offsetY)) *
                texelSize;

            const float sampleWeight =
                weights[offsetX + 2] *
                weights[offsetY + 2];

            visibility +=
                texture(
                    uShadowMap,
                    vec3(
                        sampleCoordinate,
                        currentDepth)) *
                sampleWeight;
        }
    }

    return visibility / 81.0;
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

    // Keep the physical surface normal for shadows and secondary lighting.
    // The sphere only stabilizes the MToon direct-light threshold on the face.
    const vec3 toonLightingNormal =
        normalize(
            mix(
                normal,
                vertexSphereNormal,
                clamp(
                    vertexSphereWeight,
                    0.0,
                    1.0)));

    const vec3 lightDirection =
        normalize(-uLightDirection);

    const float normalDotLight =
        dot(toonLightingNormal, lightDirection);

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

    const float threshold = -finalShadingShift;

    const float halfWidth = transitionWidth * 0.5;

    const float delta = fwidth(normalDotLight);

    const float aaWidth = max(halfWidth, delta);

    const float normalShadingFactor = smoothstep(
        threshold - aaWidth,
        threshold + aaWidth,
        normalDotLight);

    float shadingFactor = normalShadingFactor;

    if (uFaceSdfEnabled != 0)
    {
        const vec3 faceForward = normalize(uFaceForward);
        const vec3 faceRight = normalize(uFaceRight);
        const vec3 faceUp = normalize(uFaceUp);

        const vec3 projectedLight =
            lightDirection -
            faceUp * dot(lightDirection, faceUp);

        const float projectedLengthSquared =
            dot(projectedLight, projectedLight);

        float normalizedAngle = 0.0;
        float lightSide = 0.0;

        if (projectedLengthSquared > 1.0e-8)
        {
            const vec3 horizontalLight =
                projectedLight *
                inversesqrt(projectedLengthSquared);

            const float lightFront = clamp(
                dot(horizontalLight, faceForward),
                -1.0,
                1.0);

            lightSide = dot(horizontalLight, faceRight);
            normalizedAngle =
                atan(abs(lightSide), lightFront) /
                3.141592653589793;
        }

        vec2 sdfUv = vertexTexCoord0;
        const bool mirrorForLightSide = lightSide < 0.0;
        const bool configuredFlip =
            uFaceSdfFlipHorizontal != 0;

        if (mirrorForLightSide != configuredFlip)
        {
            sdfUv.x = 1.0 - sdfUv.x;
        }

        const vec2 sdfTexelSize =
            0.5 /
            vec2(textureSize(uFaceSdfTexture, 0));

        sdfUv = clamp(
            sdfUv,
            sdfTexelSize,
            vec2(1.0) - sdfTexelSize);

        const float sdfThreshold =
            texture(uFaceSdfTexture, sdfUv).r;

        const float signedThreshold =
            sdfThreshold +
            uFaceSdfOffset -
            normalizedAngle;

        const float sdfWidth = max(
            max(uFaceSdfSoftness, 0.0),
            max(
                0.5 * fwidth(signedThreshold),
                1.0e-5));

        const float sdfVisibility = smoothstep(
            -sdfWidth,
            sdfWidth,
            signedThreshold);

        shadingFactor = mix(
            normalShadingFactor,
            sdfVisibility,
            clamp(uFaceSdfStrength, 0.0, 1.0));
    }

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
            normal,
            lightDirection);

    const float normalDetail =
        dot(normal, lightDirection) -
        dot(geometricNormal, lightDirection);

    const float shadowNormalDetailStrength =
        0.15 *
        clamp(
            uShadowNormalInfluence,
            0.0,
            1.0);

    const float perturbedShadowVisibility =
        shadowVisibility +
        normalDetail * shadowNormalDetailStrength;

    float shadowMask =
        clamp(
            perturbedShadowVisibility,
            0.0,
            1.0);

    if (uShadowCutoffEnabled != 0)
    {
        const float shadowCutoff =
            clamp(
                uShadowCutoff,
                0.0,
                1.0);

        const float shadowAntiAliasWidth =
            max(
                fwidth(perturbedShadowVisibility),
                0.015);

        shadowMask =
            smoothstep(
                shadowCutoff - shadowAntiAliasWidth,
                shadowCutoff + shadowAntiAliasWidth,
                perturbedShadowVisibility);
    }

    const float faceHairShadowVisibility =
        calculateFaceHairShadowVisibility(
            vertexWorldPosition);

    // SDF and HairA both drive the same MToon self-shading factor so their
    // shadow regions use one shade color and one toon-ramp response.
    const float faceShadingFactor = min(
        shadingFactor,
        faceHairShadowVisibility);

    const float toonRampCoordinate =
        1.0 - clamp(
            faceShadingFactor,
            0.0,
            1.0);

    const vec3 sampledToonRamp =
        texture(
            uToonRampTexture,
            vec2(0.5, toonRampCoordinate)
        ).rgb;

    const vec3 toonColor =
        mix(
            shadeColor,
            litColor,
            faceShadingFactor) *
        sampledToonRamp;

    const vec3 castShadowRamp =
        texture(
            uToonRampTexture,
            vec2(0.5, 1.0)).rgb;

    const vec3 castShadowColor =
        shadeColor * castShadowRamp;

    const vec3 directColor =
        mix(
            castShadowColor,
            toonColor,
            shadowMask);

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

    const float sampledOcclusion =
        texture(
            uOcclusionTexture,
            vertexTexCoord0
        ).b;

    const float occlusion =
        mix(
            1.0,
            sampledOcclusion,
            clamp(uOcclusionStrength, 0.0, 1.0)
        );

    const vec3 indirectColor =
        baseColor.rgb * environmentRadiance * occlusion;

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

    const vec3 halfDirection =
        normalize(lightDirection + viewDirection);

    const float normalDotHalf =
        max(dot(normal, halfDirection), 0.0);

    const float specularLobe =
        pow(
            normalDotHalf,
            max(uSpecularPower, 1.0)
        );

    const vec3 sampledSpecular =
        texture(
            uSpecularTexture,
            vertexTexCoord0
        ).rgb;

    const float specularVisibility =
        step(0.0, normalDotLight) *
        shadowVisibility;

    const vec3 specularContribution =
        sampledSpecular *
        uSpecularColor *
        lightRadiance *
        max(uSpecularStrength, 0.0) *
        specularLobe *
        specularVisibility;

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
            lightRadiance * shadowMask,
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
        emissionContribution +
        specularContribution;

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
        outputColor = vec3(shadowMask);
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
    // Screen-space outlines need stable shape information. The MToon surface
    // normal includes normal-map detail, which turns fabric and hair texture
    // into isolated, view-dependent edge pixels. Keep it for lighting above,
    // but store only the interpolated geometric normal in the G-buffer.
    outNormal = vec4(geometricNormal * 0.5 + 0.5, 1.0);
    outMaterialId = vec4(uOutlineMaterialId, 1.0);
}
