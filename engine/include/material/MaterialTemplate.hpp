#pragma once

#include <cstdint>
#include <string>

namespace stylized::material
{
    
enum class MaterialKind : std::uint8_t
{
    Unlit,
    DebugNormal,
    BasicPbr,
    MToon
};

struct MaterialTemplate
{
    std::string name;

    MaterialKind kind = MaterialKind::Unlit;

    std::string vertexShaderPath;
    std::string fragmentShaderPath;

    std::uint32_t variantKey = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !vertexShaderPath.empty() && !fragmentShaderPath.empty();
    }
};

} // namespace stylized::material
