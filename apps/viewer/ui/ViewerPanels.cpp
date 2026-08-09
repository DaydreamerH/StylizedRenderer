#include "ViewerPanels.hpp"

#include <asset/AssetRegistry.hpp>
#include <asset/SceneAsset.hpp>
#include <render/pipeline/FramePipeline.hpp>
#include <render/passes/ForwardOpaquePass.hpp>
#include <render/passes/PostProcessPass.hpp>
#include <render/world/RenderWorld.hpp>
#include <render/passes/ShadowPass.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <string>

namespace
{

const char* renderTextureFormatName(
    const stylized::graphics::RenderTextureFormat format) noexcept
{
    switch (format)
    {
    case stylized::graphics::RenderTextureFormat::RGBA8:
        return "RGBA8";

    case stylized::graphics::RenderTextureFormat::RGBA16Float:
        return "RGBA16F";
    }

    return "Unknown";
}

const char* depthTextureFormatName(
    const stylized::graphics::DepthTextureFormat format) noexcept
{
    switch (format)
    {
    case stylized::graphics::DepthTextureFormat::Depth24Stencil8:
        return "Depth24Stencil8";

    case stylized::graphics::DepthTextureFormat::Depth32Float:
        return "Depth32F";
    }

    return "Unknown";
}

void drawPassStatus(
    const char* name,
    const char* status,
    const std::size_t drawCallCount,
    const bool hasGpuTime,
    const double gpuTimeMilliseconds)
{
    if (hasGpuTime)
    {
        ImGui::Text(
            "%s: %s, Draws: %zu, GPU: %.3f ms",
            name,
            status,
            drawCallCount,
            gpuTimeMilliseconds);
    }
    else
    {
        ImGui::Text(
            "%s: %s, Draws: %zu, GPU: pending",
            name,
            status,
            drawCallCount);
    }
}

} // namespace

ViewerPanels::~ViewerPanels()
{
    shutdown();
}

bool ViewerPanels::initialize(GLFWwindow* window)
{
    if (initialized_)
    {
        return true;
    }

    if (window == nullptr)
    {
        return false;
    }

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
    {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 450"))
    {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    initialized_ = true;
    return true;
}

void ViewerPanels::beginFrame() noexcept
{
    if (!initialized_)
    {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ViewerPanels::draw(
    const std::filesystem::path& modelPath,
    const stylized::asset::AssetRegistry& assets,
    const stylized::asset::SceneAsset* scene,
    const stylized::render::RenderWorld& renderWorld,
    const std::size_t drawCallCount,
    const stylized::render::FramePipeline* framePipeline,
    const stylized::render::ShadowPass* shadowPass,
    const stylized::render::ForwardOpaquePass* forwardPass,
    const stylized::render::PostProcessPass* postProcessPass,
    stylized::material::MaterialKind& materialKind,
    bool& shadowsEnabled,
    float& exposure,
    bool& toneMappingEnabled) const
{
    if (!initialized_)
    {
        return;
    }

    ImGui::Begin("StylizedRenderer");

    ImGui::Text("Model: %s", modelPath.string().c_str());

    if (scene != nullptr)
    {
        ImGui::Text("Scene: %s", scene->name.c_str());
        ImGui::Text("Nodes: %zu", scene->nodes.size());
    }
    else
    {
        ImGui::TextUnformatted("Scene: not loaded");
    }

    int materialMode = 0;

    switch (materialKind)
    {
    case stylized::material::MaterialKind::Unlit:
        materialMode = 0;
        break;

    case stylized::material::MaterialKind::DebugNormal:
        materialMode = 1;
        break;

    case stylized::material::MaterialKind::BasicPbr:
        materialMode = 2;
        break;

    case stylized::material::MaterialKind::MToon:
        materialMode = 3;
        break;
    }

    constexpr const char* materialModes[] = {
        "Unlit",
        "Debug Normal",
        "Basic PBR",
        "MToon"
    };

    ImGui::Separator();

    if (ImGui::Combo(
            "Material Mode",
            &materialMode,
            materialModes,
            IM_ARRAYSIZE(materialModes)))
    {
        switch (materialMode)
        {
        case 0:
            materialKind =
                stylized::material::MaterialKind::Unlit;
            break;

        case 1:
            materialKind =
                stylized::material::MaterialKind::DebugNormal;
            break;

        case 2:
            materialKind =
                stylized::material::MaterialKind::BasicPbr;
            break;

        case 3:
            materialKind =
                stylized::material::MaterialKind::MToon;
            break;
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Lighting");

    ImGui::Checkbox(
        "Shadows",
        &shadowsEnabled);

    const stylized::render::DirectionalLightData& mainLight =
        renderWorld.mainView.mainLight;

    ImGui::Text(
        "Direction: (%.2f, %.2f, %.2f)",
        mainLight.direction.x,
        mainLight.direction.y,
        mainLight.direction.z);

    ImGui::Text(
        "Color: (%.2f, %.2f, %.2f)",
        mainLight.color.r,
        mainLight.color.g,
        mainLight.color.b);

    ImGui::Text(
        "Intensity: %.2f",
        mainLight.intensity);

    ImGui::Separator();
    ImGui::TextUnformatted("Post Process");

    ImGui::SliderFloat(
        "Exposure",
        &exposure,
        0.0F,
        5.0F,
        "%.2f");

    ImGui::Checkbox(
        "Tone Mapping",
        &toneMappingEnabled);

    ImGui::Separator();
    ImGui::TextUnformatted("Render Pipeline");

    drawPassStatus(
        "ShadowPass",
        shadowPass == nullptr
            ? "Unavailable"
            : shadowsEnabled
                ? framePipeline != nullptr &&
                        !framePipeline->passLastExecutionSucceeded(0)
                    ? "Failed"
                    : "Active"
                : "Disabled",
        shadowPass != nullptr
            ? shadowPass->lastDrawCallCount()
            : 0,
        framePipeline != nullptr &&
            framePipeline->passHasGpuTime(0),
        framePipeline != nullptr
            ? framePipeline->passGpuTimeMilliseconds(0)
            : 0.0);

    drawPassStatus(
        "ForwardOpaquePass",
        forwardPass == nullptr
            ? "Unavailable"
            : framePipeline != nullptr &&
                    !framePipeline->passLastExecutionSucceeded(1)
                ? "Failed"
                : "Active",
        forwardPass != nullptr
            ? forwardPass->lastDrawCallCount()
            : 0,
        framePipeline != nullptr &&
            framePipeline->passHasGpuTime(1),
        framePipeline != nullptr
            ? framePipeline->passGpuTimeMilliseconds(1)
            : 0.0);

    drawPassStatus(
        "PostProcessPass",
        postProcessPass == nullptr
            ? "Unavailable"
            : framePipeline != nullptr &&
                    !framePipeline->passLastExecutionSucceeded(2)
                ? "Failed"
                : "Active",
        postProcessPass != nullptr
            ? postProcessPass->lastDrawCallCount()
            : 0,
        framePipeline != nullptr &&
            framePipeline->passHasGpuTime(2),
        framePipeline != nullptr
            ? framePipeline->passGpuTimeMilliseconds(2)
            : 0.0);

    if (framePipeline != nullptr &&
        framePipeline->hasCompleteGpuTiming())
    {
        ImGui::Text(
            "Pipeline GPU: %.3f ms",
            framePipeline->totalGpuTimeMilliseconds());
    }
    else
    {
        ImGui::TextUnformatted(
            "Pipeline GPU: pending");
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Render Targets");

    if (shadowPass != nullptr &&
        shadowPass->hasShadowMap())
    {
        const stylized::graphics::Extent2D extent =
            shadowPass->shadowMapExtent();

        ImGui::Text(
            "Shadow Map: %u x %u, %s, Rebuilds: %zu",
            extent.width,
            extent.height,
            depthTextureFormatName(
                shadowPass->shadowMapFormat()),
            shadowPass->shadowMapRebuildCount());
    }
    else
    {
        ImGui::TextUnformatted(
            "Shadow Map: not created");
    }

    if (forwardPass != nullptr &&
        forwardPass->hasRenderTargets())
    {
        const stylized::graphics::Extent2D extent =
            forwardPass->renderTargetExtent();

        const std::size_t rebuildCount =
            forwardPass->renderTargetRebuildCount();

        ImGui::Text(
            "HDR Color: %u x %u, %s, Rebuilds: %zu",
            extent.width,
            extent.height,
            renderTextureFormatName(
                forwardPass->colorFormat()),
            rebuildCount);

        ImGui::Text(
            "Forward Depth: %u x %u, %s, Rebuilds: %zu",
            extent.width,
            extent.height,
            depthTextureFormatName(
                forwardPass->depthFormat()),
            rebuildCount);
    }
    else
    {
        ImGui::TextUnformatted(
            "Forward targets: not created");
    }

    ImGui::Separator();
    ImGui::Text("Assets: %zu", assets.size());
    ImGui::Text("Render Items: %zu", renderWorld.size());
    ImGui::Text("Total Items: %zu", renderWorld.renderStats.totalItems);
    ImGui::Text("Visible Items: %zu", renderWorld.renderStats.visibleItems);
    ImGui::Text("Culled Items: %zu", renderWorld.renderStats.culledItems);
    ImGui::Text("Draw Calls: %zu", drawCallCount);

    ImGui::Separator();
    ImGui::Text(
        "Camera: (%.2f, %.2f, %.2f)",
        renderWorld.mainView.cameraPosition.x,
        renderWorld.mainView.cameraPosition.y,
        renderWorld.mainView.cameraPosition.z);

    ImGui::End();
}

void ViewerPanels::endFrame() noexcept
{
    if (!initialized_)
    {
        return;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(
        ImGui::GetDrawData());
}

void ViewerPanels::shutdown() noexcept
{
    if (!initialized_)
    {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    initialized_ = false;
}

