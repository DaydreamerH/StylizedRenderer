#include <render/passes/OutlineMaskPass.hpp>

#include <graphics/device/GraphicsCommands.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <graphics/resources/DepthTexture.hpp>
#include <graphics/resources/Texture2D.hpp>

#include <asset/AssetRegistry.hpp>

#include <material/MaterialInstance.hpp>

#include <render/resources/RuntimeMesh.hpp>
#include <render/resources/RuntimeResourceCache.hpp>
#include <render/world/RenderWorld.hpp>
#include <render/resources/SkinningPalette.hpp>

#include <glm/matrix.hpp>

#include <array>
#include <utility>

namespace stylized::render
{

namespace
{

constexpr std::uint32_t skinningPaletteBinding = 0;

} // namespace

OutlineMaskPass::OutlineMaskPass(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::AssetRegistry& assetRegistry,
    RuntimeResourceCache& resourceCache) noexcept
    : graphicsDevice_(graphicsDevice),
      assetRegistry_(assetRegistry),
      resourceCache_(resourceCache)
{
}

bool OutlineMaskPass::initialize()
{
    if (initialized_) return true;

    graphics::ShaderProgramDesc shaderDesc;

    shaderDesc.vertexShaderPath =
        "assets/shaders/outline/outline.vert";
    shaderDesc.fragmentShaderPath =
        "assets/shaders/outline/outline.frag";
    shaderDesc.debugName =
        "Outline Mask";

    graphics::ShaderProgram newShader =
        graphicsDevice_.createShaderProgram(shaderDesc);

    if (!newShader.isValid())
    {
        return false;
    }

    shader_ = std::move(newShader);
    initialized_ = true;

    return true;
}

bool OutlineMaskPass::resize(
    const graphics::Extent2D extent)
{
    if (extent.width == 0 ||
        extent.height == 0)
    {
        return false;
    }

    if (outlineMask_.isValid() &&
        extent_.width == extent.width &&
        extent_.height == extent.height)
    {
        return true;
    }

    const bool rebuilding =
        outlineMask_.isValid();

    graphics::RenderTextureDesc textureDesc;

    textureDesc.extent = extent;
    textureDesc.format =
        graphics::RenderTextureFormat::RGBA8;

    textureDesc.sampled = true;
    textureDesc.debugName =
        "Outline Mask";

    graphics::RenderTexture newOutlineMask =
        graphicsDevice_.createRenderTexture(
            textureDesc);

    if (!newOutlineMask.isValid())
    {
        return false;
    }

    outlineMask_ = std::move(newOutlineMask);

    framebuffer_ = {};
    extent_ = extent;

    if (rebuilding)
    {
        ++renderTargetRebuildCount_;
    }

    return true;
}

bool OutlineMaskPass::ensureFramebuffer(
    const graphics::DepthTexture& depth)
{
    if (framebuffer_.isValid())
        return true;

    const graphics::Extent2D depthExtent =
        depth.extent();

    if (!outlineMask_.isValid() ||
        depthExtent.width != extent_.width ||
        depthExtent.height != extent_.height)
    {
        return false;
    }

    const std::array<const graphics::RenderTexture*, 1>
        colorTextures{&outlineMask_};

    graphics::FramebufferDesc framebufferDesc;

    framebufferDesc.colorTextures = colorTextures;
    framebufferDesc.depthTexture = &depth;
    framebufferDesc.debugName = "Outline Mask Framebuffer";

    graphics::Framebuffer newFramebuffer =
        graphicsDevice_.createFramebuffer(
            framebufferDesc);

    if (!newFramebuffer.isValid())
    {
        return false;
    }

    framebuffer_ = std::move(newFramebuffer);

    return true;
}

bool OutlineMaskPass::execute(FrameContext& frame)
{
    lastDrawCallCount_ = 0;
    frame.outlineMask = nullptr;

    if (!initialized_ ||
        frame.renderWorld == nullptr ||
        frame.depth == nullptr ||
        !frame.depth->isValid() ||
        !outlineMask_.isValid())
    {
        return false;
    }

    if (!ensureFramebuffer(*frame.depth))
        return false;

    const RenderWorld& renderWorld =
        *frame.renderWorld;

    const DirectionalLightData& mainLight =
        renderWorld.mainView.mainLight;

    const bool globalWorldOutlineEnabled =
        globalSettings_.mode ==
            GlobalOutlineMode::World &&
        globalSettings_.worldWidth > 0.0F;

    if (!shader_.setMat4(
            "uViewProjection",
            renderWorld.mainView.viewProjection) ||
        !shader_.setVec2(
            "uViewportSize",
            static_cast<float>(extent_.width),
            static_cast<float>(extent_.height)) ||
        !shader_.setInt(
            "uOutlineWidthMask",
            0) ||
        !shader_.setVec3(
            "uLightColor",
            mainLight.color.r,
            mainLight.color.g,
            mainLight.color.b) ||
        !shader_.setFloat(
            "uLightIntensity",
            mainLight.intensity))
    {
        return false;
    }

    graphicsDevice_.bindFramebuffer(&framebuffer_);

    graphicsDevice_.setViewport(extent_);

    graphicsDevice_.clearColorAttachment(
        0,
        graphics::ClearValue{
            0.0F,
            0.0F,
            0.0F,
            0.0F
        });

    graphicsDevice_.setDepthWrite(false);

    const auto restoreState =
        [this, &frame]()
        {
            graphicsDevice_.setCullMode(
                graphics::CullMode::None);

            graphicsDevice_.setDepthWrite(true);

            graphicsDevice_.bindFramebuffer(nullptr);

            graphicsDevice_.setViewport(
                frame.framebufferSize);
        };

    for (const RenderItem& item : renderWorld.items)
    {
        if (item.materialClass !=
                RenderMaterialClass::Opaque ||
            item.primitive == nullptr ||
            item.vertexArray == nullptr ||
            !item.primitive->isValid())
        {
            continue;
        }

        const material::MToonMaterialParameters*
            mtoonParameters =
                item.materialInstance != nullptr &&
                    item.materialInstance
                        ->mtoonParameters
                    ? &*item.materialInstance
                        ->mtoonParameters
                    : nullptr;

        const bool skinningEnabled =
            item.skinningPalette != nullptr;

        if (skinningEnabled &&
            !item.skinningPalette->isGpuReady())
        {
            restoreState();
            return false;
        }

        const material::MToonOutlineParameters*
            materialOutline =
                mtoonParameters != nullptr
                    ? &mtoonParameters->outline
                    : nullptr;

        const bool materialOutlineEnabled =
            materialOutline != nullptr &&
            materialOutline->enabled &&
            materialOutline->width > 0.0F;

        if (!globalWorldOutlineEnabled &&
            !materialOutlineEnabled)
        {
            continue;
        }

        const material::OutlineWidthMode widthMode =
            globalWorldOutlineEnabled
                ? material::OutlineWidthMode::World
                : materialOutline->widthMode;

        const float width =
            globalWorldOutlineEnabled
                ? globalSettings_.worldWidth
                : materialOutline->width;

        const glm::vec3 color =
            globalWorldOutlineEnabled
                ? globalSettings_.color
                : materialOutline->color;

        const float lightingMix =
            globalWorldOutlineEnabled
                ? 0.0F
                : materialOutline->lightingMix;

        const asset::AssetHandle<asset::TextureAsset>
            widthMaskHandle =
                globalWorldOutlineEnabled ||
                    mtoonParameters == nullptr
                ? asset::AssetHandle<
                    asset::TextureAsset>{}
                : mtoonParameters
                    ->textures.outlineWidthMaskTexture;

        const graphics::Texture2D& widthMaskTexture =
            widthMaskHandle.isNull()
            ? resourceCache_.whiteTexture()
            : resourceCache_.getOrCreateTexture(
                widthMaskHandle,
                assetRegistry_);

        if (!widthMaskTexture.isValid())
        {
            restoreState();
            return false;
        }

        const bool windingFlipped =
            glm::determinant(
                glm::mat3(item.world)) < 0.0F;

        graphicsDevice_.setCullMode(
            windingFlipped
                ? graphics::CullMode::Back
                : graphics::CullMode::Front);

        if (!shader_.setMat4(
                "uModel",
                item.world) ||
            !shader_.setMat3(
                "uNormalMatrix",
                item.normalMatrix) ||
            !shader_.setInt(
                "uSkinningEnabled",
                skinningEnabled ? 1 : 0) ||
            !shader_.setInt(
                "uOutlineWidthMode",
                static_cast<int>(
                    widthMode)) ||
            !shader_.setFloat(
                "uOutlineWidth",
                width) ||
            !shader_.setVec3(
                "uOutlineColor",
                color.r,
                color.g,
                color.b) ||
            !shader_.setFloat(
                "uOutlineLightingMix",
                lightingMix))
        {
            restoreState();
            return false;
        }

        widthMaskTexture.bind(0);

        if (skinningEnabled)
        {
            item.skinningPalette->bind(skinningPaletteBinding);
        }

        graphics::DrawIndexedCommand command;

        command.shader = &shader_;
        command.vertexArray =
            item.vertexArray;

        command.topology =
            graphics::PrimitiveTopology::Triangles;

        command.indexType =
            item.primitive->indexType();

        command.indexCount =
            item.primitive->indexCount();

        graphicsDevice_.drawIndexed(command);

        ++lastDrawCallCount_;
    }

    restoreState();

    frame.outlineMask =
        &outlineMask_;

    return true;
}

std::string_view OutlineMaskPass::name()
    const noexcept
{
    return "OutlineMaskPass";
}

std::size_t OutlineMaskPass::lastDrawCallCount()
    const noexcept
{
    return lastDrawCallCount_;
}

bool OutlineMaskPass::hasRenderTarget() const noexcept
{
    return outlineMask_.isValid();
}

graphics::Extent2D OutlineMaskPass::renderTargetExtent()
    const noexcept
{
    return extent_;
}

graphics::RenderTextureFormat
OutlineMaskPass::renderTargetFormat() const noexcept
{
    return outlineMask_.format();
}

std::size_t OutlineMaskPass::renderTargetRebuildCount()
    const noexcept
{
    return renderTargetRebuildCount_;
}

void OutlineMaskPass::setGlobalSettings(
    const GlobalOutlineSettings& settings) noexcept
{
    globalSettings_ = settings;
}

const GlobalOutlineSettings&
OutlineMaskPass::globalSettings() const noexcept
{
    return globalSettings_;
}


} // namespace stylized::render
