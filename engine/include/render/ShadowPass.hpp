#pragma once

#include <core/NonCopyable.hpp>

#include <graphics/DepthTexture.hpp>
#include <graphics/Framebuffer.hpp>
#include <graphics/ShaderProgram.hpp>

#include <render/IRenderPass.hpp>

#include <cstddef>
#include <string_view>

namespace stylized::graphics
{
    
class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{

class ShadowPass final : public IRenderPass, public core::NonCopyable
{
public:
    explicit ShadowPass(
        graphics::GraphicsDevice& graphicsDevice) noexcept;

    ~ShadowPass() override = default;

    [[nodiscard]] bool initialize();

    [[nodiscard]] bool execute(
        FrameContext& frame) override;

    [[nodiscard]] bool resize(
        graphics::Extent2D extent) override;

    [[nodiscard]] std::string_view name()
        const noexcept override;

    [[nodiscard]] std::size_t
        lastDrawCallCount() const noexcept;

private:
    [[nodiscard]] bool ensureResources(
        graphics::Extent2D extent);

    graphics::GraphicsDevice& graphicsDevice_;

    graphics::ShaderProgram shader_;
    graphics::DepthTexture depth_;
    graphics::Framebuffer framebuffer_;

    graphics::Extent2D extent_{};

    std::size_t lastDrawCallCount_ = 0;

    bool initialized_ = false;
};

} // namespace stylized::render
