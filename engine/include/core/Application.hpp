#pragma once

#include <core/NonCopyable.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace stylized::platform { class Window; }
namespace stylized::graphics { class OpenGLContext; }
namespace stylized::graphics { class GraphicsDevice; }

namespace stylized::core
{

struct ApplicationDesc
{
    std::string title = "StylizedRenderer";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool visible = true;
    bool vsync = true;
};

class Application : public NonCopyable
{
public:
    explicit Application(const ApplicationDesc& desc = {});
    virtual ~Application();

    int run();

protected:
    virtual bool onInit() { return true; }
    virtual void onUpdate(float deltaTime) { (void)deltaTime; }
    virtual void onRender() {}
    virtual void onShutdown() {}

    [[nodiscard]] platform::Window& window();
    [[nodiscard]] graphics::GraphicsDevice& graphicsDevice();
    void requestExit();

private:
    using Clock = std::chrono::steady_clock;

    std::unique_ptr<platform::Window> window_;
    std::unique_ptr<graphics::OpenGLContext> glContext_;
    std::unique_ptr<graphics::GraphicsDevice> graphicsDevice_;
    Clock::time_point lastFrameTime_{};
};

} // namespace stylized::core
