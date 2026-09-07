#include <core/Application.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <graphics/device/OpenGLContext.hpp>
#include <platform/Window.hpp>

#include <iostream>

namespace stylized::core
{

Application::Application(const ApplicationDesc& desc)
{
    platform::Window::Desc windowDesc;
    windowDesc.title = desc.title;
    windowDesc.width = desc.width;
    windowDesc.height = desc.height;
    windowDesc.visible = desc.visible;
    windowDesc.borderlessFullscreen = desc.borderlessFullscreen;

    window_ = std::make_unique<platform::Window>(windowDesc);
    if (!window_->isValid())
    {
        std::cerr << "Application failed to create window.\n";
        return;
    }

    glContext_ = std::make_unique<graphics::OpenGLContext>(*window_);
    if (!glContext_->isValid())
    {
        std::cerr << "Application failed to create OpenGL context.\n";
        return;
    }

    glContext_->setVSync(desc.vsync);
    glContext_->printInfo();
    graphicsDevice_ = std::make_unique<graphics::GraphicsDevice>(*glContext_);
}

Application::~Application() = default;

platform::Window& Application::window()
{
    return *window_;
}

graphics::GraphicsDevice& Application::graphicsDevice()
{
    return *graphicsDevice_;
}

void Application::requestExit()
{
    if (window_)
    {
        window_->setShouldClose(true);
    }
}

int Application::run()
{
    if (!window_ || !window_->isValid() ||
        !glContext_ || !glContext_->isValid() ||
        !graphicsDevice_ || !graphicsDevice_->isValid())
    {
        return EXIT_FAILURE;
    }

    if (!onInit())
    {
        return EXIT_FAILURE;
    }

    lastFrameTime_ = Clock::now();

    while (!window_->shouldClose())
    {
        window_->pollEvents();

        uint32_t framebufferWidth = 0;
        uint32_t framebufferHeight = 0;
        window_->getFramebufferSize(framebufferWidth, framebufferHeight);
        if (framebufferWidth == 0 || framebufferHeight == 0)
        {
            if (window_->shouldClose())
            {
                break;
            }

            window_->waitEvents();
            lastFrameTime_ = Clock::now();
            continue;
        }

        const auto currentTime = Clock::now();
        const std::chrono::duration<float> elapsed = currentTime - lastFrameTime_;
        lastFrameTime_ = currentTime;

        graphicsDevice_->setViewport({framebufferWidth, framebufferHeight});
        onUpdate(elapsed.count());
        onRender();
        window_->swapBuffers();
    }

    onShutdown();
    return EXIT_SUCCESS;
}

} // namespace stylized::core
