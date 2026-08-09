#pragma once

#include <asset/AssetHandle.hpp>
#include <material/MaterialTemplate.hpp>
#include <material/mtoon/MToonMaterialParameters.hpp>

#include <optional>

#include <glm/vec4.hpp>

namespace stylized::asset
{
struct TextureAsset;
struct MaterialAsset;
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

    std::optional<MToonMaterialParameters> mtoonParameters;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !templateHandle.isNull();
    }
};

[[nodiscard]] MaterialInstance makeMaterialInstance(
    asset::AssetHandle<MaterialTemplate> templateHandle,
    const asset::MaterialAsset& source);

} // namespace stylized::material