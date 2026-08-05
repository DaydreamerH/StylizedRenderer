#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/GraphicsTypes.hpp>
#include <graphics/DepthTexture.hpp>
#include <graphics/Framebuffer.hpp>
#include <graphics/RenderTexture.hpp>
#include <render/IRenderPass.hpp>
#include <render/StaticModelRenderer.hpp>

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

class ForwardOpaquePass final : public IRenderPass, public core::NonCopyable
{
public:
    ForwardOpaquePass(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::AssetRegistry& assetRegistry,
        RuntimeResourceCache& resourceCache
    ) noexcept;

    ~ForwardOpaquePass() override = default;

    [[nodiscard]] bool initialize();

    [[nodiscard]] bool execute(FrameContext& frame) override;

    [[nodiscard]] bool resize(graphics::Extent2D extent) override;

    [[nodiscard]] std::string_view name() const noexcept override;

    [[nodiscard]] std::size_t lastDrawCallCount() const noexcept;

    void setClearValue(const graphics::ClearValue& value) noexcept;

private:
    graphics::GraphicsDevice& graphicsDevice_;

    StaticModelRenderer renderer_;

    graphics::ClearValue clearValue_{
        0.06F,
        0.07F,
        0.10F,
        1.0F
    };

    std::size_t lastDrawCallCount_ = 0;
    bool initialized_ = false;

    graphics::RenderTexture hdrColor_;
    graphics::DepthTexture depth_;
    graphics::Framebuffer framebuffer_;

    graphics::Extent2D extent_{};
};

} // namespace stylized::render
