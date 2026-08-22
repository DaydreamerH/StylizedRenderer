#version 450 core

layout(location = 0) in vec3 vertexNormal;
layout(location = 1) in vec2 vertexTexCoord0;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;

uniform int uAlphaMaskEnabled;
uniform float uAlphaCutoff;

void main()
{
    const float alpha =
        texture(
            uBaseColorTexture,
            vertexTexCoord0).a *
        uBaseColorFactor.a;

    if (uAlphaMaskEnabled != 0 &&
        alpha < uAlphaCutoff)
    {
        discard;
    }

    const vec3 normal =
        normalize(vertexNormal);

    const vec3 displayColor =
        normal * 0.5 + 0.5;

    outColor =
        vec4(displayColor, 1.0);

    outNormal =
        vec4(displayColor, 1.0);
}
