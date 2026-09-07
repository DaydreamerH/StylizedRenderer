#version 450 core

layout(location = 0) in vec3 vertexNormal;
layout(location = 1) in vec2 vertexTexCoord0;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterialId;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;

uniform int uAlphaMaskEnabled;
uniform float uAlphaCutoff;
uniform vec3 uOutlineMaterialId;

void main()
{
    const vec4 sampledColor =
        texture(
            uBaseColorTexture,
            vertexTexCoord0);

    const vec4 baseColor =
        sampledColor * uBaseColorFactor;

    if (uAlphaMaskEnabled != 0 &&
        baseColor.a < uAlphaCutoff)
    {
        discard;
    }

    const vec3 normal =
        normalize(vertexNormal);

    outColor = baseColor;

    outNormal =
        vec4(normal * 0.5 + 0.5, 1.0);

    outMaterialId =
        vec4(uOutlineMaterialId, 1.0);
}
