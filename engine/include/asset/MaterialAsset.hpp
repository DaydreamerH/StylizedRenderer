#pragma once

#include <asset/AssetHandle.hpp>

#include <cstdint>
#include <string>

#include <glm/vec4.hpp>

namespace stylized::asset
{

struct TextureAsset;

enum class AlphaMode : std::uint8_t
{
    Opaque,
    Mask,
    Blend
};

struct MaterialAsset
{
    std::string name;

    glm::vec4 baseColorFactor{1.F};

    AssetHandle<TextureAsset> baseColorTexture;

    float metallicFactor = 0.0F;
    float roughnessFactor = 1.0F;

    AlphaMode alphaMode = AlphaMode::Opaque;

    float alphaCutoff = 0.5F;

    bool doubleSided = false;
};


} // namespace stylized::asset
