#pragma once

#include <core/NonCopyable.hpp>

#include <graphics/resources/Framebuffer.hpp>
#include <graphics/resources/RenderTexture.hpp>
#include <graphics/resources/ShaderProgram.hpp>

#include <render/pipeline/IRenderPass.hpp>

#include <cstddef>
#include <string_view>

namespace stylized::graphics
{

class DepthTexture;
class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{

class OutlineMaskPass final
    : public IRenderPass,
      public core::NonCopyable
{
public:
    explicit OutlineMaskPass(graphics::GraphicsDevice& graphicsDevice) noexcept;

    ~OutlineMaskPass() override = default;

    [[nodiscard]] bool initialize();

    [[nodiscard]] bool resize(graphics::Extent2D extent) override;

    [[nodiscard]] bool execute(FrameContext& frame) override;

    [[nodiscard]] std::string_view name() const noexcept override;

    [[nodiscard]] std::size_t lastDrawCallCount() const noexcept;

private:
    [[nodiscard]] bool ensureFramebuffer(
        const graphics::DepthTexture& depth
    );

    graphics::GraphicsDevice& graphicsDevice_;

    graphics::ShaderProgram shader_;
    graphics::RenderTexture outlineMask_;
    graphics::Framebuffer framebuffer_;

    graphics::Extent2D extent_{};

    std::size_t lastDrawCallCount_ = 0;

    bool initialized_ = false;
};

} // namespace stylized::render
