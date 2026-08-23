#pragma once

#include <core/NonCopyable.hpp>
#include <render/world/RenderWorld.hpp>
#include <asset/AssetHandle.hpp>

#include <span>

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

namespace stylized::animation
{

class ScenePose;

} // namespace stylized::animation

namespace stylized::render
{

class RuntimeResourceCache;
class RuntimeMeshInstance;
class SkinningPaletteSet;

class RenderExtractor final : public core::NonCopyable
{
public:
    explicit RenderExtractor(RuntimeResourceCache& resourceCache) noexcept;
    ~RenderExtractor() = default;

    [[nodiscard]] bool extract(
        const asset::SceneAsset& sceneAsset,
        const animation::ScenePose& scenePose,
        const SkinningPaletteSet& skinningPalettes,
        std::span<const RuntimeMeshInstance>
            morphMeshInstances,
        const asset::AssetRegistry& assetRegistry,
        const scene::Camera& camera,
        const DirectionalLightData& mainLight,
        asset::AssetHandle<
            material::MaterialTemplate>
            materialTemplate,
        RenderWorld& renderWorld) const;


    [[nodiscard]] bool beginFrame(
        const scene::Camera& camera,
        const DirectionalLightData& mainLight,
        RenderWorld& renderWorld) const;

    [[nodiscard]] bool appendScene(
        const asset::SceneAsset& sceneAsset,
        const animation::ScenePose& scenePose,
        const SkinningPaletteSet& skinningPalettes,
        std::span<const RuntimeMeshInstance> morphMeshInstances,
        const asset::AssetRegistry& assetRegistry,
        asset::AssetHandle<material::MaterialTemplate> materialTemplate,
        RenderWorld& renderWorld) const;

    [[nodiscard]] bool endFrame(
        RenderWorld& renderWorld) const;

private:
    RuntimeResourceCache& resourceCache_;

};

} // namespace stylized::render


