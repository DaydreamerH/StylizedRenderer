#include <render/passes/FaceHairShadowPass.hpp>

#include <asset/AssetRegistry.hpp>
#include <graphics/device/GraphicsCommands.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <material/MaterialInstance.hpp>
#include <render/resources/RuntimeMesh.hpp>
#include <render/resources/RuntimeResourceCache.hpp>
#include <render/resources/SkinningPalette.hpp>
#include <render/world/RenderWorld.hpp>

#include <array>
#include <utility>

namespace stylized::render
{
namespace
{
constexpr std::uint32_t skinningPaletteBinding = 0;
}

FaceHairShadowPass::FaceHairShadowPass(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::AssetRegistry& assetRegistry,
    RuntimeResourceCache& resourceCache) noexcept
    : graphicsDevice_(graphicsDevice),
      assetRegistry_(assetRegistry),
      resourceCache_(resourceCache)
{
}

bool FaceHairShadowPass::initialize()
{
    graphics::ShaderProgramDesc desc;
    desc.vertexShaderPath =
        "assets/shaders/face_hair_shadow/face_hair_shadow.vert";
    desc.fragmentShaderPath =
        "assets/shaders/face_hair_shadow/face_hair_shadow.frag";
    desc.debugName = "Face Hair Shadow Mask";

    shader_ = graphicsDevice_.createShaderProgram(desc);
    if (!shader_.isValid() ||
        !shader_.setInt("uBaseColorTexture", 0))
    {
        return false;
    }

    initialized_ = true;
    return true;
}

bool FaceHairShadowPass::ensureResources(
    const graphics::Extent2D extent)
{
    if (extent.width == 0 || extent.height == 0)
    {
        return false;
    }

    if (framebuffer_.isValid() &&
        extent_.width == extent.width &&
        extent_.height == extent.height)
    {
        return true;
    }

    graphics::RenderTextureDesc maskDesc;
    maskDesc.extent = extent;
    maskDesc.format = graphics::RenderTextureFormat::RGBA8;
    maskDesc.sampled = true;
    maskDesc.debugName = "Face Hair Shadow Mask";

    graphics::DepthTextureDesc depthDesc;
    depthDesc.extent = extent;
    depthDesc.format = graphics::DepthTextureFormat::Depth32Float;
    depthDesc.debugName = "Face Hair Shadow Depth";

    graphics::RenderTexture mask =
        graphicsDevice_.createRenderTexture(maskDesc);
    graphics::DepthTexture depth =
        graphicsDevice_.createDepthTexture(depthDesc);

    if (!mask.isValid() || !depth.isValid())
    {
        return false;
    }

    const std::array<const graphics::RenderTexture*, 1>
        colorTextures{&mask};

    graphics::FramebufferDesc framebufferDesc;
    framebufferDesc.colorTextures = colorTextures;
    framebufferDesc.depthTexture = &depth;
    framebufferDesc.debugName = "Face Hair Shadow Framebuffer";

    graphics::Framebuffer framebuffer =
        graphicsDevice_.createFramebuffer(framebufferDesc);

    if (!framebuffer.isValid())
    {
        return false;
    }

    mask_ = std::move(mask);
    depth_ = std::move(depth);
    framebuffer_ = std::move(framebuffer);
    extent_ = extent;
    return true;
}

bool FaceHairShadowPass::execute(FrameContext& frame)
{
    frame.faceHairShadowMask = nullptr;

    if (!initialized_ || frame.renderWorld == nullptr)
    {
        return false;
    }

    const FaceHairShadowView& view =
        frame.renderWorld->faceHairShadowView;

    if (!view.valid || frame.renderWorld->faceHairShadowItems.empty())
    {
        return true;
    }

    if (!ensureResources(view.extent) ||
        !shader_.setMat4(
            "uFaceHairShadowViewProjection",
            view.viewProjection) ||
        !shader_.setFloat("uAlphaCutoff", view.alphaCutoff))
    {
        return false;
    }

    graphicsDevice_.bindFramebuffer(&framebuffer_);
    graphicsDevice_.setViewport(extent_);
    graphicsDevice_.setDepthTest(true);
    graphicsDevice_.setDepthWrite(true);
    graphicsDevice_.setCullMode(graphics::CullMode::None);
    graphicsDevice_.clear({1.0F, 1.0F, 1.0F, 1.0F});

    for (const FaceHairShadowRenderItem& item :
         frame.renderWorld->faceHairShadowItems)
    {
        if (item.primitive == nullptr ||
            item.vertexArray == nullptr ||
            item.materialInstance == nullptr)
        {
            continue;
        }

        const bool skinningEnabled = item.skinningPalette != nullptr;
        if (skinningEnabled && !item.skinningPalette->isGpuReady())
        {
            return false;
        }

        if (!shader_.setMat4("uModel", item.world) ||
            !shader_.setInt(
                "uSkinningEnabled",
                skinningEnabled ? 1 : 0) ||
            !shader_.setVec4(
                "uBaseColorFactor",
                item.materialInstance->baseColorFactor))
        {
            return false;
        }

        if (skinningEnabled)
        {
            item.skinningPalette->bind(skinningPaletteBinding);
        }

        const graphics::Texture2D& texture =
            resourceCache_.getOrCreateTexture(
                item.materialInstance->baseColorTexture,
                assetRegistry_);
        if (!texture.isValid()) return false;
        texture.bind(0);

        graphics::DrawIndexedCommand command;
        command.shader = &shader_;
        command.vertexArray = item.vertexArray;
        command.topology = graphics::PrimitiveTopology::Triangles;
        command.indexType = item.primitive->indexType();
        command.indexCount = item.primitive->indexCount();
        graphicsDevice_.drawIndexed(command);
    }

    graphicsDevice_.bindFramebuffer(nullptr);
    graphicsDevice_.setViewport(frame.framebufferSize);
    frame.faceHairShadowMask = &mask_;
    return true;
}

bool FaceHairShadowPass::resize(const graphics::Extent2D extent)
{
    return extent.width > 0 && extent.height > 0;
}

std::string_view FaceHairShadowPass::name() const noexcept
{
    return "FaceHairShadowPass";
}

} // namespace stylized::render
