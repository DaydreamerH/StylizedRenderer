#include <render/passes/PostProcessPass.hpp>

#include <graphics/device/GraphicsCommands.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <graphics/resources/RenderTexture.hpp>

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

PostProcessPass::PostProcessPass(
    graphics::GraphicsDevice& graphicsDevice) noexcept
    : graphicsDevice_(graphicsDevice)
{
}

bool PostProcessPass::initialize()
{
    if (initialized_)
    {
        return true;
    }

    graphics::ShaderProgramDesc shaderDesc;

    shaderDesc.vertexShaderPath =
        "assets/shaders/postprocess/postprocess.vert";

    shaderDesc.fragmentShaderPath =
        "assets/shaders/postprocess/postprocess.frag";

    shaderDesc.debugName =
        "Post Process";

    graphics::ShaderProgram newShader =
        graphicsDevice_.createShaderProgram(
            shaderDesc);

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
        "Post Process Vertex Buffer";

    graphics::Buffer newVertexBuffer =
        graphicsDevice_.createBuffer(
            vertexBufferDesc,
            std::span<const FullscreenVertex>{
                vertices
            });

    if (!newVertexBuffer.isValid())
    {
        return false;
    }

    graphics::BufferDesc indexBufferDesc;
    indexBufferDesc.usage =
        graphics::BufferUsage::Static;
    indexBufferDesc.debugName =
        "Post Process Index Buffer";

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
        "Post Process Vertex Array";

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

bool PostProcessPass::resize(
    const graphics::Extent2D extent)
{
    if (extent.width == 0 || extent.height == 0)
    {
        return false;
    }

    if (ldrColor_.isValid() &&
        ldrColor_.extent().width == extent.width &&
        ldrColor_.extent().height == extent.height)
    {
        return true;
    }

    graphics::RenderTextureDesc colorDesc;
    colorDesc.extent = extent;
    colorDesc.format =
        graphics::RenderTextureFormat::RGBA8;
    colorDesc.debugName =
        "Post Process LDR Color";

    graphics::RenderTexture newLdrColor =
        graphicsDevice_.createRenderTexture(
            colorDesc);

    if (!newLdrColor.isValid())
    {
        return false;
    }

    const std::array<const graphics::RenderTexture*, 1>
        colorTextures{
            &newLdrColor
        };

    graphics::FramebufferDesc framebufferDesc;
    framebufferDesc.colorTextures = colorTextures;
    framebufferDesc.debugName =
        "Post Process Framebuffer";

    graphics::Framebuffer newFramebuffer =
        graphicsDevice_.createFramebuffer(
            framebufferDesc);

    if (!newFramebuffer.isValid())
    {
        return false;
    }

    ldrColor_ = std::move(newLdrColor);
    framebuffer_ = std::move(newFramebuffer);

    return true;
}

bool PostProcessPass::execute(
    FrameContext& frame)
{
    lastDrawCallCount_ = 0;

    if (!initialized_ ||
        frame.hdrColor == nullptr ||
        !frame.hdrColor->isValid() ||
        !ldrColor_.isValid() ||
        !framebuffer_.isValid() ||
        frame.framebufferSize.width == 0 ||
        frame.framebufferSize.height == 0)
    {
        return false;
    }

    graphicsDevice_.bindFramebuffer(
        &framebuffer_);

    graphicsDevice_.setViewport(
        frame.framebufferSize);

    graphicsDevice_.clearColorAttachment(
        0,
        graphics::ClearValue{
            0.0F,
            0.0F,
            0.0F,
            1.0F
        });

    frame.hdrColor->bind(0);

    if (!shader_.setInt(
            "uHdrColor",
            0))
    {
        return false;
    }

    if (!shader_.setFloat(
            "uExposure",
            frame.exposure))
    {
        return false;
    }

    if (!shader_.setInt(
            "uToneMappingEnabled",
            frame.toneMappingEnabled ? 1 : 0))
    {
        return false;
    }

    graphicsDevice_.setDepthTest(false);
    graphicsDevice_.setDepthWrite(false);
    graphicsDevice_.setCullMode(
        graphics::CullMode::None);
    graphicsDevice_.setAlphaBlending(false);
    graphicsDevice_.setColorAttachmentWrite(0, true);

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

    graphicsDevice_.setDepthTest(true);
    graphicsDevice_.setDepthWrite(true);
    graphicsDevice_.setCullMode(
        graphics::CullMode::Back);

    frame.ldrColor = &ldrColor_;
    frame.framebuffer = &framebuffer_;

    lastDrawCallCount_ = 1;

    return true;
}

std::string_view PostProcessPass::name()
    const noexcept
{
    return "PostProcessPass";
}

std::size_t PostProcessPass::lastDrawCallCount()
    const noexcept
{
    return lastDrawCallCount_;
}

} // namespace stylized::render
