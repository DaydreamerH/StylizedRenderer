#pragma once

#include <stylized/core/NonCopyable.hpp>
#include <stylized/graphics/Buffer.hpp>
#include <stylized/graphics/GraphicsTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace stylized::graphics
{

class GraphicsDevice;

struct VertexArrayDesc
{
    const Buffer* vertexBuffer = nullptr;
    const Buffer* indexBuffer = nullptr;

    VertexBufferBindingDesc vertexBinding;

    std::span<const VertexAttributeDesc> attributes;

    std::size_t vertexBufferOffset = 0;
    std::string debugName;
};

class VertexArray final : public core::NonCopyable
{
public:
    VertexArray() = default;

    VertexArray(VertexArray&& other) noexcept;
    VertexArray& operator=(VertexArray&& other) noexcept;

    ~VertexArray();

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool hasIndexBuffer() const noexcept;

private:
    friend class GraphicsDevice;

    explicit VertexArray(const VertexArrayDesc& desc);

    void release() noexcept;

    uint32_t id_ = 0;
    bool hasIndexBuffer_ = false;
};

} // namespace stylized::graphics