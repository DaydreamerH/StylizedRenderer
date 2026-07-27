#version 450 core

layout (location = 0) in vec2 vertexUv;

layout (location = 0) out vec4 outColor;

uniform sampler2D uTexture;
uniform vec4 uTint;

void main()
{
   outColor = texture(uTexture, vertexUv) * uTint;
}