#version 450 core

in vec2 vTexCoord;

layout(binding = 0)
uniform sampler2D uBaseColorTexture;

uniform vec4 uBaseColorFactor;
uniform bool uAlphaMaskEnabled;
uniform float uAlphaCutoff;

void main()
{
    if (uAlphaMaskEnabled)
    {
        const float alpha =
            texture(
                uBaseColorTexture,
                vTexCoord
            ).a *
            uBaseColorFactor.a;

        if (alpha < uAlphaCutoff)
        {
            discard;
        }
    }
}
