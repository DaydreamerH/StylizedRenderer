#pragma once

#include <stylized/core/NonCopyable.hpp>
#include <stylized/graphics/GraphicsTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace stylized::graphics
{

class GraphicsDevice;

class Texture2D final : public core::NonCopyable
{
public:
    Texture2D() = default;

    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    ~Texture2D();

    [[nodiscard]] bool isValid() const noexcept;

    [[nodiscard]] uint32_t width() const noexcept;
    [[nodiscard]] uint32_t height() const noexcept;
    [[nodiscard]] TextureFormat format() const noexcept;

    void bind(uint32_t slot) const noexcept;

private:
    friend class GraphicsDevice;

    explicit Texture2D(const Texture2DDesc& desc, std::span<const std::byte> pixels);

    void release() noexcept;

    uint32_t id_ = 0;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    TextureFormat format_ = TextureFormat::RGBA8;
};

} // namespace stylized::graphics