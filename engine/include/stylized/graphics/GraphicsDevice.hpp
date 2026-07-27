#pragma once

#include <stylized/core/NonCopyable.hpp>
#include <stylized/graphics/GraphicsTypes.hpp>
#include <stylized/graphics/Buffer.hpp>
#include <stylized/graphics/VertexArray.hpp>
#include <stylized/graphics/ShaderProgram.hpp>
#include <stylized/graphics/GraphicsCommands.hpp>

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
    void clear(const ClearValue& value);

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

    void drawIndexed(const DrawIndexedCommand& command);

private:
    bool initialized_ = false;
};

} // namespace stylized::graphics
