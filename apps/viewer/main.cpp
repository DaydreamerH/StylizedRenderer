#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
constexpr int initialWidth = 1280;
constexpr int initialHeight = 720;

void glfwErrorCallback(const int errorCode, const char* description)
{
    std::cerr << "[GLFW " << errorCode << "] " << description << '\n';
}

void framebufferSizeCallback(GLFWwindow*, const int width, const int height)
{
    glViewport(0, 0, width, height);
}

void GLAD_API_PTR openGlDebugCallback(
    GLenum,
    GLenum type,
    GLuint,
    GLenum severity,
    GLsizei,
    const GLchar* message,
    const void*)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        return;
    }

    std::cerr << "[OpenGL";
    if (type == GL_DEBUG_TYPE_ERROR)
    {
        std::cerr << " error";
    }
    std::cerr << "] " << message << '\n';
}
}

int main(const int argc, char* argv[])
{
    const bool smokeTest =
        argc > 1 && std::string_view{argv[1]} == "--smoke-test";

    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() != GLFW_TRUE)
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
    if (smokeTest)
    {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    GLFWwindow* window = glfwCreateWindow(
        initialWidth,
        initialHeight,
        "StylizedRenderer",
        nullptr,
        nullptr);

    if (window == nullptr)
    {
        std::cerr << "Failed to create an OpenGL 4.5 window.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    const int loadedVersion = gladLoadGL(glfwGetProcAddress);
    if (loadedVersion == 0)
    {
        std::cerr << "Failed to load OpenGL functions.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

#ifndef NDEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(openGlDebugCallback, nullptr);
#endif

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    std::cout
        << "OpenGL " << GLAD_VERSION_MAJOR(loadedVersion) << '.'
        << GLAD_VERSION_MINOR(loadedVersion) << '\n'
        << "Vendor: " << glGetString(GL_VENDOR) << '\n'
        << "Renderer: " << glGetString(GL_RENDERER) << '\n'
        << "Version: " << glGetString(GL_VERSION) << '\n';

    int renderedFrameCount = 0;
    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        glClearColor(0.06F, 0.07F, 0.10F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();

        ++renderedFrameCount;
        if (smokeTest && renderedFrameCount >= 3)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
