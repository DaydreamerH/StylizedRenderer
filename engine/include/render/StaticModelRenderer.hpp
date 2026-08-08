#pragma once

#include <core/NonCopyable.hpp>
#include <render/RenderWorld.hpp>

#include <cstddef>

namespace stylized::asset
{

class AssetRegistry;

} // namespace stylized::asset

namespace stylized::graphics
{
    
class GraphicsDevice;
class DepthTexture;

} // namespace stylized::graphics

namespace stylized::render
{

class RuntimeResourceCache;

class StaticModelRenderer final : public core::NonCopyable
{
private:
    graphics::GraphicsDevice& graphicsDevice_;
    const asset::AssetRegistry& assetRegistry_;
    RuntimeResourceCache& resourceCache_;

    std::size_t lastDrawCallCount_ = 0;

public:
    StaticModelRenderer(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::AssetRegistry& assetRegistry,
        RuntimeResourceCache& resourceCache) noexcept;


    ~StaticModelRenderer() = default;

    [[nodiscard]] bool render(
        const RenderWorld& renderWorld,
        const graphics::DepthTexture* shadowMap);

    [[nodiscard]] std::size_t lastDrawCallCount() const noexcept;
};

} // namespace stylized::render
