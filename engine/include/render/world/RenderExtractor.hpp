#pragma once

#include <core/NonCopyable.hpp>
#include <render/world/RenderWorld.hpp>
#include <asset/AssetHandle.hpp>

namespace stylized::asset
{
class AssetRegistry;
struct SceneAsset;
}

namespace stylized::scene
{
class Camera;
}

namespace stylized::material
{

struct MaterialTemplate;

} // namespace stylized::material

namespace stylized::render
{

class RuntimeResourceCache;

class RenderExtractor final : public core::NonCopyable
{
public:
    explicit RenderExtractor(RuntimeResourceCache& resourceCache) noexcept;
    ~RenderExtractor() = default;

    [[nodiscard]] bool extract(
        const asset::SceneAsset& sceneAsset,
        const asset::AssetRegistry& assetRegistry,
        const scene::Camera& camera,
        const DirectionalLightData& mainLight,
        asset::AssetHandle<
            material::MaterialTemplate>
            materialTemplate,
        RenderWorld& renderWorld) const;

private:
    RuntimeResourceCache& resourceCache_;
};

} // namespace stylized::render


