#pragma once

#include <stylized/core/NonCopyable.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include <glm/mat4x4.hpp>

namespace stylized::graphics
{

class GraphicsDevice;

struct ShaderProgramDesc
{
    std::filesystem::path vertexShaderPath;
    std::filesystem::path fragmentShaderPath;
    std::string debugName;
};

class ShaderProgram final : public core::NonCopyable
{
public:
    ShaderProgram() = default;
    
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    ~ShaderProgram();

    [[nodiscard]] bool isValid() const noexcept;

    bool reload(); // 重新读取文件、编译和链接

    void bind() const noexcept;

    bool setInt(std::string_view name, int32_t value);

    bool setFloat(std::string_view name, float value);

    bool setVec2 (std::string_view name, float x, float y);

    bool setVec3(std::string_view name, float x, float y, float z);

    bool setVec4(std::string_view name, float x, float y, float z, float w);

    bool setMat4(std::string_view name, const glm::mat4& value);

private:
    friend class GraphicsDevice;

    explicit ShaderProgram(const ShaderProgramDesc& desc);

    [[nodiscard]] int32_t uniformLocation(std::string_view name);

    void release() noexcept;

    uint32_t id_ = 0;

    ShaderProgramDesc desc_;

    std::unordered_map<std::string, int32_t> uniformLocationCache_;
};

}