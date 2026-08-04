#include <render/ForwardOpaquePass.hpp>

#include <graphics/Framebuffer.hpp>
#include <graphics/GraphicsDevice.hpp>
#include <render/RuntimeResourceCache.hpp>

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

    if (!renderer_.initialize()) return false;

    initialized_ = true;
    return true;
}

bool ForwardOpaquePass::execute(FrameContext& frame)
{
    lastDrawCallCount_ = 0;

    if (!initialized_) return false;

    if (frame.renderWorld == nullptr) return false;

    if (frame.framebuffer != nullptr && !frame.framebuffer->isValid()) return false;

    graphics::Extent2D targetExtent = frame.framebufferSize;

    if (frame.framebuffer != nullptr)
        targetExtent = frame.framebuffer->extent();

    if (targetExtent.width == 0 || targetExtent.height == 0)
        return false;

    graphicsDevice_.bindFramebuffer(frame.framebuffer);
    graphicsDevice_.setViewport(targetExtent);
    graphicsDevice_.clear(clearValue_);
    const bool rendered = renderer_.render(*frame.renderWorld);

    lastDrawCallCount_ = renderer_.lastDrawCallCount();

    return rendered;
}

bool ForwardOpaquePass::resize(graphics::Extent2D extent)
{
    return extent.width != 0 && extent.height != 0;
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
