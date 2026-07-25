#pragma once

#include <cstdint>

namespace stylized::graphics
{

enum class BufferUsage : uint8_t
{
    Static,
    Dynamic
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
