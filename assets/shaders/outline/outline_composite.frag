#version 450 core

layout(location = 0) in vec2 vertexTextureCoordinate;

layout(location = 0) out vec4 outColor;

uniform sampler2D uHdrColor;
uniform sampler2D uOutlineMask;

void main()
{
    const vec4 hdrColor =
        texture(uHdrColor, vertexTextureCoordinate);

    const vec4 outlineMask =
        texture(
            uOutlineMask,
            vertexTextureCoordinate
        );

    const float coverage =
        clamp(
            outlineMask.a,
            0.0,
            1.0
        );

    const vec3 compositedColor =
        mix(
            hdrColor.rgb,
            outlineMask.rgb,
            coverage
        );

    outColor = vec4(compositedColor, hdrColor.a);
}
