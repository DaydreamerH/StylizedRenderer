#pragma once

#include <asset/AssetHandle.hpp>

#include <glm/vec3.hpp>

#include <cstdint>

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
};

struct MToonMaterialParameters
{
    glm::vec3 shadeColor{0.1F};

    float shadingShift = 0.0F;
    float shadingShiftTextureScale = 1.0F;
    float shadingToony = 0.9F;
    float giEqualization = 0.9F;

    float normalScale = 1.0F;

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
};

} // namespace stylized::material
