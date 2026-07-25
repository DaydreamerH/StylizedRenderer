#include <stylized/graphics/GraphicsDevice.hpp>
#include <stylized/graphics/OpenGLContext.hpp>
#include <stylized/platform/Window.hpp>

#include <cstdlib>
#include <string_view>

int main(const int argc, char* argv[])
{
    const bool smokeTest =
        argc > 1 && std::string_view{argv[1]} == "--smoke-test";

    stylized::platform::Window::Desc windowDesc;
    windowDesc.title = "StylizedRenderer";
    windowDesc.visible = !smokeTest;

    stylized::platform::Window window(windowDesc);
    if (!window.isValid())
    {
        return EXIT_FAILURE;
    }

    stylized::graphics::OpenGLContext context(window);
    if (!context.isValid())
    {
        return EXIT_FAILURE;
    }

    context.setVSync(!smokeTest);
    context.printInfo();

    stylized::graphics::GraphicsDevice graphicsDevice(context);
    if (!graphicsDevice.isValid())
    {
        return EXIT_FAILURE;
    }

    int renderedFrameCount = 0;
    while (!window.shouldClose())
    {
        window.pollEvents();
        if (window.isKeyPressed(stylized::platform::Key::Escape))
        {
            window.setShouldClose(true);
        }

        uint32_t framebufferWidth = 0;
        uint32_t framebufferHeight = 0;
        window.getFramebufferSize(framebufferWidth, framebufferHeight);
        if (framebufferWidth == 0 || framebufferHeight == 0)
        {
            window.waitEvents();
            continue;
        }

        graphicsDevice.setViewport({framebufferWidth, framebufferHeight});
        graphicsDevice.clear({0.06F, 0.07F, 0.10F, 1.0F});
        window.swapBuffers();

        ++renderedFrameCount;
        if (smokeTest && renderedFrameCount >= 3)
        {
            window.setShouldClose(true);
        }
    }

    return EXIT_SUCCESS;
}
