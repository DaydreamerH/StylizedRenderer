#pragma once

#include <asset/AssetHandle.hpp>
#include <asset/MaterialTemplate.hpp>

#include <glm/vec4.hpp>

namespace stylized::asset
{
    
struct TextureAsset;

struct MaterialInstance
{
    AssetHandle<MaterialTemplate> templateHandle;

    glm::vec4 baseColorFactor{1.F};

    float metallic = 0.F;
    float roughness = 1.F;

    AssetHandle<TextureAsset> baseColorTexture;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !templateHandle.isNull();
    }
};

} // namespace stylized::asset
