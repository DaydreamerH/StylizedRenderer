#pragma once

#include <stylized/core/NonCopyable.hpp>
#include <stylized/graphics/GraphicsTypes.hpp>

namespace stylized::graphics
{

class OpenGLContext;

class GraphicsDevice final : public core::NonCopyable
{
public:
    explicit GraphicsDevice(const OpenGLContext& context);
    ~GraphicsDevice();

    [[nodiscard]] bool isValid() const;
    void setViewport(const Extent2D& extent);
    void clear(const ClearValue& value);

private:
    bool initialized_ = false;
};

} // namespace stylized::graphics
