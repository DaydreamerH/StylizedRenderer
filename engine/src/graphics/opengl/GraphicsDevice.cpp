#include <stylized/graphics/GraphicsDevice.hpp>
#include <stylized/graphics/OpenGLContext.hpp>

#include <glad/gl.h>

namespace stylized::graphics
{

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
    glClear(GL_COLOR_BUFFER_BIT);
}

Buffer GraphicsDevice::createBuffer(const BufferDesc &desc, std::span<const std::byte> initialData)
{
    if (!initialized_) return {};
    return Buffer{desc, initialData};
}

} // namespace stylized::graphics
