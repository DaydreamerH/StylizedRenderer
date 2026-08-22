#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/device/GraphicsTypes.hpp>
#include <graphics/resources/Buffer.hpp>
#include <graphics/resources/VertexArray.hpp>
#include <graphics/resources/ShaderProgram.hpp>
#include <graphics/device/GraphicsCommands.hpp>
#include <graphics/resources/Texture2D.hpp>
#include <graphics/resources/RenderTexture.hpp>
#include <graphics/resources/DepthTexture.hpp>
#include <graphics/resources/Framebuffer.hpp>
#include <graphics/resources/GpuTimerQuery.hpp>

#include <cstddef>
#include <span>
#include <type_traits>

namespace stylized::graphics
{

class OpenGLContext;

class GraphicsDevice final : public core::NonCopyable
{
public:
    explicit GraphicsDevice(const OpenGLContext& context);
    ~GraphicsDevice();

    [[nodiscard]] bool isValid() const;

    void setViewport(const Extent2D& extent);
    void setCullMode(CullMode mode);
    void setDepthWrite(bool enabled);
    void setAlphaBlending(bool enabled);

    void clear(const ClearValue& value);
    void clearColorAttachment(std::uint32_t attachmentIndex, const ClearValue& value);

    [[nodiscard]] Buffer createBuffer(const BufferDesc& desc, std::span<const std::byte> initialData = {});
    template<typename T>
    [[nodiscard]] Buffer createBuffer(BufferDesc desc, const std::span<const T> initialData)
    {
        static_assert(std::is_trivially_copyable_v<T>, "GPU buffer elements must be trivially copyable.");

        if (desc.size == 0)
        {
            desc.size = initialData.size_bytes();
        }

        return createBuffer(desc, std::as_bytes(initialData));
    }

    [[nodiscard]] VertexArray createVertexArray(const VertexArrayDesc& desc);

    [[nodiscard]] ShaderProgram createShaderProgram(const ShaderProgramDesc& desc);

    [[nodiscard]] Texture2D createTexture2D(const Texture2DDesc& desc, std::span<const std::byte> pixels);
    template<typename T>
    [[nodiscard]] Texture2D createTexture2D(const Texture2DDesc& desc, const std::span<const T> pixels)
    {
        static_assert(std::is_trivially_copyable_v<T>, "Texture pixels must be trivially copyable.");

        return createTexture2D(desc, std::as_bytes(pixels));
    }

    [[nodiscard]] RenderTexture createRenderTexture(const RenderTextureDesc& desc);

    [[nodiscard]] DepthTexture createDepthTexture(const DepthTextureDesc& desc);

    [[nodiscard]] Framebuffer createFramebuffer(const FramebufferDesc& desc);
    [[nodiscard]] GpuTimerQuery createGpuTimerQuery();
    void bindFramebuffer(const Framebuffer* framebuffer);
    void blitColorToDefaultFramebuffer(
        const Framebuffer& source,
        Extent2D destinationExtent
    );

    void setPolygonOffset(
        bool enabled,
        float factor = 0.0F,
        float units = 0.0F
    );

    void setColorAttachmentWrite(
        std::uint32_t attachmentIndex,
        bool enabled);

    void clearDepth(float value = 1.0F);

    void drawIndexed(const DrawIndexedCommand& command);

private:
    bool initialized_ = false;
};

} // namespace stylized::graphics
