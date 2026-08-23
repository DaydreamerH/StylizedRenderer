#include <graphics/device/GraphicsDevice.hpp>
#include <graphics/device/OpenGLContext.hpp>

#include <glad/gl.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <iostream>

namespace stylized::graphics
{

namespace
{

GLenum toOpenGLTopology(
    const PrimitiveTopology topology) noexcept
{
    switch (topology)
    {
    case PrimitiveTopology::Triangles:
        return GL_TRIANGLES;
    }

    return GL_TRIANGLES;
}

GLenum toOpenGLIndexType(
    const IndexType indexType) noexcept
{
    switch (indexType)
    {
    case IndexType::Uint16:
        return GL_UNSIGNED_SHORT;

    case IndexType::Uint32:
        return GL_UNSIGNED_INT;
    }

    return GL_UNSIGNED_INT;
}

std::size_t indexTypeSize(
    const IndexType indexType) noexcept
{
    switch (indexType)
    {
    case IndexType::Uint16:
        return sizeof(uint16_t);

    case IndexType::Uint32:
        return sizeof(uint32_t);
    }

    return 0;
}

GLenum toOpenGLCullFace(const CullMode mode) noexcept
{
    switch (mode)
    {
    case CullMode::Front:
        return GL_FRONT;
    case CullMode::Back:
        return GL_BACK;
    case CullMode::None:
        break;
    }
    return GL_BACK;
}

} // namespace

GraphicsDevice::GraphicsDevice(const OpenGLContext& context)
    : initialized_(context.isValid())
{
}

GraphicsDevice::~GraphicsDevice() = default;

bool GraphicsDevice::isValid() const
{
    return initialized_;
}

void GraphicsDevice::setViewport(const Extent2D& extent)
{
    if (!initialized_)
    {
        return;
    }

    glViewport(
        0,
        0,
        static_cast<GLsizei>(extent.width),
        static_cast<GLsizei>(extent.height));
}


void GraphicsDevice::setCullMode(const CullMode mode)
{
    if (!initialized_)
        return;

    if (mode == CullMode::None)
    {
        glDisable(GL_CULL_FACE);
        return;
    }

    glEnable(GL_CULL_FACE);
    glCullFace(toOpenGLCullFace(mode));
}

void GraphicsDevice::setDepthTest(const bool enabled)
{
    if (!initialized_)
    {
        return;
    }

    if (enabled)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
}

void GraphicsDevice::setDepthWrite(const bool enabled)
{
    if (!initialized_)
        return;

    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void GraphicsDevice::setAlphaBlending(
    const bool enabled
)
{
    if (!initialized_)
    {
        return;
    }

    if (!enabled)
    {
        glDisable(GL_BLEND);
        return;
    }

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);

    glBlendFuncSeparate(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA,
        GL_ONE,
        GL_ONE_MINUS_SRC_ALPHA);
}

void GraphicsDevice::clear(const ClearValue& value)
{
    if (!initialized_)
    {
        return;
    }

    glClearColor(value.r, value.g, value.b, value.a);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GraphicsDevice::clearColorAttachment(
    const std::uint32_t attachmentIndex,
    const ClearValue& value
)
{
    if (!initialized_ ||
        attachmentIndex > static_cast<std::uint32_t>(
            std::numeric_limits<GLint>::max()
        ))
    {
        return;
    }

    const GLfloat color [] =
    {
        value.r,
        value.g,
        value.b,
        value.a
    };

    glClearBufferfv(GL_COLOR, static_cast<GLint>(attachmentIndex), color);
}

Buffer GraphicsDevice::createBuffer(const BufferDesc &desc, std::span<const std::byte> initialData)
{
    if (!initialized_) return {};
    return Buffer{desc, initialData};
}

VertexArray GraphicsDevice::createVertexArray(const VertexArrayDesc &desc)
{
    if (!initialized_)
    {
        return {};
    }

    return VertexArray{desc};
}

ShaderProgram GraphicsDevice::createShaderProgram(const ShaderProgramDesc &desc)
{
    if (!initialized_) return{};

    return ShaderProgram{desc};
}

void GraphicsDevice::setPolygonOffset(
    const bool enabled,
    const float factor,
    const float units)
{
    if (!enabled)
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
        return;
    }

    glEnable(GL_POLYGON_OFFSET_FILL);

    glPolygonOffset(factor, units);
}

void GraphicsDevice::setColorAttachmentWrite(
    const std::uint32_t attachmentIndex,
    const bool enabled)
{
    if (!initialized_)
    {
        return;
    }

    const GLboolean writeEnabled =
        enabled ? GL_TRUE : GL_FALSE;

    glColorMaski(
        static_cast<GLuint>(attachmentIndex),
        writeEnabled,
        writeEnabled,
        writeEnabled,
        writeEnabled);
}

void GraphicsDevice::clearDepth(const float value)
{
    if (!initialized_) return;

    glClearDepth(
        static_cast<GLdouble>(value)
    );

    glClear(GL_DEPTH_BUFFER_BIT);
}

void GraphicsDevice::drawIndexed(const DrawIndexedCommand& command)
{
    if (!initialized_) return;

    if (command.shader == nullptr || !command.shader->isValid()) return;

    if (command.vertexArray == nullptr || !command.vertexArray->isValid() || !command.vertexArray->hasIndexBuffer())
    {
        return;
    }

    if (command.indexCount == 0)
    {
        return;
    }

    if (command.indexCount >
        static_cast<uint32_t>(
            std::numeric_limits<GLsizei>::max()))
    {
        return;
    }

    const std::size_t elementSize =
        indexTypeSize(command.indexType);

    if (elementSize == 0)
    {
        return;
    }

    if (command.firstIndex >
        std::numeric_limits<std::size_t>::max() /
            elementSize)
    {
        return;
    }

    const std::size_t byteOffset =
        static_cast<std::size_t>(command.firstIndex) *
        elementSize;

    glUseProgram(command.shader->id_);
    glBindVertexArray(command.vertexArray->id_);

    glDrawElements(
        toOpenGLTopology(command.topology),
        static_cast<GLsizei>(command.indexCount),
        toOpenGLIndexType(command.indexType),
        reinterpret_cast<const void*>(byteOffset));
}

Texture2D GraphicsDevice::createTexture2D(const Texture2DDesc& desc, const std::span<const std::byte> pixels)
{
    if (!initialized_) return {};

    return Texture2D{desc, pixels};
}

RenderTexture GraphicsDevice::createRenderTexture(const RenderTextureDesc& desc)
{
    if (!initialized_) return {};

    return RenderTexture{desc};
}

DepthTexture GraphicsDevice::createDepthTexture(const DepthTextureDesc& desc)
{
    if (!initialized_) return {};

    return DepthTexture{desc};
}

Framebuffer GraphicsDevice::createFramebuffer(const FramebufferDesc& desc)
{
    if (!initialized_) return {};

    return Framebuffer{desc};
}

GpuTimerQuery GraphicsDevice::createGpuTimerQuery()
{
    if (!initialized_) return {};

    return GpuTimerQuery{*this};
}

void GraphicsDevice::bindFramebuffer(const Framebuffer* framebuffer)
{
    if (!initialized_) return;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer != nullptr ? framebuffer->id_ : 0);
}

void GraphicsDevice::blitColorToDefaultFramebuffer(
    const Framebuffer& source,
    const Extent2D destinationExtent)
{
    if (!initialized_ ||
        !source.isValid() ||
        destinationExtent.width == 0 ||
        destinationExtent.height == 0)
    {
        return;
    }

    const Extent2D sourceExtent = source.extent();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, source.id_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    glBlitFramebuffer(0, 0,
        static_cast<GLint>(sourceExtent.width), static_cast<GLint>(sourceExtent.height),
        0, 0,
        static_cast<GLint>(destinationExtent.width), static_cast<GLint>(destinationExtent.height),
        GL_COLOR_BUFFER_BIT,
        GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace stylized::graphics
