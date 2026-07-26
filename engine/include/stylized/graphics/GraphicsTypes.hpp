#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace stylized::graphics
{

enum class BufferUsage : uint8_t
{
    Static,
    Dynamic
};

struct BufferDesc
{
    std::size_t size = 0;
    BufferUsage usage = BufferUsage::Static;
    std::string debugName;
};

enum class VertexAttributeFormat : uint8_t
{
    Float,
    Float2,
    Float3,
    Float4,
    Uint8Normalized4
};

struct VertexAttributeDesc
{
    uint32_t location = 0;
    uint32_t binding = 0;
    VertexAttributeFormat format = VertexAttributeFormat::Float3;
    std::size_t offset = 0;
};

struct VertexBufferBindingDesc
{
    uint32_t binding = 0;
    std::size_t stride = 0;
};

enum class TextureFormat : uint8_t
{
    R8,
    RGB8,
    RGBA8
};

enum class TextureWrap : uint8_t
{
    Repeat,
    ClampToEdge
};

enum class TextureFilter : uint8_t
{
    Nearest,
    Linear
};

enum class PrimitiveTopology : uint8_t
{
    Triangles
};

enum class IndexType : uint8_t
{
    Uint16,
    Uint32
};

struct ClearValue
{
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
};

struct Extent2D
{
    uint32_t width = 0;
    uint32_t height = 0;
};

} // namespace stylized::graphics
