#version 450 core

layout(location = 0) in vec3 vertexNormal;
layout(location = 1) in vec2 vertexTexCoord0;

layout(location = 0) out vec4 outColor;

uniform sampler2D uBaseColorTexture;
uniform vec4 uBaseColorFactor;

void main()
{
    vec4 baseColor = texture(uBaseColorTexture, vertexTexCoord0);

    vec3 normal = normalize(vertexNormal);

    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.6));

    float diffuse = max(dot(normal, lightDirection), 0.0);

    float lighting = 0.25 + 0.75 * diffuse;

    outColor = vec4(baseColor.rgb * uBaseColorFactor.rgb * lighting, baseColor.a * uBaseColorFactor.a);
}