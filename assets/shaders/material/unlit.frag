#version 450 core

layout(location = 1) in vec2 vertexTexCoord0;
layout(location = 0) out vec4 outColor;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;

void main()
{
    const vec4 sampledColor = texture(uBaseColorTexture, vertexTexCoord0);

    outColor = sampledColor * uBaseColorFactor;
}