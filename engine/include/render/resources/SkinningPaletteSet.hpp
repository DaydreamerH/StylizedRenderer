#pragma once

#include <core/NonCopyable.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace stylized::animation
{

class ScenePose;

} // namespace stylized::animation

namespace stylized::asset
{

class AssetRegistry;
struct SceneAsset;

} // namespace stylized::asset

namespace stylized::graphics
{

class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{

class SkinningPalette;

class SkinningPaletteSet final
    : public core::NonCopyable
{
public:
    SkinningPaletteSet();
    ~SkinningPaletteSet();

    [[nodiscard]] bool initialize(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::SceneAsset& sceneAsset,
        const asset::AssetRegistry& assetRegistry);

    [[nodiscard]] bool update(
        const asset::SceneAsset& sceneAsset,
        const asset::AssetRegistry& assetRegistry,
        const animation::ScenePose& scenePose);

    [[nodiscard]] const SkinningPalette* find(
        std::uint32_t nodeIndex,
        std::size_t primitiveIndex) const noexcept;

    void clear() noexcept;

    [[nodiscard]] std::size_t paletteCount()
        const noexcept;

    [[nodiscard]] std::size_t jointMatrixCount()
        const noexcept;

    [[nodiscard]] std::size_t lastUploadCount()
        const noexcept;

private:
    std::vector<
        std::vector<
            std::unique_ptr<SkinningPalette>>>
        palettes_;

    std::size_t paletteCount_ = 0;
    std::size_t jointMatrixCount_ = 0;
    std::size_t lastUploadCount_ = 0;
    std::uint64_t lastPoseVersion_ = 0;
};

} // namespace stylized::render
