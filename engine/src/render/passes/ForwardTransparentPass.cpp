#include <render/passes/ForwardTransparentPass.hpp>

#include <graphics/device/GraphicsDevice.hpp>
#include <graphics/resources/Framebuffer.hpp>
#include <render/resources/RuntimeResourceCache.hpp>

namespace stylized::render
{

ForwardTransparentPass::ForwardTransparentPass(
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

bool ForwardTransparentPass::initialize()
{
    if (initialized_)
    {
        return true;
    }

    graphics::DepthTextureDesc depthDesc;

    depthDesc.extent = {1, 1};
    depthDesc.format =
        graphics::DepthTextureFormat::Depth32Float;
    depthDesc.comparisonSampling = true;
    depthDesc.debugName =
        "Transparent Fallback Shadow Depth";

    fallbackShadowMap_ =
        graphicsDevice_.createDepthTexture(depthDesc);

    if (!fallbackShadowMap_.isValid())
    {
        return false;
    }

    initialized_ = true;
    return true;
}

bool ForwardTransparentPass::execute(
    FrameContext& frame)
{
    lastDrawCallCount_ = 0;

    if (!initialized_ ||
        frame.renderWorld == nullptr ||
        frame.framebuffer == nullptr ||
        !frame.framebuffer->isValid() ||
        extent_.width == 0 ||
        extent_.height == 0)
    {
        return false;
    }

    const bool shadowMapAvailable =
        frame.shadowsEnabled &&
        frame.shadowMap != nullptr &&
        frame.shadowMap->isValid();

    const graphics::DepthTexture& sampledShadowMap =
        shadowMapAvailable
            ? *frame.shadowMap
            : fallbackShadowMap_;

    graphicsDevice_.bindFramebuffer(
        frame.framebuffer);

    graphicsDevice_.setViewport(extent_);

    graphicsDevice_.setDepthWrite(false);
    graphicsDevice_.setAlphaBlending(true);

    graphicsDevice_.setColorAttachmentWrite(
        1,
        false);

    graphicsDevice_.setColorAttachmentWrite(
        2,
        false);

    const auto restoreState =
        [this, &frame]()
        {
            graphicsDevice_.setColorAttachmentWrite(
                1,
                true);

            graphicsDevice_.setColorAttachmentWrite(
                2,
                true);

            graphicsDevice_.setAlphaBlending(false);
            graphicsDevice_.setDepthWrite(true);

            graphicsDevice_.bindFramebuffer(nullptr);

            graphicsDevice_.setViewport(
                frame.framebufferSize);
        };

    const bool rendered =
        renderer_.render(
            *frame.renderWorld,
            sampledShadowMap,
            shadowMapAvailable,
            StaticModelRenderQueue::Transparent);

    lastDrawCallCount_ =
        renderer_.lastDrawCallCount();

    restoreState();

    return rendered;
}

bool ForwardTransparentPass::resize(
    const graphics::Extent2D extent)
{
    if (extent.width == 0 ||
        extent.height == 0)
    {
        return false;
    }

    extent_ = extent;
    return true;
}

std::string_view ForwardTransparentPass::name()
    const noexcept
{
    return "ForwardTransparentPass";
}

std::size_t
ForwardTransparentPass::lastDrawCallCount()
    const noexcept
{
    return lastDrawCallCount_;
}

} // namespace stylized::render
