#pragma once

#include <core/NonCopyable.hpp>

#include <graphics/resources/DepthTexture.hpp>
#include <graphics/resources/Framebuffer.hpp>
#include <graphics/resources/ShaderProgram.hpp>

#include <render/pipeline/IRenderPass.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace stylized::asset
{
class AssetRegistry;
} // namespace stylized::asset

namespace stylized::graphics
{
    
class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{

class RuntimeResourceCache;

enum class ShadowPassKind : std::uint8_t
{
    Main,
    FaceFiltered
};

class ShadowPass final : public IRenderPass, public core::NonCopyable
{
public:
    ShadowPass(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::AssetRegistry& assetRegistry,
        RuntimeResourceCache& resourceCache,
        ShadowPassKind kind = ShadowPassKind::Main) noexcept;

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

    [[nodiscard]] bool hasShadowMap() const noexcept;

    [[nodiscard]] graphics::Extent2D
        shadowMapExtent() const noexcept;

    [[nodiscard]] graphics::DepthTextureFormat
        shadowMapFormat() const noexcept;

    [[nodiscard]] std::size_t
        shadowMapRebuildCount() const noexcept;

private:
    [[nodiscard]] bool ensureResources(
        graphics::Extent2D extent);

    graphics::GraphicsDevice& graphicsDevice_;
    const asset::AssetRegistry& assetRegistry_;
    RuntimeResourceCache& resourceCache_;
    ShadowPassKind kind_ = ShadowPassKind::Main;

    graphics::ShaderProgram shader_;
    graphics::DepthTexture depth_;
    graphics::Framebuffer framebuffer_;

    graphics::Extent2D extent_{};

    std::size_t lastDrawCallCount_ = 0;
    std::size_t shadowMapRebuildCount_ = 0;

    bool initialized_ = false;
};

} // namespace stylized::render
