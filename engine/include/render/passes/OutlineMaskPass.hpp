#pragma once

#include <core/NonCopyable.hpp>

#include <graphics/resources/Framebuffer.hpp>
#include <graphics/resources/RenderTexture.hpp>
#include <graphics/resources/ShaderProgram.hpp>

#include <render/pipeline/IRenderPass.hpp>
#include <render/outline/OutlineSettings.hpp>

#include <cstddef>
#include <string_view>

namespace stylized::asset
{

class AssetRegistry;

} // namespace stylized::asset

namespace stylized::graphics
{

class DepthTexture;
class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{

class RuntimeResourceCache;

class OutlineMaskPass final
    : public IRenderPass,
      public core::NonCopyable
{
public:
    explicit OutlineMaskPass(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::AssetRegistry& assetRegistry,
        RuntimeResourceCache& resourceCache
    ) noexcept;

    ~OutlineMaskPass() override = default;

    [[nodiscard]] bool initialize();

    [[nodiscard]] bool resize(graphics::Extent2D extent) override;

    [[nodiscard]] bool execute(FrameContext& frame) override;

    [[nodiscard]] std::string_view name() const noexcept override;

    [[nodiscard]] std::size_t lastDrawCallCount() const noexcept;

    [[nodiscard]] bool hasRenderTarget() const noexcept;

    [[nodiscard]] graphics::Extent2D renderTargetExtent() const noexcept;

    [[nodiscard]] graphics::RenderTextureFormat
        renderTargetFormat() const noexcept;

    [[nodiscard]] std::size_t
        renderTargetRebuildCount() const noexcept;

    void setGlobalSettings(
        const GlobalOutlineSettings& settings) noexcept;

    [[nodiscard]] const GlobalOutlineSettings&
        globalSettings() const noexcept;

private:
    [[nodiscard]] bool ensureFramebuffer(
        const graphics::DepthTexture& depth
    );

    graphics::GraphicsDevice& graphicsDevice_;

    graphics::ShaderProgram shader_;
    graphics::RenderTexture outlineMask_;
    graphics::Framebuffer framebuffer_;

    graphics::Extent2D extent_{};

    const asset::AssetRegistry& assetRegistry_;
    RuntimeResourceCache& resourceCache_;

    std::size_t lastDrawCallCount_ = 0;
    std::size_t renderTargetRebuildCount_ = 0;

    GlobalOutlineSettings globalSettings_;

    bool initialized_ = false;
};

} // namespace stylized::render
