#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <string_view>

#include <material/mtoon/MToonMaterialParameters.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace stylized::asset
{

class AssetRegistry;

} // namespace stylized::asset

namespace stylized::material
{

struct MaterialInstance;
struct MToonSidecarError;

struct MToonSidecarTexturePaths
{
    std::filesystem::path baseColor;
    std::filesystem::path shade;
    std::filesystem::path toonRamp;
    std::filesystem::path normal;
    std::filesystem::path shadingShift;
    std::filesystem::path matcap;
    std::filesystem::path rimMask;
    std::filesystem::path emission;
    std::filesystem::path outlineWidthMask;
    std::filesystem::path occlusion;
    std::filesystem::path specular;
};

struct MToonSidecarMaterial
{
    std::string name;

    // Optional shared identity used only by the global screen outline.
    // Empty preserves the default per-source-material behavior.
    std::string outlineGroup;

    bool outlineDetectSelfDepth = true;
    bool outlineDetectSelfNormal = true;

    bool screenOutlineEnabled = true;
    bool screenOutlineDepthEnabled = true;
    bool screenOutlineNormalEnabled = true;

    std::optional<float> screenOutlineWidth;
    std::optional<float> screenOutlineDepthThreshold;
    std::optional<float> screenOutlineNormalThreshold;
    std::optional<glm::vec3> screenOutlineColor;

    bool hasScreenOutline = false;

    glm::vec4 baseColorFactor{1.0F};

    glm::vec3 shadeColor{0.1F};

    float shadingShift = 0.0F;
    float shadingShiftTextureScale = 1.0F;
    float shadingToony = 0.9F;

    float normalScale = 1.0F;
    float surfaceOffset = 0.0F;
    float shadowNormalInfluence = 0.0F;
    bool castShadow = true;
    bool receiveShadow = true;
    bool shadowCutoffEnabled = false;
    float shadowCutoff = 0.5F;

    bool sphericalFaceNormalEnabled = false;
    glm::vec3 sphericalFaceNormalCenter{0.0F};
    float sphericalFaceNormalRadius = 0.06F;
    float sphericalFaceNormalSoftness = 0.015F;
    float sphericalFaceNormalBlend = 1.0F;

    float giEqualization = 0.9F;

    glm::vec3 matcapColor{1.0F};
    float matcapStrength = 0.0F;

    glm::vec3 rimColor{0.0F};
    float rimFresnelPower = 5.0F;
    float rimLift = 0.0F;
    float rimLightingMix = 1.0F;

    glm::vec3 emissionColor{0.0F};
    float emissionStrength = 1.0F;

    bool outlineEnabled = false;

    OutlineWidthMode outlineWidthMode =
        OutlineWidthMode::World;

    float outlineWidth = 1.0F;

    glm::vec3 outlineColor{0.0F};

    float outlineLightingMix = 0.0F;

    MToonSidecarTexturePaths textures;

    float occlusionStrength = 1.0F;

    glm::vec3 specularColor{1.0F};
    float specularStrength = 1.0F;
    float specularPower = 64.0F;
};

struct MToonMaterialSidecar
{
    static constexpr std::uint32_t
        minimumSupportedVersion = 1;

    static constexpr std::uint32_t
        currentVersion = 3;

    std::uint32_t version = currentVersion;

    std::vector<MToonSidecarMaterial> materials;
};

[[nodiscard]] bool captureMToonSidecarMaterial(
    std::string_view materialName,
    const MaterialInstance& instance,
    const asset::AssetRegistry& assets,
    const std::filesystem::path& sidecarDirectory,
    MToonSidecarMaterial& destination,
    MToonSidecarError& error
);

[[nodiscard]] bool applyMToonSidecarMaterial(
    const MToonSidecarMaterial& source,
    const std::filesystem::path& sidecarDirectory,
    asset::AssetRegistry& assets,
    MaterialInstance& destination,
    MToonSidecarError& error);

} // namespace stylized::material
