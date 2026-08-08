#pragma once

#include <core/NonCopyable.hpp>

#include <graphics/Buffer.hpp>
#include <graphics/ShaderProgram.hpp>
#include <graphics/VertexArray.hpp>

#include <render/pipeline/IRenderPass.hpp>

#include <cstddef>
#include <string_view>

namespace stylized::graphics
{

class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{

class PostProcessPass final
    : public IRenderPass,
      public core::NonCopyable
{
public:
    explicit PostProcessPass(
        graphics::GraphicsDevice& graphicsDevice) noexcept;

    ~PostProcessPass() override = default;

    [[nodiscard]] bool initialize();

    [[nodiscard]] bool resize(
        graphics::Extent2D extent) override;

    [[nodiscard]] bool execute(
        FrameContext& frame) override;

    [[nodiscard]] std::string_view name()
        const noexcept override;

    [[nodiscard]] std::size_t
        lastDrawCallCount() const noexcept;

private:
    graphics::GraphicsDevice& graphicsDevice_;

    graphics::ShaderProgram shader_;

    graphics::Buffer vertexBuffer_;
    graphics::Buffer indexBuffer_;
    graphics::VertexArray vertexArray_;

    std::size_t lastDrawCallCount_ = 0;

    bool initialized_ = false;
};

} // namespace stylized::render
