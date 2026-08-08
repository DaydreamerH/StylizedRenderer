#version 450 core

layout(location = 0) in vec2 vertexTextureCoordinate;

layout(location = 0) out vec4 fragmentColor;

uniform sampler2D uHdrColor;
uniform float uExposure;
uniform int uToneMappingEnabled;

void main()
{
    const vec3 hdrColor =
        texture(
            uHdrColor,
            vertexTextureCoordinate).rgb;

    const float exposure =
        max(
            uExposure,
            0.0);

    const vec3 exposedColor =
        hdrColor * exposure;

    const vec3 mappedColor =
        uToneMappingEnabled != 0
        ? vec3(1.0) - exp(-exposedColor)
        : clamp(
            exposedColor,
            vec3(0.0),
            vec3(1.0));

    const vec3 gammaCorrectedColor =
        pow(
            max(
                mappedColor,
                vec3(0.0)),
            vec3(1.0 / 2.2));

    fragmentColor =
        vec4(
            gammaCorrectedColor,
            1.0);
}
