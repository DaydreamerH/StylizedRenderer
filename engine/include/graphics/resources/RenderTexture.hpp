#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/device/GraphicsTypes.hpp>

#include <cstdint>

namespace stylized::graphics
{
    
class GraphicsDevice;
class Framebuffer;

class RenderTexture final : public core::NonCopyable
{
public:
    RenderTexture() = default;

    RenderTexture(RenderTexture&& other) noexcept;
    RenderTexture& operator=(RenderTexture&& other) noexcept;

    ~RenderTexture();

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] Extent2D extent() const noexcept;
    [[nodiscard]] RenderTextureFormat format() const noexcept;

    void bind(std::uint32_t slot) const noexcept;

private:
    friend class GraphicsDevice;
    friend class Framebuffer;

    explicit RenderTexture(const RenderTextureDesc& desc);

    void release() noexcept;

    std::uint32_t id_ = 0;
    Extent2D extent_{};
    RenderTextureFormat format_ = RenderTextureFormat::RGBA16Float;
};

} // namespace stylized::graphics
