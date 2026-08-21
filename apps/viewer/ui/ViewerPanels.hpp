#pragma once

#include <asset/AssetHandle.hpp>
#include <core/NonCopyable.hpp>
#include <material/MaterialTemplate.hpp>

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>

struct GLFWwindow;

namespace stylized::asset
{
class AssetRegistry;
struct MaterialAsset;
struct SceneAsset;
}

namespace stylized::render
{
class ForwardOpaquePass;
class FramePipeline;
class OutlineMaskPass;
class PostProcessPass;
class RuntimeResourceCache;
class RuntimeMeshInstance;
class ScreenSpaceOutlinePass;
class ShadowPass;
struct RenderWorld;
}

class ViewerPanels final : public stylized::core::NonCopyable
{
public:
    ViewerPanels() = default;
    ~ViewerPanels();

    [[nodiscard]] bool initialize(GLFWwindow* window);
    [[nodiscard]] bool wantsMouseCapture() const noexcept;

    void beginFrame() noexcept;

    void draw(
        const std::filesystem::path& modelPath,
        stylized::asset::AssetRegistry& assets,
        stylized::render::RuntimeResourceCache& resourceCache,
        stylized::asset::AssetHandle<
            stylized::material::MaterialTemplate>
            materialTemplate,
        const stylized::asset::SceneAsset* scene,
        std::span<stylized::render::RuntimeMeshInstance>
            morphMeshInstances,
        stylized::render::RenderWorld& renderWorld,
        std::size_t drawCallCount,
        const stylized::render::FramePipeline* framePipeline,
        const stylized::render::ShadowPass* shadowPass,
        const stylized::render::ForwardOpaquePass* forwardPass,
        const stylized::render::OutlineMaskPass* outlineMaskPass,
        stylized::render::ScreenSpaceOutlinePass*
            screenSpaceOutlinePass,
        const stylized::render::PostProcessPass* postProcessPass,
        stylized::material::MaterialKind& materialKind,
        bool& shadowsEnabled,
        float& exposure,
        bool& toneMappingEnabled);

    void endFrame() noexcept;
    void shutdown() noexcept;

private:
    stylized::asset::AssetHandle<
        stylized::asset::MaterialAsset>
        selectedMaterial_;

    std::string materialSidecarStatus_;
    bool materialSidecarFailed_ = false;

    bool initialized_ = false;
};

