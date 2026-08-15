#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/resources/DepthTexture.hpp>
#include <graphics/resources/RenderTexture.hpp>

#include <cstdint>
#include <string>
#include <span>

namespace stylized::graphics
{
    
class GraphicsDevice;

struct FramebufferDesc
{
    std::span<const RenderTexture* const>
        colorTextures;
    const DepthTexture* depthTexture = nullptr;
    std::string debugName;
};

class Framebuffer final : public core::NonCopyable
{
public:
    Framebuffer() = default;

    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    ~Framebuffer();

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] Extent2D extent() const noexcept;

private:
    friend class GraphicsDevice;
    explicit Framebuffer(const FramebufferDesc& desc);

    void release() noexcept;

    std::uint32_t id_ = 0;
    Extent2D extent_{};
};

} // namespace stylized::graphics
