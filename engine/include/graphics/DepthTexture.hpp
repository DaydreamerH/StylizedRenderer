#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/GraphicsTypes.hpp>

#include <cstdint>

namespace stylized::graphics
{
    
class GraphicsDevice;
class Framebuffer;

class DepthTexture final : public core::NonCopyable
{
public:
    DepthTexture() = default;
 
    DepthTexture(DepthTexture&& other) noexcept;
    DepthTexture& operator=(DepthTexture&& other) noexcept;

    ~DepthTexture();

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] Extent2D extent() const noexcept;
    [[nodiscard]] DepthTextureFormat format() const noexcept;

    void bind(std::uint32_t slot) const noexcept;

private:
    friend class GraphicsDevice;
    friend class Framebuffer;

    explicit DepthTexture(const DepthTextureDesc& desc);

    void release() noexcept;

    std::uint32_t id_ = 0;
    Extent2D extent_{};

    DepthTextureFormat format_ = DepthTextureFormat::Depth24Stencil8;
};

} // namespace stylized::graphics
