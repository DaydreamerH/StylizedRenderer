#include <graphics/GraphicsDevice.hpp>
#include <graphics/OpenGLContext.hpp>

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

} // namespace stylized::graphics
