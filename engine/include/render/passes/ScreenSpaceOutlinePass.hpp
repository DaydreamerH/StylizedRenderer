#pragma once

#include <core/NonCopyable.hpp>

#include <graphics/resources/Buffer.hpp>
#include <graphics/resources/Framebuffer.hpp>
#include <graphics/resources/RenderTexture.hpp>
#include <graphics/resources/ShaderProgram.hpp>
#include <graphics/resources/VertexArray.hpp>

#include <render/pipeline/IRenderPass.hpp>

#include <glm/vec3.hpp>

#include <cstddef>
#include <string_view>

namespace stylized::graphics
{

class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{

struct ScreenSpaceOutlineSettings
{
    bool enabled = false;

    glm::vec3 color{0.0F};

    float width = 1.0F;
    float depthThreshold = 0.01F;
    float normalThreshold = 0.2F;
};


class ScreenSpaceOutlinePass final
    : public IRenderPass,
      public core::NonCopyable
{
public:
    explicit ScreenSpaceOutlinePass(
        graphics::GraphicsDevice& graphicsDevice) noexcept;

    ~ScreenSpaceOutlinePass() override = default;

    [[nodiscard]] bool initialize();

    [[nodiscard]] bool resize(
        graphics::Extent2D extent) override;

    [[nodiscard]] bool execute(
        FrameContext& frame) override;

    [[nodiscard]] std::string_view name()
        const noexcept override;

    [[nodiscard]] std::size_t
        lastDrawCallCount() const noexcept;

    [[nodiscard]] bool hasRenderTarget() const noexcept;

    [[nodiscard]] graphics::Extent2D
        renderTargetExtent() const noexcept;

    [[nodiscard]] graphics::RenderTextureFormat
        renderTargetFormat() const noexcept;

    [[nodiscard]] std::size_t
        renderTargetRebuildCount() const noexcept;

    void setSettings(
        const ScreenSpaceOutlineSettings& settings) noexcept;

    [[nodiscard]] const ScreenSpaceOutlineSettings&
        settings() const noexcept;

private:
    graphics::GraphicsDevice& graphicsDevice_;

    graphics::ShaderProgram shader_;

    graphics::Buffer vertexBuffer_;
    graphics::Buffer indexBuffer_;
    graphics::VertexArray vertexArray_;

    graphics::RenderTexture outlinedHdrColor_;
    graphics::Framebuffer framebuffer_;

    graphics::Extent2D extent_{};

    ScreenSpaceOutlineSettings settings_;

    std::size_t lastDrawCallCount_ = 0;
    std::size_t renderTargetRebuildCount_ = 0;

    bool initialized_ = false;
};

} // namespace stylized::render
