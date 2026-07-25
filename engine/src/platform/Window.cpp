#include <stylized/platform/Window.hpp>

#include <GLFW/glfw3.h>

#include <iostream>

namespace stylized::platform
{
namespace
{

void errorCallback(const int code, const char* description)
{
    std::cerr << "GLFW[" << code << "] " << description << '\n';
}

int toGlfwKey(const Key key)
{
    switch (key)
    {
    case Key::Escape:
        return GLFW_KEY_ESCAPE;
    }

    return GLFW_KEY_UNKNOWN;
}

} // namespace

Window::Window(const Desc& desc)
{
    glfwSetErrorCallback(errorCallback);

    if (glfwInit() != GLFW_TRUE)
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    if (!desc.visible)
    {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    window_ = glfwCreateWindow(
        static_cast<int>(desc.width),
        static_cast<int>(desc.height),
        desc.title.c_str(),
        nullptr,
        nullptr);

    if (window_ == nullptr)
    {
        std::cerr << "Failed to create GLFW window.\n";
    }
}

Window::~Window()
{
    if (window_ != nullptr)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

bool Window::isValid() const
{
    return window_ != nullptr;
}

bool Window::shouldClose() const
{
    if (window_ == nullptr)
    {
        return true;
    }
    return glfwWindowShouldClose(window_) != GLFW_FALSE;
}

void Window::swapBuffers()
{
    if (window_ != nullptr)
    {
        glfwSwapBuffers(window_);
    }
}

void Window::pollEvents()
{
    if (window_ != nullptr)
    {
        glfwPollEvents();
    }
}

void Window::waitEvents()
{
    if (window_ != nullptr)
    {
        glfwWaitEvents();
    }
}

void Window::getFramebufferSize(uint32_t& width, uint32_t& height) const
{
    if (window_ == nullptr)
    {
        width = 0;
        height = 0;
        return;
    }
    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    width = static_cast<uint32_t>(w);
    height = static_cast<uint32_t>(h);
}

void Window::setShouldClose(const bool value)
{
    if (window_ != nullptr)
    {
        glfwSetWindowShouldClose(window_, value ? GLFW_TRUE : GLFW_FALSE);
    }
}

bool Window::isKeyPressed(const Key key) const
{
    if (window_ == nullptr)
    {
        return false;
    }

    const int glfwKey = toGlfwKey(key);
    return glfwKey != GLFW_KEY_UNKNOWN &&
           glfwGetKey(window_, glfwKey) == GLFW_PRESS;
}

GLFWwindow* Window::nativeHandle() const
{
    return window_;
}

} // namespace stylized::platform
