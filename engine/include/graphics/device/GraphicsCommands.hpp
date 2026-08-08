#pragma once

#include <graphics/device/GraphicsTypes.hpp>

#include <cstdint>

namespace stylized::graphics
{

class ShaderProgram;
class VertexArray;

struct DrawIndexedCommand
{
    const ShaderProgram* shader = nullptr;
    const VertexArray* vertexArray = nullptr;

    PrimitiveTopology topology = PrimitiveTopology::Triangles;
    IndexType indexType = IndexType::Uint32;

    uint32_t indexCount = 0;
    uint32_t firstIndex = 0;
};

} // namespace stylized::graphics
