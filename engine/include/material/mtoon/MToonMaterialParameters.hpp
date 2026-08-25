#pragma once

#include <asset/AssetHandle.hpp>

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>

namespace stylized::asset
{

struct TextureAsset;

} // namespace stylized::asset

namespace stylized::material
{

enum class OutlineWidthMode : std::uint8_t
{
    World,
    Screen
};

struct MToonOutlineParameters
{
    bool enabled = false;

    OutlineWidthMode widthMode =
        OutlineWidthMode::Screen;

    float width = 1.0F;

    glm::vec3 color{0.0F};

    float lightingMix = 0.0F;
};

struct MToonTextureBindings
{
    asset::AssetHandle<asset::TextureAsset>
        shadeTexture;

    asset::AssetHandle<asset::TextureAsset>
        toonRampTexture;

    asset::AssetHandle<asset::TextureAsset>
        normalTexture;

    asset::AssetHandle<asset::TextureAsset>
        shadingShiftTexture;

    asset::AssetHandle<asset::TextureAsset>
        matcapTexture;

    asset::AssetHandle<asset::TextureAsset>
        rimMaskTexture;

    asset::AssetHandle<asset::TextureAsset>
        emissionTexture;

    asset::AssetHandle<asset::TextureAsset>
        outlineWidthMaskTexture;

    asset::AssetHandle<asset::TextureAsset>
        occlusionTexture;

    asset::AssetHandle<asset::TextureAsset>
        specularTexture;
};

struct MToonMaterialParameters
{
    // Materials in the same non-empty group share one screen-outline ID.
    // This allows separately exported hair materials to suppress their
    // internal overlap edges without merging their rendering parameters.
    std::string outlineGroup;

    bool outlineDetectSelfDepth = true;
    bool outlineDetectSelfNormal = true;

    glm::vec3 shadeColor{0.1F};

    float shadingShift = 0.0F;
    float shadingShiftTextureScale = 1.0F;
    float shadingToony = 0.9F;
    float giEqualization = 0.9F;

    float normalScale = 1.0F;
    float surfaceOffset = 0.0F;
    float shadowNormalInfluence = 0.0F;
    bool receiveShadow = true;
    bool shadowCutoffEnabled = false;
    float shadowCutoff = 0.5F;

    bool sphericalFaceNormalEnabled = false;
    glm::vec3 sphericalFaceNormalCenter{0.0F};
    float sphericalFaceNormalRadius = 0.06F;
    float sphericalFaceNormalSoftness = 0.015F;
    float sphericalFaceNormalBlend = 1.0F;

    glm::vec3 matcapColor{1.0F};
    float matcapStrength = 0.0F;

    glm::vec3 rimColor{0.0F};
    float rimFresnelPower = 5.0F;
    float rimLift = 0.0F;
    float rimLightingMix = 1.0F;

    glm::vec3 emissionColor{0.0F};
    float emissionStrength = 1.0F;

    MToonOutlineParameters outline;

    MToonTextureBindings textures;

    float occlusionStrength = 1.0F;

    glm::vec3 specularColor{1.0F};
    float specularStrength = 1.0F;
    float specularPower = 64.0F;
};

} // namespace stylized::material
