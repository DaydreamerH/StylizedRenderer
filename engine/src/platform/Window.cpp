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

int toGlfwMouseButton(const MouseButton button)
{
    switch (button)
    {
    case MouseButton::Left:
        return GLFW_MOUSE_BUTTON_LEFT;
    
    case MouseButton::Middle:
        return GLFW_MOUSE_BUTTON_MIDDLE;
    
    case MouseButton::Right:
        return GLFW_MOUSE_BUTTON_RIGHT;
    }

    return -1;
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
        return;
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetScrollCallback(window_, &Window::scrollCallback);
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

bool Window::isMouseButtonPressed(const MouseButton button) const
{
    if (window_ == nullptr) return false;

    const int glfwButton = toGlfwMouseButton(button);

    return glfwButton >= 0 && glfwGetMouseButton(window_, glfwButton) == GLFW_PRESS;
}

void Window::getCursorPosition(double& x, double& y) const
{
    if (window_ == nullptr) 
    {
        x = 0.0;
        y = 0.0;
        return;
    }

    glfwGetCursorPos(window_, &x, &y);
}

double Window::consumeScrollDelta() noexcept
{
    const double result = scrollDelta_;
    scrollDelta_ = 0.0;

    return result;
}

void Window::scrollCallback(GLFWwindow *window, double, double yOffset)
{
    if (window == nullptr) return;

    auto* owner = static_cast<Window*>(glfwGetWindowUserPointer(window));

    if (owner != nullptr) 
        owner->scrollDelta_ += yOffset;
}

} // namespace stylized::platform
