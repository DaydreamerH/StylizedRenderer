#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/device/GraphicsTypes.hpp>
#include <graphics/resources/DepthTexture.hpp>
#include <render/pipeline/IRenderPass.hpp>
#include <render/renderers/StaticModelRenderer.hpp>

#include <cstddef>
#include <string_view>

namespace stylized::asset
{

class AssetRegistry;

} // namespace stylized::asset

namespace stylized::graphics
{

class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{

class RuntimeResourceCache;

class ForwardTransparentPass final
    : public IRenderPass,
      public core::NonCopyable
{
public:
    ForwardTransparentPass(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::AssetRegistry& assetRegistry,
        RuntimeResourceCache& resourceCache) noexcept;

    ~ForwardTransparentPass() override = default;

    [[nodiscard]] bool initialize();

    [[nodiscard]] bool execute(
        FrameContext& frame) override;

    [[nodiscard]] bool resize(
        graphics::Extent2D extent) override;

    [[nodiscard]] std::string_view name()
        const noexcept override;

    [[nodiscard]] std::size_t lastDrawCallCount()
        const noexcept;

private:
    graphics::GraphicsDevice& graphicsDevice_;

    StaticModelRenderer renderer_;

    graphics::DepthTexture fallbackShadowMap_;

    graphics::Extent2D extent_{};

    std::size_t lastDrawCallCount_ = 0;

    bool initialized_ = false;
};

} // namespace stylized::render