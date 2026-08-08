#include <render/ShadowPass.hpp>

#include <graphics/GraphicsCommands.hpp>
#include <graphics/GraphicsDevice.hpp>

#include <render/RenderWorld.hpp>
#include <render/RuntimeMesh.hpp>

#include <utility>

namespace stylized::render
{

ShadowPass::ShadowPass(
    graphics::GraphicsDevice& graphicsDevice) noexcept
    : graphicsDevice_(graphicsDevice)
{
}

bool ShadowPass::initialize()
{
    if (initialized_)
    {
        return true;
    }

    graphics::ShaderProgramDesc shaderDesc;

    shaderDesc.vertexShaderPath =
        "assets/shaders/shadow/shadow.vert";

    shaderDesc.fragmentShaderPath =
        "assets/shaders/shadow/shadow.frag";

    shaderDesc.debugName =
        "Directional Shadow";

    shader_ =
        graphicsDevice_.createShaderProgram(
            shaderDesc);

    if (!shader_.isValid())
    {
        return false;
    }

    initialized_ = true;
    return true;
}

bool ShadowPass::ensureResources(
    const graphics::Extent2D extent)
{
    if (extent.width == 0 ||
        extent.height == 0)
    {
        return false;
    }

    if (depth_.isValid() &&
        framebuffer_.isValid() &&
        extent_.width == extent.width &&
        extent_.height == extent.height)
    {
        return true;
    }

    graphics::DepthTextureDesc depthDesc;

    depthDesc.extent = extent;

    depthDesc.format =
        graphics::DepthTextureFormat::Depth32Float;

    depthDesc.debugName =
        "Directional Shadow Depth";

    graphics::DepthTexture newDepth =
        graphicsDevice_.createDepthTexture(
            depthDesc);

    if (!newDepth.isValid())
    {
        return false;
    }

    graphics::FramebufferDesc framebufferDesc;

    framebufferDesc.depthTexture =
        &newDepth;

    framebufferDesc.debugName =
        "Directional Shadow Framebuffer";

    graphics::Framebuffer newFramebuffer =
        graphicsDevice_.createFramebuffer(
            framebufferDesc);

    if (!newFramebuffer.isValid())
    {
        return false;
    }

    depth_ = std::move(newDepth);
    framebuffer_ = std::move(newFramebuffer);
    extent_ = extent;

    return true;
}

bool ShadowPass::execute(
    FrameContext& frame)
{
    lastDrawCallCount_ = 0;
    frame.shadowMap = nullptr;

    if (!initialized_ ||
        frame.renderWorld == nullptr)
    {
        return false;
    }

    const RenderWorld& renderWorld =
        *frame.renderWorld;

    if (renderWorld.shadowItems.empty())
    {
        return true;
    }

    const ShadowView& shadowView =
        renderWorld.shadowView;

    if (!ensureResources(
            shadowView.extent))
    {
        return false;
    }

    if (!shader_.setMat4(
            "uLightViewProjection",
            shadowView.viewProjection))
    {
        return false;
    }

    graphicsDevice_.bindFramebuffer(
        &framebuffer_);

    graphicsDevice_.setViewport(
        extent_);

    graphicsDevice_.clearDepth();

    for (const ShadowRenderItem& item :
         renderWorld.shadowItems)
    {
        if (item.primitive == nullptr ||
            !item.primitive->isValid())
        {
            continue;
        }

        if (!shader_.setMat4(
                "uModel",
                item.world))
        {
            graphicsDevice_.bindFramebuffer(
                nullptr);

            graphicsDevice_.setViewport(
                frame.framebufferSize);

            return false;
        }

        graphics::DrawIndexedCommand command;

        command.shader =
            &shader_;

        command.vertexArray =
            &item.primitive->vertexArray();

        command.topology =
            graphics::PrimitiveTopology::Triangles;

        command.indexType =
            item.primitive->indexType();

        command.indexCount =
            item.primitive->indexCount();

        command.firstIndex = 0;

        graphicsDevice_.drawIndexed(
            command);

        ++lastDrawCallCount_;
    }

    frame.shadowMap =
        &depth_;

    return true;
}

bool ShadowPass::resize(
    const graphics::Extent2D extent)
{
    return extent.width > 0 &&
        extent.height > 0;
}

std::string_view ShadowPass::name()
    const noexcept
{
    return "ShadowPass";
}

std::size_t ShadowPass::lastDrawCallCount()
    const noexcept
{
    return lastDrawCallCount_;
}

} // namespace stylized::render
