#pragma once

#include <stylized/core/NonCopyable.hpp>

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace stylized::platform
{

enum class Key : uint8_t
{
    Escape
};

class Window final : public core::NonCopyable
{
public:
    struct Desc
    {
        std::string title = "StylizedRenderer";
        uint32_t width = 1280;
        uint32_t height = 720;
        bool visible = true;
    };

    explicit Window(const Desc& desc);
    ~Window();

    Window(Window&&)            = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool shouldClose() const;
    void               swapBuffers();
    void               pollEvents();
    void               waitEvents();
    void               getFramebufferSize(uint32_t& width, uint32_t& height) const;
    void               setShouldClose(bool value);
    [[nodiscard]] bool isKeyPressed(Key key) const;
    [[nodiscard]] GLFWwindow* nativeHandle() const;

private:
    GLFWwindow* window_ = nullptr;
};

} // namespace stylized::platform
