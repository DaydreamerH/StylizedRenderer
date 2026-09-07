#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/resources/DepthTexture.hpp>
#include <graphics/resources/Framebuffer.hpp>
#include <graphics/resources/RenderTexture.hpp>
#include <graphics/resources/ShaderProgram.hpp>
#include <render/pipeline/IRenderPass.hpp>

#include <cstddef>
#include <string_view>

namespace stylized::asset
{
class AssetRegistry;
}

namespace stylized::graphics
{
class GraphicsDevice;
}

namespace stylized::render
{
class RuntimeResourceCache;

class FaceHairShadowPass final
    : public IRenderPass,
      public core::NonCopyable
{
public:
    FaceHairShadowPass(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::AssetRegistry& assetRegistry,
        RuntimeResourceCache& resourceCache) noexcept;

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool execute(FrameContext& frame) override;
    [[nodiscard]] bool resize(graphics::Extent2D extent) override;
    [[nodiscard]] std::string_view name() const noexcept override;

private:
    [[nodiscard]] bool ensureResources(graphics::Extent2D extent);

    graphics::GraphicsDevice& graphicsDevice_;
    const asset::AssetRegistry& assetRegistry_;
    RuntimeResourceCache& resourceCache_;

    graphics::ShaderProgram shader_;
    graphics::RenderTexture mask_;
    graphics::DepthTexture depth_;
    graphics::Framebuffer framebuffer_;
    graphics::Extent2D extent_{};
    bool initialized_ = false;
};

} // namespace stylized::render
