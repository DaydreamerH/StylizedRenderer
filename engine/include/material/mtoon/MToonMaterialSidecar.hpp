#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace stylized::material
{

struct MToonSidecarTexturePaths
{
    std::filesystem::path baseColor;
    std::filesystem::path shade;
    std::filesystem::path normal;
    std::filesystem::path shadingShift;
    std::filesystem::path matcap;
    std::filesystem::path rimMask;
    std::filesystem::path emission;
};

struct MToonSidecarMaterial
{
    std::string name;

    glm::vec4 baseColorFactor{1.0F};

    glm::vec3 shadeColor{0.1F};

    float shadingShift = 0.0F;
    float shadingShiftTextureScale = 1.0F;
    float shadingToony = 0.9F;

    float normalScale = 1.0F;

    float giEqualization = 0.9F;

    glm::vec3 matcapColor{1.0F};
    float matcapStrength = 0.0F;

    glm::vec3 rimColor{0.0F};
    float rimFresnelPower = 5.0F;
    float rimLift = 0.0F;
    float rimLightingMix = 1.0F;

    glm::vec3 emissionColor{0.0F};
    float emissionStrength = 1.0F;

    MToonSidecarTexturePaths textures;
};

struct MToonMaterialSidecar
{
    static constexpr std::uint32_t currentVersion = 1;

    std::uint32_t version = currentVersion;

    std::vector<MToonSidecarMaterial> materials;
};

} // namespace stylized::material
