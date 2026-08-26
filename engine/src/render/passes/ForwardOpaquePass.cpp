#include <render/passes/ForwardOpaquePass.hpp>

#include <graphics/resources/Framebuffer.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <render/resources/RuntimeResourceCache.hpp>

#include <utility>
#include <array>

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
        "Fallback Shadow Depth";

    fallbackShadowMap_ =
        graphicsDevice_.createDepthTexture(depthDesc);

    if (!fallbackShadowMap_.isValid())
    {
        return false;
    }

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
        !depth_.isValid() ||
        !normal_.isValid() ||
        !materialId_.isValid())
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
    frame.normal = &normal_;
    frame.materialId = &materialId_;
    frame.framebuffer = &framebuffer_;

    graphicsDevice_.bindFramebuffer(&framebuffer_);
    graphicsDevice_.setViewport(extent_);
    graphicsDevice_.clear(clearValue_);

    graphicsDevice_.clearColorAttachment(
        1,
        graphics::ClearValue{
            0.5F, 0.5F, 1.0F, 0.0F
        });

    graphicsDevice_.clearColorAttachment(
        2,
        graphics::ClearValue{
            0.0F, 0.0F, 0.0F, 0.0F
        });

    const bool shadowMapAvailable =
        frame.shadowsEnabled &&
        frame.shadowMap != nullptr &&
        frame.shadowMap->isValid();

    const graphics::DepthTexture& sampledShadowMap =
        shadowMapAvailable
            ? *frame.shadowMap
            : fallbackShadowMap_;

    if (!renderer_.render(
            *frame.renderWorld,
            sampledShadowMap,
            shadowMapAvailable,
            frame.faceHairShadowMask,
            StaticModelRenderQueue::Opaque))
    {
        return false;
    }

    lastDrawCallCount_ =
        renderer_.lastDrawCallCount();

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

    const bool rebuilding =
        framebuffer_.isValid();

    graphics::RenderTextureDesc colorDesc;
    colorDesc.extent = extent;
    colorDesc.format = graphics::RenderTextureFormat::RGBA16Float;
    colorDesc.sampled = true;
    colorDesc.debugName = "Forward Opaque Color";

    graphics::RenderTexture newHdrColor = graphicsDevice_.createRenderTexture(colorDesc);

    graphics::RenderTextureDesc normalDesc;
    normalDesc.extent = extent;
    normalDesc.format = graphics::RenderTextureFormat::RGBA8;
    normalDesc.sampled = true;
    normalDesc.debugName =
        "Forward Opaque Normal";

    graphics::RenderTexture newNormal =
        graphicsDevice_.createRenderTexture(
            normalDesc);

    if (!newNormal.isValid())
    {
        return false;
    }

    graphics::RenderTextureDesc materialIdDesc;
    materialIdDesc.extent = extent;
    materialIdDesc.format = graphics::RenderTextureFormat::RGBA8;
    materialIdDesc.sampled = true;
    materialIdDesc.debugName =
        "Forward Opaque Material ID";

    graphics::RenderTexture newMaterialId =
        graphicsDevice_.createRenderTexture(
            materialIdDesc);

    if (!newMaterialId.isValid())
    {
        return false;
    }

    if (!newHdrColor.isValid()) return false;

    graphics::DepthTextureDesc depthDesc;
    depthDesc.extent = extent;
    depthDesc.format = graphics::DepthTextureFormat::Depth24Stencil8;
    depthDesc.debugName = "Forward Opaque Depth";

    graphics::DepthTexture newDepth = graphicsDevice_.createDepthTexture(depthDesc);

    if (!newDepth.isValid()) return false;

    graphics::FramebufferDesc framebufferDesc;

    const std::array<const graphics::RenderTexture*, 3>
        colorTextures{
            &newHdrColor,
            &newNormal,
            &newMaterialId
        };

    framebufferDesc.colorTextures = colorTextures;
    framebufferDesc.depthTexture = &newDepth;
    framebufferDesc.debugName = "Forward Opaque Framebuffer";

    graphics::Framebuffer newFramebuffer =
        graphicsDevice_.createFramebuffer(framebufferDesc);

    if (!newFramebuffer.isValid()) return false;

    hdrColor_ = std::move(newHdrColor);
    normal_ = std::move(newNormal);
    materialId_ = std::move(newMaterialId);
    depth_ = std::move(newDepth);
    framebuffer_ = std::move(newFramebuffer);
    extent_ = extent;

    if (rebuilding)
    {
        ++renderTargetRebuildCount_;
    }

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

bool ForwardOpaquePass::hasRenderTargets() const noexcept
{
    return hdrColor_.isValid() &&
        normal_.isValid() &&
        materialId_.isValid() &&
        depth_.isValid() &&
        framebuffer_.isValid();
}

graphics::Extent2D ForwardOpaquePass::renderTargetExtent()
    const noexcept
{
    return extent_;
}

graphics::RenderTextureFormat
ForwardOpaquePass::normalFormat() const noexcept
{
    return normal_.format();
}

graphics::RenderTextureFormat ForwardOpaquePass::colorFormat()
    const noexcept
{
    return hdrColor_.format();
}

graphics::DepthTextureFormat ForwardOpaquePass::depthFormat()
    const noexcept
{
    return depth_.format();
}

std::size_t ForwardOpaquePass::renderTargetRebuildCount()
    const noexcept
{
    return renderTargetRebuildCount_;
}

void ForwardOpaquePass::setClearValue(const graphics::ClearValue& value) noexcept
{
    clearValue_ = value;
}


} // namespace stylized::render
