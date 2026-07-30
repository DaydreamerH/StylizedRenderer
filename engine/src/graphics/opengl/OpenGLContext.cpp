#include <graphics/OpenGLContext.hpp>
#include <platform/Window.hpp>

#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <iostream>

namespace stylized::graphics
{
namespace
{
void GLAD_API_PTR debugCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei /*length*/,
    const GLchar* message,
    const void* /*userParam*/)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        return;
    }

    std::cerr << "[OpenGL"
              << " source=0x" << std::hex << source
              << " type=0x" << type;
    if (type == GL_DEBUG_TYPE_ERROR)
    {
        std::cerr << "(ERROR)";
    }
    std::cerr << " severity=0x" << severity
              << " id=" << std::dec << id
              << "] " << message << '\n';
}

} // namespace

OpenGLContext::OpenGLContext(platform::Window& window)
{
    if (!window.isValid())
    {
        std::cerr << "OpenGLContext creation failed: Window is invalid.\n";
        return;
    }

    glfwMakeContextCurrent(window.nativeHandle());

    const int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0)
    {
        std::cerr << "Failed to load OpenGL functions.\n";
        glfwMakeContextCurrent(nullptr);
        return;
    }

    if (!GLAD_GL_VERSION_4_5)
    {
        std::cerr
            << "OpenGL 4.5 is required, but the current context provides "
            << GLAD_VERSION_MAJOR(version) << '.'
            << GLAD_VERSION_MINOR(version) << ".\n";
        glfwMakeContextCurrent(nullptr);
        return;
    }

#ifndef NDEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(debugCallback, nullptr);
#endif

    initialized_ = true;
}

OpenGLContext::~OpenGLContext()
{
    if (initialized_)
    {
#ifndef NDEBUG
        glDebugMessageCallback(nullptr, nullptr);
#endif
        glfwMakeContextCurrent(nullptr);
        initialized_ = false;
    }
}

bool OpenGLContext::isValid() const
{
    return initialized_;
}

void OpenGLContext::setVSync(const bool enabled)
{
    if (initialized_)
    {
        glfwSwapInterval(enabled ? 1 : 0);
    }
}

void OpenGLContext::printInfo() const
{
    if (!initialized_)
    {
        return;
    }

    std::cout
        << "Vendor:   " << glGetString(GL_VENDOR) << '\n'
        << "Renderer: " << glGetString(GL_RENDERER) << '\n'
        << "Version:  " << glGetString(GL_VERSION) << '\n';
}

} // namespace stylized::graphics
