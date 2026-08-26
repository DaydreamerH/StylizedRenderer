#pragma once

#include <core/NonCopyable.hpp>
#include <render/world/RenderWorld.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace stylized::asset
{

class AssetRegistry;

} // namespace stylized::asset

namespace stylized::graphics
{
    
class GraphicsDevice;
class DepthTexture;
class RenderTexture;

} // namespace stylized::graphics

namespace stylized::render
{

enum class StaticModelRenderQueue : std::uint8_t
{
    Opaque,
    Transparent
};

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
        const graphics::DepthTexture& shadowMap,
        bool shadowMapAvailable,
        const graphics::RenderTexture* faceHairShadowMask,
        StaticModelRenderQueue renderQueue);

    [[nodiscard]] std::size_t lastDrawCallCount() const noexcept;

private:
    std::vector<const RenderItem*> renderItems_;

};

} // namespace stylized::render
