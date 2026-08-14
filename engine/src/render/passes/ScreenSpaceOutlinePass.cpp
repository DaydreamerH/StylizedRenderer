#include <render/passes/ScreenSpaceOutlinePass.hpp>

#include <graphics/device/GraphicsCommands.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <graphics/resources/DepthTexture.hpp>
#include <render/world/RenderWorld.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace stylized::render
{

namespace
{

struct FullscreenVertex
{
    float positionX = 0.0F;
    float positionY = 0.0F;

    float textureU = 0.0F;
    float textureV = 0.0F;
};

} // namespace

ScreenSpaceOutlinePass::ScreenSpaceOutlinePass(
    graphics::GraphicsDevice& graphicsDevice
) noexcept
: graphicsDevice_(graphicsDevice)
{
}

bool ScreenSpaceOutlinePass::initialize()
{
    if (initialized_)
        return true;

    graphics::ShaderProgramDesc shaderDesc;

    shaderDesc.vertexShaderPath =
        "assets/shaders/postprocess/postprocess.vert";

    shaderDesc.fragmentShaderPath =
        "assets/shaders/outline/outline_composite.frag";

    shaderDesc.debugName =
        "Screen Space Outline";

    graphics::ShaderProgram newShader =
        graphicsDevice_.createShaderProgram(shaderDesc);

    if (!newShader.isValid())
    {
        return false;
    }

    const std::array<FullscreenVertex, 3> vertices{
        FullscreenVertex{
            -1.0F,
            -1.0F,
            0.0F,
            0.0F
        },
        FullscreenVertex{
            3.0F,
            -1.0F,
            2.0F,
            0.0F
        },
        FullscreenVertex{
            -1.0F,
            3.0F,
            0.0F,
            2.0F
        }
    };

    const std::array<std::uint16_t, 3> indices{
        0,
        1,
        2
    };

    graphics::BufferDesc vertexBufferDesc;

    vertexBufferDesc.usage =
        graphics::BufferUsage::Static;

    vertexBufferDesc.debugName =
        "Outline Composite Vertex Buffer";

    graphics::Buffer newVertexBuffer =
        graphicsDevice_.createBuffer(
            vertexBufferDesc,
            std::span<const FullscreenVertex>{
                vertices
            });

    if (!newVertexBuffer.isValid())
        return false;

    graphics::BufferDesc indexBufferDesc;

    indexBufferDesc.usage =
        graphics::BufferUsage::Static;

    indexBufferDesc.debugName =
        "Outline Composite Index Buffer";

    graphics::Buffer newIndexBuffer =
        graphicsDevice_.createBuffer(
            indexBufferDesc,
            std::span<const std::uint16_t>{
                indices
            });

    if (!newIndexBuffer.isValid())
    {
        return false;
    }

    const std::array<
        graphics::VertexAttributeDesc,
        2> attributes{
        graphics::VertexAttributeDesc{
            .location = 0,
            .binding = 0,
            .format =
                graphics::VertexAttributeFormat::Float2,
            .offset =
                offsetof(
                    FullscreenVertex,
                    positionX)
        },
        graphics::VertexAttributeDesc{
            .location = 1,
            .binding = 0,
            .format =
                graphics::VertexAttributeFormat::Float2,
            .offset =
                offsetof(
                    FullscreenVertex,
                    textureU)
        }
    };

    graphics::VertexArrayDesc vertexArrayDesc;

    vertexArrayDesc.vertexBuffer =
        &newVertexBuffer;

    vertexArrayDesc.indexBuffer =
        &newIndexBuffer;

    vertexArrayDesc.vertexBinding.binding = 0;

    vertexArrayDesc.vertexBinding.stride =
        sizeof(FullscreenVertex);

    vertexArrayDesc.attributes =
        std::span<
            const graphics::VertexAttributeDesc>{
                attributes
            };

    vertexArrayDesc.debugName =
        "Outline Composite Vertex Array";

    graphics::VertexArray newVertexArray =
        graphicsDevice_.createVertexArray(
            vertexArrayDesc);

    if (!newVertexArray.isValid())
    {
        return false;
    }

    shader_ = std::move(newShader);
    vertexBuffer_ = std::move(newVertexBuffer);
    indexBuffer_ = std::move(newIndexBuffer);
    vertexArray_ = std::move(newVertexArray);

    initialized_ = true;

    return true;
}

bool ScreenSpaceOutlinePass::resize(
    const graphics::Extent2D extent
)
{
    if (extent.width == 0||
        extent.height ==0)
    {
        return false;
    }

    if (outlinedHdrColor_.isValid() &&
        framebuffer_.isValid() &&
        extent_.width == extent.width &&
        extent_.height == extent.height)
    {
        return true;
    }

    graphics::RenderTextureDesc textureDesc;
    textureDesc.extent = extent;
    textureDesc.format =
        graphics::RenderTextureFormat::RGBA16Float;
    textureDesc.sampled = true;
    textureDesc.debugName =
        "Outlined HDR Color";

    graphics::RenderTexture newOutlinedHdrColor =
        graphicsDevice_.createRenderTexture(textureDesc);

    if (!newOutlinedHdrColor.isValid())
        return false;

    const std::array<
        const graphics::RenderTexture*,
        1> colorTextures{
            &newOutlinedHdrColor
        };

    graphics::FramebufferDesc framebufferDesc;

    framebufferDesc.colorTextures =
        colorTextures;

    framebufferDesc.debugName =
        "Screen Space Outline Framebuffer";

    graphics::Framebuffer newFramebuffer =
        graphicsDevice_.createFramebuffer(
            framebufferDesc);

    if (!newFramebuffer.isValid())
    {
        return false;
    }

    outlinedHdrColor_ =
        std::move(newOutlinedHdrColor);

    framebuffer_ =
        std::move(newFramebuffer);

    extent_ = extent;

    return true;
}

bool ScreenSpaceOutlinePass::execute(
    FrameContext& frame)
{
    lastDrawCallCount_ = 0;

    if (!initialized_ ||
        frame.renderWorld == nullptr ||
        frame.hdrColor == nullptr ||
        frame.outlineMask == nullptr ||
        frame.depth == nullptr ||
        frame.normal == nullptr ||
        !frame.hdrColor->isValid() ||
        !frame.normal->isValid() ||
        !frame.outlineMask->isValid() ||
        !frame.depth->isValid() ||
        !outlinedHdrColor_.isValid() ||
        !framebuffer_.isValid())
    {
        return false;
    }

    frame.hdrColor->bind(0);
    frame.outlineMask->bind(1);
    frame.depth->bind(2);
    frame.normal->bind(3);

    const RenderView& view =
        frame.renderWorld->mainView;

    if (!shader_.setInt(
            "uHdrColor",
            0) ||
        !shader_.setInt(
            "uOutlineMask",
            1) ||
        !shader_.setInt(
            "uDepth",
            2) ||
        !shader_.setInt(
            "uNormal",
            3) ||
        !shader_.setInt(
            "uScreenOutlineEnabled",
            settings_.enabled ? 1 : 0) ||
        !shader_.setVec3(
            "uScreenOutlineColor",
            settings_.color.r,
            settings_.color.g,
            settings_.color.b) ||
        !shader_.setFloat(
            "uScreenOutlineWidth",
            settings_.width) ||
        !shader_.setFloat(
            "uDepthThreshold",
            settings_.depthThreshold) ||
        !shader_.setFloat(
            "uNearPlane",
            view.nearPlane) ||
        !shader_.setFloat(
            "uFarPlane",
            view.farPlane) ||
        !shader_.setFloat(
            "uNormalThreshold",
            settings_.normalThreshold))
    {
        return false;
    }

    graphicsDevice_.bindFramebuffer(
        &framebuffer_);

    graphicsDevice_.setViewport(
        extent_);

    graphics::DrawIndexedCommand command;

    command.shader =
        &shader_;

    command.vertexArray =
        &vertexArray_;

    command.topology =
        graphics::PrimitiveTopology::Triangles;

    command.indexType =
        graphics::IndexType::Uint16;

    command.indexCount = 3;
    command.firstIndex = 0;

    graphicsDevice_.drawIndexed(command);

    frame.hdrColor =
        &outlinedHdrColor_;

    frame.framebuffer =
        &framebuffer_;

    lastDrawCallCount_ = 1;

    return true;
}

std::string_view ScreenSpaceOutlinePass::name()
    const noexcept
{
    return "ScreenSpaceOutlinePass";
}

std::size_t
ScreenSpaceOutlinePass::lastDrawCallCount()
    const noexcept
{
    return lastDrawCallCount_;
}

void ScreenSpaceOutlinePass::setSettings(
    const ScreenSpaceOutlineSettings& settings) noexcept
{
    settings_ = settings;
}

const ScreenSpaceOutlineSettings&
ScreenSpaceOutlinePass::settings() const noexcept
{
    return settings_;
}

} // namespace stylized::render
