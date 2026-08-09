#version 450 core

layout(location = 0) in vec3 vertexNormal;
layout(location = 1) in vec2 vertexTexCoord0;

layout(location = 0) out vec4 outColor;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;

uniform vec3 uShadeColor;
uniform float uShadingShift;
uniform float uShadingToony;

uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform float uLightIntensity;

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

    const vec3 finalColor =
        mix(shadeColor, litColor, shadingFactor);

    outColor = vec4(finalColor, baseColor.a);
}
