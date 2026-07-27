#include <stylized/graphics/ShaderProgram.hpp>

#include <glad/gl.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace stylized::graphics
{

namespace
{

std::optional<std::string> readTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        std::cerr << "Failed to open shader file:" << path.string() << '\n';

        return std::nullopt;
    }

    std::string source{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

    if (!file.eof() && file.fail())
    {
        std::cerr << "Failed while reading shader file:" << path.string() << '\n';

        return std::nullopt;
    }

    return source;
}    

const char* shaderStageName(const GLenum stage) noexcept
{
    switch (stage)
    {
    case GL_VERTEX_SHADER:
        return "Vertex";
    case GL_FRAGMENT_SHADER:
        return "Fragment";
    }

    return "Unkonwn";
}

void setObjectLabel(const GLenum objectType, const GLuint object, const std::string& label)
{
#ifndef NDEBUG
    if (object == 0 || label.empty())
    {
        return;
    }

    if (label.size() >
        static_cast<std::size_t>(
            std::numeric_limits<GLsizei>::max()))
    {
        std::cerr
            << "OpenGL object label is too long: "
            << label
            << '\n';

        return;
    }

    glObjectLabel(
        objectType,
        object,
        static_cast<GLsizei>(label.size()),
        label.c_str());
#else
    (void)objectType;
    (void)object;
    (void)label;
#endif
}

void printShaderCompileLog(const GLuint shader, const GLenum stage, const std::filesystem::path& path)
{
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    std::vector<GLchar> log(logLength > 0 ? static_cast<std::size_t>(logLength) : 1);

     GLsizei writtenLength = 0;

    glGetShaderInfoLog(
        shader,
        static_cast<GLsizei>(log.size()),
        &writtenLength,
        log.data());

    std::cerr
        << shaderStageName(stage)
        << " shader compilation failed.\n"
        << "File: "
        << path.string()
        << '\n';

    if (writtenLength > 0)
    {
        std::cerr
            << "OpenGL log:\n"
            << log.data()
            << '\n';
    }
}

void printProgramLinkLog(
    const GLuint program,
    const ShaderProgramDesc& desc)
{
    GLint logLength = 0;
    glGetProgramiv(
        program,
        GL_INFO_LOG_LENGTH,
        &logLength);

    std::vector<GLchar> log(
        logLength > 0
            ? static_cast<std::size_t>(logLength)
            : 1);

    GLsizei writtenLength = 0;

    glGetProgramInfoLog(
        program,
        static_cast<GLsizei>(log.size()),
        &writtenLength,
        log.data());

    std::cerr
        << "Shader program linking failed.\n"
        << "Vertex file: "
        << desc.vertexShaderPath.string()
        << '\n'
        << "Fragment file: "
        << desc.fragmentShaderPath.string()
        << '\n';

    if (writtenLength > 0)
    {
        std::cerr
            << "OpenGL log:\n"
            << log.data()
            << '\n';
    }
}

GLuint compileShader(const GLenum stage, const std::filesystem::path& path, const std::string& debugName)
{
    const std::optional<std::string> source = readTextFile(path);

    if (!source) return 0;

    if (source->size() > static_cast<std::size_t>(std::numeric_limits<GLint>::max()))
    {
         std::cerr
            << shaderStageName(stage)
            << " shader source is too large: "
            << path.string()
            << '\n';

        return 0;
    }

    const GLuint shader = glCreateShader(stage);

    if (shader == 0)
    {
        std::cerr
            << "OpenGL failed to create a "
            << shaderStageName(stage)
            << " shader.\n";

        return 0;
    }

    const GLchar* sourceData = source->data();
    const GLint sourceLength = static_cast<GLint>(source->size());

    glShaderSource(shader, 1, &sourceData, &sourceLength);

    glCompileShader(shader);

    GLint compileStatus = GL_FALSE;

    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);

    if (compileStatus != GL_TRUE)
    {
        printShaderCompileLog(
            shader,
            stage,
            path);

        glDeleteShader(shader);
        return 0;
    }

    setObjectLabel(
        GL_SHADER,
        shader,
        debugName + " " +
            shaderStageName(stage) +
            " Shader");

    return shader;
}

GLuint buildProgram(const ShaderProgramDesc& desc)
{
    const GLuint vertexShader =
        compileShader(
            GL_VERTEX_SHADER,
            desc.vertexShaderPath,
            desc.debugName);

    if (vertexShader == 0)
    {
        return 0;
    }

    const GLuint fragmentShader =
        compileShader(
            GL_FRAGMENT_SHADER,
            desc.fragmentShaderPath,
            desc.debugName);

    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);
        return 0;
    }

    const GLuint program = glCreateProgram();

    if (program == 0)
    {
        std::cerr
            << "OpenGL failed to create a shader program.\n";

        glDeleteShader(fragmentShader);
        glDeleteShader(vertexShader);

        return 0;
    }

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);

    /*
     * 链接完成后，Program 已经拥有所需的可执行代码。
     * Shader 对象可以分离并删除。
     */
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linkStatus = GL_FALSE;

    glGetProgramiv(
        program,
        GL_LINK_STATUS,
        &linkStatus);

    if (linkStatus != GL_TRUE)
    {
        printProgramLinkLog(program, desc);

        glDeleteProgram(program);
        return 0;
    }

    setObjectLabel(
        GL_PROGRAM,
        program,
        desc.debugName);

    return program;
}

} // namespace

ShaderProgram::ShaderProgram(const ShaderProgramDesc& desc) 
    : desc_(desc)
{
    reload();
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : id_(std::exchange(other.id_, 0)), desc_(std::move(other.desc_)), uniformLocationCache_(std::move(other.uniformLocationCache_))
{
}

ShaderProgram& ShaderProgram::operator=(
    ShaderProgram&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    release();

    id_ = std::exchange(other.id_, 0);
    desc_ = std::move(other.desc_);

    uniformLocationCache_ =
        std::move(other.uniformLocationCache_);

    return *this;
}

ShaderProgram::~ShaderProgram()
{
    release();
}

bool ShaderProgram::isValid() const noexcept
{
    return id_ != 0;
}

bool ShaderProgram::reload()
{
    const GLuint newProgram =
        buildProgram(desc_);

    if (newProgram == 0)
    {
        std::cerr
            << "Shader reload failed; "
            << "the previous program remains active.\n";

        return false;
    }

    if (id_ != 0)
    {
        glDeleteProgram(id_);
    }

    id_ = newProgram;
    uniformLocationCache_.clear();

    std::cout
        << "Shader program loaded successfully: "
        << desc_.debugName
        << '\n';

    return true;
}

void ShaderProgram::bind() const noexcept
{
    glUseProgram(id_);
}

bool ShaderProgram::setInt(
    const std::string_view name,
    const int32_t value)
{
    const int32_t location =
        uniformLocation(name);

    if (location < 0)
    {
        return false;
    }

    glProgramUniform1i(
        id_,
        location,
        value);

    return true;
}

bool ShaderProgram::setFloat(
    const std::string_view name,
    const float value)
{
    const int32_t location =
        uniformLocation(name);

    if (location < 0)
    {
        return false;
    }

    glProgramUniform1f(
        id_,
        location,
        value);

    return true;
}

bool ShaderProgram::setVec2(
    const std::string_view name,
    const float x,
    const float y)
{
    const int32_t location =
        uniformLocation(name);

    if (location < 0)
    {
        return false;
    }

    glProgramUniform2f(
        id_,
        location,
        x,
        y);

    return true;
}

bool ShaderProgram::setVec3(
    const std::string_view name,
    const float x,
    const float y,
    const float z)
{
    const int32_t location =
        uniformLocation(name);

    if (location < 0)
    {
        return false;
    }

    glProgramUniform3f(
        id_,
        location,
        x,
        y,
        z);

    return true;
}

bool ShaderProgram::setVec4(
    const std::string_view name,
    const float x,
    const float y,
    const float z,
    const float w)
{
    const int32_t location =
        uniformLocation(name);

    if (location < 0)
    {
        return false;
    }

    glProgramUniform4f(
        id_,
        location,
        x,
        y,
        z,
        w);

    return true;
}

int32_t ShaderProgram::uniformLocation(
    const std::string_view name)
{
    if (!isValid() || name.empty())
    {
        return -1;
    }

    const std::string key{name};

    const auto cached =
        uniformLocationCache_.find(key);

    if (cached != uniformLocationCache_.end())
    {
        return cached->second;
    }

    const GLint location =
        glGetUniformLocation(
            id_,
            key.c_str());

    uniformLocationCache_.emplace(
        key,
        location);

    return location;
}

void ShaderProgram::release() noexcept
{
    if (id_ != 0)
    {
        glDeleteProgram(id_);
        id_ = 0;
    }

    uniformLocationCache_.clear();
}

} // namespace stylized::graphics