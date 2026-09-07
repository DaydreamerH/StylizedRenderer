#version 450 core

in vec2 vertexTexCoord0;
layout(location = 0) out vec4 fragmentColor;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;
uniform float uAlphaCutoff;

void main()
{
    const float alpha =
        texture(uBaseColorTexture, vertexTexCoord0).a *
        uBaseColorFactor.a;

    if (alpha < uAlphaCutoff)
    {
        discard;
    }

    fragmentColor = vec4(0.0, 0.0, 0.0, 1.0);
}
