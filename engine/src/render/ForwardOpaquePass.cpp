#include <render/ForwardOpaquePass.hpp>

#include <graphics/Framebuffer.hpp>
#include <graphics/GraphicsDevice.hpp>
#include <render/RuntimeResourceCache.hpp>

#include <utility>

namespace stylized::render
{

ForwardOpaquePass::ForwardOpaquePass(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::AssetRegistry& assetRegistry,
    RuntimeResourceCache& resourceCache) noexcept
    : graphicsDevice_(graphicsDevice),
      renderer_(
          graphicsDevice,
          assetRegistry,
          resourceCache)
{
}

bool ForwardOpaquePass::initialize()
{
    if (initialized_) return true;

    initialized_ = true;
    return true;
}

bool ForwardOpaquePass::execute(FrameContext& frame)
{
    lastDrawCallCount_ = 0;

    if (!initialized_ ||
        frame.renderWorld == nullptr ||
        !framebuffer_.isValid() ||
        !hdrColor_.isValid() ||
        !depth_.isValid())
    {
        return false;
    }

    if (frame.framebufferSize.width == 0 ||
        frame.framebufferSize.height == 0)
    {
        return false;
    }

    frame.hdrColor = &hdrColor_;
    frame.depth = &depth_;
    frame.framebuffer = &framebuffer_;

    graphicsDevice_.bindFramebuffer(&framebuffer_);
    graphicsDevice_.setViewport(extent_);
    graphicsDevice_.clear(clearValue_);

    if (!renderer_.render(*frame.renderWorld))
        return false;

    lastDrawCallCount_ =
        renderer_.lastDrawCallCount();

    graphicsDevice_.blitColorToDefaultFramebuffer(
        framebuffer_,
        frame.framebufferSize);

    graphicsDevice_.setViewport(frame.framebufferSize);

    return true;
}

bool ForwardOpaquePass::resize(graphics::Extent2D extent)
{
    if (extent.width == 0 || extent.height == 0)
        return false;

    if (framebuffer_.isValid() &&
        extent_.width == extent.width &&
        extent_.height == extent.height)
        return true;

    graphics::RenderTextureDesc colorDesc;
    colorDesc.extent = extent;
    colorDesc.format = graphics::RenderTextureFormat::RGBA16Float;
    colorDesc.sampled = true;
    colorDesc.debugName = "Forward Opaque Color";

    graphics::RenderTexture newHdrColor = graphicsDevice_.createRenderTexture(colorDesc);

    if (!newHdrColor.isValid()) return false;

    graphics::DepthTextureDesc depthDesc;
    depthDesc.extent = extent;
    depthDesc.format = graphics::DepthTextureFormat::Depth24Stencil8;
    depthDesc.debugName = "Forward Opaque Depth";

    graphics::DepthTexture newDepth = graphicsDevice_.createDepthTexture(depthDesc);

    if (!newDepth.isValid()) return false;

    graphics::FramebufferDesc framebufferDesc;
    framebufferDesc.colorTexture = &newHdrColor;
    framebufferDesc.depthTexture = &newDepth;
    framebufferDesc.debugName = "Forward Opaque Framebuffer";

    graphics::Framebuffer newFramebuffer =
        graphicsDevice_.createFramebuffer(framebufferDesc);

    if (!newFramebuffer.isValid()) return false;

    hdrColor_ = std::move(newHdrColor);
    depth_ = std::move(newDepth);
    framebuffer_ = std::move(newFramebuffer);
    extent_ = extent;

    return true;
}

std::string_view ForwardOpaquePass::name() const noexcept
{
    return "ForwardOpaquePass";
}

std::size_t ForwardOpaquePass::lastDrawCallCount() const noexcept
{
    return lastDrawCallCount_;
}

void ForwardOpaquePass::setClearValue(const graphics::ClearValue& value) noexcept
{
    clearValue_ = value;
}


} // namespace stylized::render
