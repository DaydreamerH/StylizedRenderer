#version 450 core

layout(location = 0) out vec4 outOutlineMask;

uniform vec3 uOutlineColor;
uniform float uOutlineLightingMix;

uniform vec3 uLightColor;
uniform float uLightIntensity;

void main()
{
    const vec3 lightRadiance =
        max(uLightColor, vec3(0.0)) *
        max(uLightIntensity, 0.0);

    const vec3 outlineLighting =
        mix(
            vec3(1.0),
            lightRadiance,
            clamp(
                uOutlineLightingMix,
                0.0,
                1.0));

    const vec3 finalOutlineColor =
        uOutlineColor * outlineLighting;

    outOutlineMask =
        vec4(
            finalOutlineColor,
            1.0);
}
