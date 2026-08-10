#pragma once

#include <core/NonCopyable.hpp>
#include <material/MaterialTemplate.hpp>

#include <cstddef>
#include <filesystem>

struct GLFWwindow;

namespace stylized::asset
{
class AssetRegistry;
struct SceneAsset;
}

namespace stylized::render
{
class ForwardOpaquePass;
class FramePipeline;
class PostProcessPass;
class ShadowPass;
struct RenderWorld;
}

class ViewerPanels final : public stylized::core::NonCopyable
{
public:
    ViewerPanels() = default;
    ~ViewerPanels();

    [[nodiscard]] bool initialize(GLFWwindow* window);
    void beginFrame() noexcept;

    void draw(
        const std::filesystem::path& modelPath,
        const stylized::asset::AssetRegistry& assets,
        const stylized::asset::SceneAsset* scene,
        stylized::render::RenderWorld& renderWorld,
        std::size_t drawCallCount,
        const stylized::render::FramePipeline* framePipeline,
        const stylized::render::ShadowPass* shadowPass,
        const stylized::render::ForwardOpaquePass* forwardPass,
        const stylized::render::PostProcessPass* postProcessPass,
        stylized::material::MaterialKind& materialKind,
        bool& shadowsEnabled,
        float& exposure,
        bool& toneMappingEnabled) const;

    void endFrame() noexcept;
    void shutdown() noexcept;

private:
    bool initialized_ = false;
};

