#pragma once

#include <core/NonCopyable.hpp>

namespace stylized::platform { class Window; }

namespace stylized::graphics
{

class OpenGLContext final : public core::NonCopyable
{
public:
    explicit OpenGLContext(platform::Window& window);
    ~OpenGLContext();

    [[nodiscard]] bool isValid() const;
    void setVSync(bool enabled);
    void printInfo() const;

private:
    bool initialized_ = false;
};

} // namespace stylized::graphics
