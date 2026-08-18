#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/device/GraphicsTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace stylized::graphics
{

class GraphicsDevice;
class VertexArray;

class Buffer final : public core::NonCopyable
{
public:
    Buffer() = default;

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    ~Buffer();

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] BufferUsage usage() const noexcept;

    bool update(std::size_t offset, std::span<const std::byte> data);

    template<typename T>
    bool update(const std::size_t elementOffset, const std::span<const T> data)
    {
        static_assert(std::is_trivially_copyable_v<T>, "GPU buffer elements must be trivially copyable.");

        return update(elementOffset * sizeof(T), std::as_bytes(data));
    }

    void bindShaderStorage(
        std::uint32_t binding) const noexcept;

private:
    friend class GraphicsDevice;
    friend class VertexArray;

    explicit Buffer(const BufferDesc& desc, std::span<const std::byte> initialData);

    void release() noexcept;

    uint32_t id_ = 0;
    std::size_t size_ = 0;
    BufferUsage usage_ = BufferUsage::Static;

};

}
