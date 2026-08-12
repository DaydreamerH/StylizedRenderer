#version 450 core

layout(location = 0) in vec3 vertexNormal;
layout(location = 1) in vec2 vertexTexCoord0;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;

void main()
{
    const vec4 sampledColor = texture(uBaseColorTexture, vertexTexCoord0);

    const vec3 normal =
        normalize(vertexNormal);

    outColor = sampledColor * uBaseColorFactor;

    outNormal = vec4(normal * 0.5 + 0.5, 1.0);
}
