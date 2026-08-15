#include "ViewerPanels.hpp"

#include <asset/AssetRegistry.hpp>
#include <asset/MaterialAsset.hpp>
#include <asset/MeshAsset.hpp>
#include <asset/SceneAsset.hpp>
#include <asset/TextureAsset.hpp>
#include <render/pipeline/FramePipeline.hpp>
#include <render/passes/ForwardOpaquePass.hpp>
#include <render/passes/OutlineMaskPass.hpp>
#include <render/passes/PostProcessPass.hpp>
#include <render/passes/ScreenSpaceOutlinePass.hpp>
#include <render/world/RenderWorld.hpp>
#include <render/passes/ShadowPass.hpp>
#include <render/resources/RuntimeResourceCache.hpp>

#include <material/MaterialInstance.hpp>
#include <material/mtoon/MToonMaterialSidecar.hpp>
#include <material/mtoon/MToonMaterialSidecarSerializer.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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

std::vector<
    stylized::asset::AssetHandle<
        stylized::asset::MaterialAsset>>
collectMaterialHandles(
    const stylized::asset::AssetRegistry& assets,
    const stylized::asset::SceneAsset* scene)
{
    using MaterialHandle =
        stylized::asset::AssetHandle<
            stylized::asset::MaterialAsset>;

    std::vector<MaterialHandle> handles;

    if (scene == nullptr)
    {
        return handles;
    }

    for (const stylized::asset::SceneNodeAsset& node :
         scene->nodes)
    {
        const stylized::asset::MeshAsset* mesh =
            assets.get(node.mesh);

        if (mesh == nullptr)
        {
            continue;
        }

        for (const stylized::asset::MeshPrimitiveAsset& primitive :
             mesh->primitives)
        {
            const MaterialHandle handle =
                primitive.material;

            if (handle.isNull())
            {
                continue;
            }

            const auto existing =
                std::find(
                    handles.begin(),
                    handles.end(),
                    handle);

            if (existing == handles.end())
            {
                handles.push_back(handle);
            }
        }
    }

    return handles;
}

std::filesystem::path makeMaterialSidecarPath(
    const std::filesystem::path& modelPath)
{
    std::filesystem::path sidecarPath =
        modelPath;

    sidecarPath.replace_extension(
        ".mtoon.json");

    return sidecarPath;
}

void setSidecarError(
    stylized::material::MToonSidecarError& error,
    const std::string& material,
    const std::string& field,
    const std::string& message)
{
    error.material = material;
    error.field = field;
    error.message = message;
}

std::string formatSidecarError(
    const stylized::material::MToonSidecarError& error)
{
    std::string message = "Failed";

    if (!error.material.empty())
    {
        message += ": material=" +
            error.material;
    }

    if (!error.field.empty())
    {
        message += ", field=" +
            error.field;
    }

    if (!error.message.empty())
    {
        message += ", reason=" +
            error.message;
    }

    return message;
}

bool saveMaterialSidecar(
    const std::filesystem::path& modelPath,
    const std::vector<
        stylized::asset::AssetHandle<
            stylized::asset::MaterialAsset>>& materialHandles,
    const stylized::asset::AssetHandle<
        stylized::material::MaterialTemplate> materialTemplate,
    stylized::asset::AssetRegistry& assets,
    stylized::render::RuntimeResourceCache& resourceCache,
    stylized::material::MToonSidecarError& error)
{
    const std::filesystem::path sidecarPath =
        makeMaterialSidecarPath(modelPath);

    const std::filesystem::path sidecarDirectory =
        sidecarPath.parent_path();

    stylized::material::MToonMaterialSidecar sidecar;

    for (const auto handle : materialHandles)
    {
        const stylized::asset::MaterialAsset* material =
            assets.get(handle);

        if (material == nullptr)
        {
            setSidecarError(
                error,
                {},
                "material",
                "Material asset is unavailable.");

            return false;
        }

        stylized::material::MaterialInstance* instance =
            resourceCache.getOrCreateMaterialInstance(
                handle,
                materialTemplate,
                assets);

        if (instance == nullptr)
        {
            setSidecarError(
                error,
                material->name,
                "materialInstance",
                "Material instance is unavailable.");

            return false;
        }

        stylized::material::MToonSidecarMaterial captured;

        if (!stylized::material::captureMToonSidecarMaterial(
                material->name,
                *instance,
                assets,
                sidecarDirectory,
                captured,
                error))
        {
            return false;
        }

        sidecar.materials.push_back(
            std::move(captured));
    }

    return stylized::material::saveMToonSidecarFile(
        sidecarPath,
        sidecar,
        error);
}

bool loadMaterialSidecar(
    const std::filesystem::path& modelPath,
    const std::vector<
        stylized::asset::AssetHandle<
            stylized::asset::MaterialAsset>>& materialHandles,
    const stylized::asset::AssetHandle<
        stylized::material::MaterialTemplate> materialTemplate,
    stylized::asset::AssetRegistry& assets,
    stylized::render::RuntimeResourceCache& resourceCache,
    stylized::material::MToonSidecarError& error)
{
    const std::filesystem::path sidecarPath =
        makeMaterialSidecarPath(modelPath);

    stylized::material::MToonMaterialSidecar sidecar;

    if (!stylized::material::loadMToonSidecarFile(
            sidecarPath,
            sidecar,
            error))
    {
        return false;
    }

    struct PendingMaterial
    {
        stylized::material::MaterialInstance* destination = nullptr;
        stylized::material::MaterialInstance value;
    };

    std::vector<PendingMaterial> pendingMaterials;
    pendingMaterials.reserve(
        sidecar.materials.size());

    for (const stylized::material::MToonSidecarMaterial& record :
         sidecar.materials)
    {
        stylized::asset::AssetHandle<
            stylized::asset::MaterialAsset>
            matchingHandle;

        for (const auto handle : materialHandles)
        {
            const stylized::asset::MaterialAsset* material =
                assets.get(handle);

            if (material != nullptr &&
                material->name == record.name)
            {
                matchingHandle = handle;
                break;
            }
        }

        if (matchingHandle.isNull())
        {
            setSidecarError(
                error,
                record.name,
                "name",
                "Material does not exist in the current model.");

            return false;
        }

        stylized::material::MaterialInstance* destination =
            resourceCache.getOrCreateMaterialInstance(
                matchingHandle,
                materialTemplate,
                assets);

        if (destination == nullptr)
        {
            setSidecarError(
                error,
                record.name,
                "materialInstance",
                "Material instance is unavailable.");

            return false;
        }

        stylized::material::MaterialInstance applied =
            *destination;

        if (!stylized::material::applyMToonSidecarMaterial(
                record,
                sidecarPath.parent_path(),
                assets,
                applied,
                error))
        {
            return false;
        }

        pendingMaterials.push_back({
            .destination = destination,
            .value = std::move(applied)
        });
    }

    for (PendingMaterial& pending : pendingMaterials)
    {
        *pending.destination =
            std::move(pending.value);
    }

    return true;
}

void drawTextureStatus(
    const char* label,
    const stylized::asset::AssetHandle<
        stylized::asset::TextureAsset> handle,
    const stylized::asset::AssetRegistry& assets,
    const char* fallbackName)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine();

    if (handle.isNull())
    {
        ImGui::TextDisabled(
            "Missing (%s fallback)",
            fallbackName);
        return;
    }

    const stylized::asset::TextureAsset* texture =
        assets.get(handle);

    if (texture == nullptr)
    {
        ImGui::TextColored(
            ImVec4{1.0F, 0.3F, 0.3F, 1.0F},
            "Invalid texture asset");
        return;
    }

    const std::string textureName =
        !texture->sourcePath.empty()
        ? texture->sourcePath.string()
        : !texture->debugName.empty()
            ? texture->debugName
            : "Texture asset " +
                std::to_string(handle.id().value);

    ImGui::TextWrapped(
        "%s",
        textureName.c_str());
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

bool ViewerPanels::wantsMouseCapture() const noexcept
{
    if (!initialized_)
    {
        return false;
    }

    return ImGui::GetIO().WantCaptureMouse;
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
    stylized::asset::AssetRegistry& assets,
    stylized::render::RuntimeResourceCache& resourceCache,
    const stylized::asset::AssetHandle<
        stylized::material::MaterialTemplate>
        materialTemplate,
    const stylized::asset::SceneAsset* scene,
    stylized::render::RenderWorld& renderWorld,
    const std::size_t drawCallCount,
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
    bool& toneMappingEnabled)
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

    if (materialKind ==
        stylized::material::MaterialKind::MToon)
    {
        int debugView = static_cast<int>(
            renderWorld.mainView.mtoonDebugView);

        constexpr const char* debugViews[] = {
            "Final",
            "Base",
            "Shade",
            "Lighting",
            "Rim",
            "MatCap",
            "Emission"
        };

        if (ImGui::Combo(
                "MToon Debug View",
                &debugView,
                debugViews,
                IM_ARRAYSIZE(debugViews)))
        {
            renderWorld.mainView.mtoonDebugView =
                static_cast<
                    stylized::render::MToonDebugView>(
                        debugView);
        }
    }

    const auto materialHandles =
        collectMaterialHandles(
            assets,
            scene);

    const auto selectedIterator =
        std::find(
            materialHandles.begin(),
            materialHandles.end(),
            selectedMaterial_);

    if (selectedIterator == materialHandles.end())
    {
        selectedMaterial_ =
            materialHandles.empty()
            ? decltype(selectedMaterial_){}
            : materialHandles.front();
    }

    const stylized::asset::MaterialAsset* selectedMaterial =
        assets.get(selectedMaterial_);

    const char* selectedMaterialName =
        selectedMaterial != nullptr &&
            !selectedMaterial->name.empty()
        ? selectedMaterial->name.c_str()
        : "None";

    ImGui::Separator();
    ImGui::Text(
        "Materials: %zu",
        materialHandles.size());

    if (ImGui::BeginCombo(
            "Selected Material",
            selectedMaterialName))
    {
        for (const auto handle : materialHandles)
        {
            const stylized::asset::MaterialAsset* material =
                assets.get(handle);

            if (material == nullptr)
            {
                continue;
            }

            const std::string visibleName =
                material->name.empty()
                ? "Unnamed Material"
                : material->name;

            const std::string label =
                visibleName +
                "##material_" +
                std::to_string(handle.id().value);

            const bool selected =
                handle == selectedMaterial_;

            if (ImGui::Selectable(
                    label.c_str(),
                    selected))
            {
                selectedMaterial_ = handle;
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    if (materialKind ==
        stylized::material::MaterialKind::MToon)
    {
        const std::filesystem::path sidecarPath =
            makeMaterialSidecarPath(modelPath);

        ImGui::TextWrapped(
            "Sidecar: %s",
            sidecarPath.string().c_str());

        stylized::material::MToonSidecarError sidecarError;

        if (ImGui::Button("Save Materials"))
        {
            materialSidecarFailed_ =
                !saveMaterialSidecar(
                    modelPath,
                    materialHandles,
                    materialTemplate,
                    assets,
                    resourceCache,
                    sidecarError);

            materialSidecarStatus_ =
                materialSidecarFailed_
                ? formatSidecarError(sidecarError)
                : "Saved: " + sidecarPath.string();
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Materials"))
        {
            materialSidecarFailed_ =
                !loadMaterialSidecar(
                    modelPath,
                    materialHandles,
                    materialTemplate,
                    assets,
                    resourceCache,
                    sidecarError);

            materialSidecarStatus_ =
                materialSidecarFailed_
                ? formatSidecarError(sidecarError)
                : "Loaded: " + sidecarPath.string();
        }

        if (!materialSidecarStatus_.empty())
        {
            const ImVec4 statusColor =
                materialSidecarFailed_
                ? ImVec4{1.0F, 0.3F, 0.3F, 1.0F}
                : ImVec4{0.3F, 1.0F, 0.3F, 1.0F};

            ImGui::TextColored(
                statusColor,
                "%s",
                materialSidecarStatus_.c_str());
        }
    }

    if (materialKind ==
            stylized::material::MaterialKind::MToon &&
        !selectedMaterial_.isNull())
    {
        stylized::material::MaterialInstance* materialInstance =
            resourceCache.getOrCreateMaterialInstance(
                selectedMaterial_,
                materialTemplate,
                assets);

        if (materialInstance != nullptr &&
            materialInstance->mtoonParameters.has_value())
        {
            bool resetFailed = false;

            if (ImGui::Button("Reset Selected Material"))
            {
                resetFailed =
                    !resourceCache.resetMaterialInstance(
                        selectedMaterial_,
                        materialTemplate,
                        assets);
            }

            if (resetFailed)
            {
                ImGui::TextColored(
                    ImVec4{1.0F, 0.3F, 0.3F, 1.0F},
                    "Failed to reset selected material.");
            }

            stylized::material::MToonMaterialParameters& parameters =
                materialInstance->mtoonParameters.value();

            if (ImGui::CollapsingHeader(
                    "Base / Shade",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ColorEdit4(
                    "Base Color Factor",
                    &materialInstance->baseColorFactor.x);

                drawTextureStatus(
                    "Base Texture:",
                    materialInstance->baseColorTexture,
                    assets,
                    "White");

                ImGui::ColorEdit3(
                    "Shade Color",
                    &parameters.shadeColor.x);

                drawTextureStatus(
                    "Shade Texture:",
                    parameters.textures.shadeTexture,
                    assets,
                    "White");

                ImGui::SliderFloat(
                    "Shading Shift",
                    &parameters.shadingShift,
                    -1.0F,
                    1.0F,
                    "%.3f");

                ImGui::SliderFloat(
                    "Shift Texture Scale",
                    &parameters.shadingShiftTextureScale,
                    -2.0F,
                    2.0F,
                    "%.3f");

                drawTextureStatus(
                    "Shift Texture:",
                    parameters.textures.shadingShiftTexture,
                    assets,
                    "Black");

                ImGui::SliderFloat(
                    "Shading Toony",
                    &parameters.shadingToony,
                    0.0F,
                    1.0F,
                    "%.3f");
            }

            if (ImGui::CollapsingHeader("Normal"))
            {
                ImGui::SliderFloat(
                    "Normal Scale",
                    &parameters.normalScale,
                    0.0F,
                    2.0F,
                    "%.3f");

                drawTextureStatus(
                    "Normal Texture:",
                    parameters.textures.normalTexture,
                    assets,
                    "Neutral Normal");
            }

            if (ImGui::CollapsingHeader("GI"))
            {
                ImGui::SliderFloat(
                    "GI Equalization",
                    &parameters.giEqualization,
                    0.0F,
                    1.0F,
                    "%.3f");
            }

            if (ImGui::CollapsingHeader("MatCap"))
            {
                ImGui::ColorEdit3(
                    "MatCap Color",
                    &parameters.matcapColor.x);

                ImGui::SliderFloat(
                    "MatCap Strength",
                    &parameters.matcapStrength,
                    0.0F,
                    4.0F,
                    "%.3f");

                drawTextureStatus(
                    "MatCap Texture:",
                    parameters.textures.matcapTexture,
                    assets,
                    "Black");
            }

            if (ImGui::CollapsingHeader("Rim"))
            {
                ImGui::ColorEdit3(
                    "Rim Color",
                    &parameters.rimColor.x);

                ImGui::SliderFloat(
                    "Rim Fresnel Power",
                    &parameters.rimFresnelPower,
                    0.1F,
                    16.0F,
                    "%.3f");

                ImGui::SliderFloat(
                    "Rim Lift",
                    &parameters.rimLift,
                    -1.0F,
                    1.0F,
                    "%.3f");

                ImGui::SliderFloat(
                    "Rim Lighting Mix",
                    &parameters.rimLightingMix,
                    0.0F,
                    1.0F,
                    "%.3f");

                drawTextureStatus(
                    "Rim Mask Texture:",
                    parameters.textures.rimMaskTexture,
                    assets,
                    "White");
            }

            if (ImGui::CollapsingHeader("Emission"))
            {
                ImGui::ColorEdit3(
                    "Emission Color",
                    &parameters.emissionColor.x,
                    ImGuiColorEditFlags_HDR |
                        ImGuiColorEditFlags_Float);

                ImGui::SliderFloat(
                    "Emission Strength",
                    &parameters.emissionStrength,
                    0.0F,
                    10.0F,
                    "%.3f");

                drawTextureStatus(
                    "Emission Texture:",
                    parameters.textures.emissionTexture,
                    assets,
                    "Black");
            }

            if (ImGui::CollapsingHeader("Outline"))
            {
                ImGui::Checkbox(
                    "Outline Enabled",
                    &parameters.outline.enabled);

                int widthMode =
                    parameters.outline.widthMode ==
                            stylized::material::OutlineWidthMode::World
                        ? 0
                        : 1;

                constexpr const char* widthModes[] = {
                    "World",
                    "Screen"
                };

                if (ImGui::Combo(
                        "Outline Width Mode",
                        &widthMode,
                        widthModes,
                        IM_ARRAYSIZE(widthModes)))
                {
                    parameters.outline.widthMode =
                        widthMode == 0
                            ? stylized::material::
                                OutlineWidthMode::World
                            : stylized::material::
                                OutlineWidthMode::Screen;
                }

                const float widthSpeed =
                    parameters.outline.widthMode ==
                            stylized::material::OutlineWidthMode::World
                        ? 0.001F
                        : 0.1F;

                ImGui::DragFloat(
                    "Outline Width",
                    &parameters.outline.width,
                    widthSpeed,
                    0.0F,
                    100.0F,
                    "%.3f");

                ImGui::ColorEdit3(
                    "Outline Color",
                    &parameters.outline.color.x);

                ImGui::SliderFloat(
                    "Outline Lighting Mix",
                    &parameters.outline.lightingMix,
                    0.0F,
                    1.0F,
                    "%.3f");

                drawTextureStatus(
                    "Outline Width Mask:",
                    parameters.textures.outlineWidthMaskTexture,
                    assets,
                    "White");
            }
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Screen Space Outline");

    if (screenSpaceOutlinePass != nullptr)
    {
        stylized::render::ScreenSpaceOutlineSettings settings =
            screenSpaceOutlinePass->settings();

        bool changed = false;

        changed |= ImGui::Checkbox(
            "Screen Outline Enabled",
            &settings.enabled);

        changed |= ImGui::ColorEdit3(
            "Screen Outline Color",
            &settings.color.x);

        changed |= ImGui::SliderFloat(
            "Screen Outline Width",
            &settings.width,
            1.0F,
            8.0F,
            "%.1f");

        changed |= ImGui::SliderFloat(
            "Depth Threshold",
            &settings.depthThreshold,
            0.001F,
            0.1F,
            "%.4f");

        changed |= ImGui::SliderFloat(
            "Normal Threshold",
            &settings.normalThreshold,
            0.01F,
            1.0F,
            "%.3f");

        if (changed)
        {
            screenSpaceOutlinePass->setSettings(settings);
        }
    }
    else
    {
        ImGui::TextUnformatted("Unavailable");
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
        "OutlineMaskPass",
        outlineMaskPass == nullptr
            ? "Unavailable"
            : framePipeline != nullptr &&
                    !framePipeline->passLastExecutionSucceeded(2)
                ? "Failed"
                : "Active",
        outlineMaskPass != nullptr
            ? outlineMaskPass->lastDrawCallCount()
            : 0,
        framePipeline != nullptr &&
            framePipeline->passHasGpuTime(2),
        framePipeline != nullptr
            ? framePipeline->passGpuTimeMilliseconds(2)
            : 0.0);

    drawPassStatus(
        "ScreenSpaceOutlinePass",
        screenSpaceOutlinePass == nullptr
            ? "Unavailable"
            : framePipeline != nullptr &&
                    !framePipeline
                        ->passLastExecutionSucceeded(3)
                ? "Failed"
                : "Active",
        screenSpaceOutlinePass != nullptr
            ? screenSpaceOutlinePass
                ->lastDrawCallCount()
            : 0,
        framePipeline != nullptr &&
            framePipeline->passHasGpuTime(3),
        framePipeline != nullptr
            ? framePipeline
                ->passGpuTimeMilliseconds(3)
            : 0.0);

    drawPassStatus(
        "PostProcessPass",
        postProcessPass == nullptr
            ? "Unavailable"
            : framePipeline != nullptr &&
                    !framePipeline->passLastExecutionSucceeded(4)
                ? "Failed"
                : "Active",
        postProcessPass != nullptr
            ? postProcessPass->lastDrawCallCount()
            : 0,
        framePipeline != nullptr &&
            framePipeline->passHasGpuTime(4),
        framePipeline != nullptr
            ? framePipeline->passGpuTimeMilliseconds(4)
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

        ImGui::Text(
            "Forward Normal: %u x %u, %s, Rebuilds: %zu",
            extent.width,
            extent.height,
            renderTextureFormatName(
                forwardPass->normalFormat()),
            rebuildCount);
    }
    else
    {
        ImGui::TextUnformatted(
            "Forward targets: not created");
    }

    if (outlineMaskPass != nullptr &&
        outlineMaskPass->hasRenderTarget())
    {
        const stylized::graphics::Extent2D extent =
            outlineMaskPass->renderTargetExtent();

        ImGui::Text(
            "Outline Mask: %u x %u, %s, Rebuilds: %zu",
            extent.width,
            extent.height,
            renderTextureFormatName(
                outlineMaskPass->renderTargetFormat()),
            outlineMaskPass->renderTargetRebuildCount());
    }
    else
    {
        ImGui::TextUnformatted(
            "Outline Mask: not created");
    }

    if (screenSpaceOutlinePass != nullptr &&
        screenSpaceOutlinePass->hasRenderTarget())
    {
        const stylized::graphics::Extent2D extent =
            screenSpaceOutlinePass->renderTargetExtent();

        ImGui::Text(
            "Outlined HDR: %u x %u, %s, Rebuilds: %zu",
            extent.width,
            extent.height,
            renderTextureFormatName(
                screenSpaceOutlinePass->renderTargetFormat()),
            screenSpaceOutlinePass->renderTargetRebuildCount());
    }
    else
    {
        ImGui::TextUnformatted(
            "Outlined HDR: not created");
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

