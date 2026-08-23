#pragma once

#include <asset/AssetHandle.hpp>
#include <core/NonCopyable.hpp>
#include <material/MaterialTemplate.hpp>

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>

struct GLFWwindow;

struct ViewerCpuTimings
{
    double frameIntervalMilliseconds = 0.0;
    double animationMilliseconds = 0.0;
    double skinningMilliseconds = 0.0;
    double morphMilliseconds = 0.0;
    double updateMilliseconds = 0.0;
    double extractionMilliseconds = 0.0;
    double pipelineMilliseconds = 0.0;
    double uiMilliseconds = 0.0;
    double renderMilliseconds = 0.0;
};

namespace stylized::asset
{
class AssetRegistry;
struct MaterialAsset;
struct SceneAsset;
}

namespace stylized::animation
{
class AnimationPlayer;
}

namespace stylized::render
{
class ForwardOpaquePass;
class ForwardTransparentPass;
class FramePipeline;
class OutlineMaskPass;
class PostProcessPass;
class RuntimeResourceCache;
class RuntimeMeshInstance;
class ScreenSpaceOutlinePass;
class ShadowPass;
class SkinningPaletteSet;
struct RenderWorld;
}

namespace stylized::scene
{
class Transform;
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
        std::span<const std::filesystem::path> modelPaths,
        std::size_t& selectedSceneIndex,
        stylized::scene::Transform& rootTransform,
        stylized::asset::AssetRegistry& assets,
        stylized::render::RuntimeResourceCache& resourceCache,
        stylized::asset::AssetHandle<
            stylized::material::MaterialTemplate>
            materialTemplate,
        const stylized::asset::SceneAsset* scene,
        stylized::animation::AnimationPlayer&
            animationPlayer,
        const stylized::render::SkinningPaletteSet&
            skinningPalettes,
        std::span<stylized::render::RuntimeMeshInstance>
            morphMeshInstances,
        stylized::render::RenderWorld& renderWorld,
        std::size_t drawCallCount,
        const ViewerCpuTimings& cpuTimings,
        const stylized::render::FramePipeline* framePipeline,
        const stylized::render::ShadowPass* shadowPass,
        const stylized::render::ForwardOpaquePass* forwardPass,
        const stylized::render::ForwardTransparentPass*
            transparentPass,
        stylized::render::OutlineMaskPass* outlineMaskPass,
        stylized::render::ScreenSpaceOutlinePass*
            screenSpaceOutlinePass,
        const stylized::render::PostProcessPass* postProcessPass,
        stylized::material::MaterialKind& materialKind,
        bool& shadowsEnabled,
        float& exposure,
        bool& toneMappingEnabled,
        bool& fxaaEnabled);

    void endFrame() noexcept;
    void shutdown() noexcept;

private:
    stylized::asset::AssetHandle<
        stylized::asset::MaterialAsset>
        selectedMaterial_;

    std::string materialSidecarStatus_;
    bool materialSidecarFailed_ = false;
    std::filesystem::path displayedModelPath_;
    bool pendingSidecarLoad_ = true;

    float sidebarWidth_ = 0.0F;
    bool sidebarResizing_ = false;
    bool sidebarExpanded_ = true;
    bool initialized_ = false;
};

