#version 450 core

layout(location = 0) in vec2 vertexTextureCoordinate;

layout(location = 0) out vec4 fragmentColor;

uniform sampler2D uLdrColor;
uniform vec2 uInverseScreenSize;
uniform int uFxaaEnabled;

const float minimumDirectionReduce = 1.0 / 128.0;
const float directionReduceMultiplier = 1.0 / 8.0;
const float maximumSpan = 8.0;

float calculateLuminance(const vec3 color)
{
    return dot(
        color,
        vec3(
            0.299,
            0.587,
            0.114));
}

void main()
{
    const vec3 centerColor =
        texture(
            uLdrColor,
            vertexTextureCoordinate).rgb;

    if (uFxaaEnabled == 0)
    {
        fragmentColor = vec4(centerColor, 1.0);
        return;
    }

    const vec3 northWestColor =
        texture(
            uLdrColor,
            vertexTextureCoordinate +
                vec2(-1.0, 1.0) *
                    uInverseScreenSize).rgb;

    const vec3 northEastColor =
        texture(
            uLdrColor,
            vertexTextureCoordinate +
                vec2(1.0, 1.0) *
                    uInverseScreenSize).rgb;

    const vec3 southWestColor =
        texture(
            uLdrColor,
            vertexTextureCoordinate +
                vec2(-1.0, -1.0) *
                    uInverseScreenSize).rgb;

    const vec3 southEastColor =
        texture(
            uLdrColor,
            vertexTextureCoordinate +
                vec2(1.0, -1.0) *
                    uInverseScreenSize).rgb;

    const float centerLuminance =
        calculateLuminance(centerColor);
    const float northWestLuminance =
        calculateLuminance(northWestColor);
    const float northEastLuminance =
        calculateLuminance(northEastColor);
    const float southWestLuminance =
        calculateLuminance(southWestColor);
    const float southEastLuminance =
        calculateLuminance(southEastColor);

    const float minimumLuminance =
        min(
            centerLuminance,
            min(
                min(
                    northWestLuminance,
                    northEastLuminance),
                min(
                    southWestLuminance,
                    southEastLuminance)));

    const float maximumLuminance =
        max(
            centerLuminance,
            max(
                max(
                    northWestLuminance,
                    northEastLuminance),
                max(
                    southWestLuminance,
                    southEastLuminance)));

    vec2 direction;
    direction.x =
        -(
            northWestLuminance +
            northEastLuminance -
            southWestLuminance -
            southEastLuminance);

    direction.y =
        northWestLuminance +
        southWestLuminance -
        northEastLuminance -
        southEastLuminance;

    const float directionReduce =
        max(
            (
                northWestLuminance +
                northEastLuminance +
                southWestLuminance +
                southEastLuminance) *
                0.25 *
                directionReduceMultiplier,
            minimumDirectionReduce);

    const float inverseMinimumDirection =
        1.0 /
        (
            min(
                abs(direction.x),
                abs(direction.y)) +
            directionReduce);

    direction =
        clamp(
            direction * inverseMinimumDirection,
            vec2(-maximumSpan),
            vec2(maximumSpan)) *
        uInverseScreenSize;

    const vec3 colorA =
        0.5 *
        (
            texture(
                uLdrColor,
                vertexTextureCoordinate +
                    direction *
                        (1.0 / 3.0 - 0.5)).rgb +
            texture(
                uLdrColor,
                vertexTextureCoordinate +
                    direction *
                        (2.0 / 3.0 - 0.5)).rgb
        );

    const vec3 colorB =
        colorA * 0.5 +
        0.25 *
        (
            texture(
                uLdrColor,
                vertexTextureCoordinate +
                    direction * -0.5).rgb +
            texture(
                uLdrColor,
                vertexTextureCoordinate +
                    direction * 0.5).rgb
        );

    const float colorBLuminance =
        calculateLuminance(colorB);

    const vec3 result =
        colorBLuminance < minimumLuminance ||
        colorBLuminance > maximumLuminance
            ? colorA
            : colorB;

    fragmentColor = vec4(result, 1.0);
}
