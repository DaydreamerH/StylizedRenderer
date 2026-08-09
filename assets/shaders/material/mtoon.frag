#version 450 core

layout(location = 0) in vec3 vertexNormal;
layout(location = 1) in vec2 vertexTexCoord0;
layout(location = 2) in vec3 vertexWorldPosition;

layout(location = 0) out vec4 outColor;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;

uniform vec3 uShadeColor;
uniform float uShadingShift;
uniform float uShadingToony;

uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform float uLightIntensity;

uniform sampler2D uShadowMap;
uniform mat4 uLightViewProjection;
uniform int uShadowEnabled;

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
        normalize(vertexNormal);

    const vec3 lightDirection =
        normalize(-uLightDirection);

    const float normalDotLight =
        dot(normal, lightDirection);

    const float shadingToony =
        clamp(uShadingToony, 0.0, 1.0);

    const float transitionWidth =
        max(1.0 - shadingToony, 0.0001);

    const float shadingFactor =
        clamp(
            (normalDotLight + uShadingShift) / transitionWidth,
            0.0,
            1.0);

    const vec3 lightRadiance =
        uLightColor * max(uLightIntensity, 0.0);

    const vec3 litColor =
        baseColor.rgb * lightRadiance;

    const vec3 shadeColor =
        baseColor.rgb * uShadeColor * lightRadiance;
    
    const float shadowVisibility =
        calculateShadowVisibility(
            vertexWorldPosition,
            normal,
            lightDirection);

    const float visibleShadingFactor =
        shadingFactor *
        shadowVisibility;

    const vec3 finalColor =
        mix(shadeColor, litColor, visibleShadingFactor);

    outColor = vec4(finalColor, baseColor.a);
}
