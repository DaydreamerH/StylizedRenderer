#pragma once

#include <asset/AssetHandle.hpp>
#include <material/MaterialTemplate.hpp>

#include <glm/vec4.hpp>

namespace stylized::asset
{
struct TextureAsset;
}

namespace stylized::material
{

struct MaterialInstance
{
    asset::AssetHandle<MaterialTemplate> templateHandle;

    glm::vec4 baseColorFactor{1.F};

    float metallic = 0.F;
    float roughness = 1.F;

    asset::AssetHandle<asset::TextureAsset> baseColorTexture;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !templateHandle.isNull();
    }
};

} // namespace stylized::material