#version 450 core

layout(location = 0) in vec3 vertexNormal;
layout(location = 1) in vec2 vertexTexCoord0;
layout(location = 2) in vec3 vertexWorldPosition;

layout(location = 0) out vec4 outColor;

uniform sampler2D uBaseColorTexture;

uniform vec4 uBaseColorFactor;
uniform float uMetallic;
uniform float uRoughness;

uniform vec3 uCameraPosition;

uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform float uLightIntensity;

const float PI = 3.14159265359;

float distributionGGX(
    const vec3 normal,
    const vec3 halfwayDirection,
    const float roughness
)
{
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;

    const float NdotH = max(dot(normal, halfwayDirection), 0.0);
    const float NdotH2 = NdotH * NdotH;

    const float denomTerm = NdotH2 * (alpha2 - 1.0) + 1.0;

    return alpha2 / max(PI * denomTerm * denomTerm, 0.001);
}

float geometrySchlickGGX(
    const float normalDotDirection,
    const float roughness)
{
    const float adjustedRoughness = roughness + 1.0;

    const float k = adjustedRoughness * adjustedRoughness / 8.0;

    return normalDotDirection / max(normalDotDirection * (1.0 - k) + k, 0.001);
}

float geometrySmith(
    const vec3 normal,
    const vec3 viewDirection,
    const vec3 lightDirection,
    const float roughness)
{
    const float NdotV = max(dot(normal, viewDirection), 0.0);
    const float NdotL = max(dot(normal, lightDirection), 0.0);

    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(
    const float cosine,
    const vec3 F0)
{
    return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - cosine, 0.0, 1.0), 5.0);
}

void main()
{
    const vec4 sampledBaseColor = texture(uBaseColorTexture, vertexTexCoord0);

    const vec3 baseColor = sampledBaseColor.rgb * uBaseColorFactor.rgb;

    const float metallic = clamp(uMetallic, 0.0, 1.0);

    const float roughness = clamp(uRoughness, 0.04, 1.0);

    const vec3 normal = normalize(vertexNormal);

    const vec3 viewDirection = normalize(uCameraPosition - vertexWorldPosition);

    const vec3 lightDirection = normalize(-uLightDirection);

    const vec3 halfwayDirection = normalize(lightDirection + viewDirection);

    const float NdotL = max(dot(normal, lightDirection), 0.0);

    const float NdotV = max(dot(normal, viewDirection), 0.0);

    const vec3 dielectriReflectance = vec3(0.04);

    const vec3 F0 = mix(dielectriReflectance, baseColor, metallic);

    const float normalDistribution = distributionGGX(normal, halfwayDirection, roughness);

    const float geometry = geometrySmith(normal, viewDirection, lightDirection, roughness);

    const vec3 fresnel = fresnelSchlick(max(dot(halfwayDirection, viewDirection), 0.0), F0);

    const vec3 numerator = normalDistribution * geometry * fresnel;

    const float denominator = max(4.0 * NdotV * NdotL, 0.0001);

    const vec3 specular = numerator / denominator;

    const vec3 diffuseContribution = (vec3(1.0) - fresnel) * (vec3(1.0) - metallic);

    const vec3 radiance = uLightColor * max(uLightIntensity, 0.0);

    const vec3 directLighting = (diffuseContribution * baseColor / PI + specular) * radiance * NdotL;

    const vec3 ambientLighting = baseColor * 0.03;

    const vec3 finalColor = ambientLighting + directLighting;

    const float alpha = sampledBaseColor.a * uBaseColorFactor.a;

    outColor = vec4(finalColor, alpha);
}
