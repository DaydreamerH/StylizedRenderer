#include "ViewerPanels.hpp"
#include "ViewerTheme.hpp"

#include <animation/AnimationPlayer.hpp>
#include <asset/AnimationAsset.hpp>
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
#include <render/resources/RuntimeMeshInstance.hpp>
#include <render/resources/SkinningPaletteSet.hpp>

#include <material/MaterialInstance.hpp>
#include <material/mtoon/MToonMaterialSidecar.hpp>
#include <material/mtoon/MToonMaterialSidecarSerializer.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cfloat>
#include <map>
#include <span>
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
        ImGui::TextWrapped(
            "%s: %s, Draws: %zu, GPU: %.3f ms",
            name,
            status,
            drawCallCount,
            gpuTimeMilliseconds);
    }
    else
    {
        ImGui::TextWrapped(
            "%s: %s, Draws: %zu, GPU: pending",
            name,
            status,
            drawCallCount);
    }
}

[[nodiscard]] bool beginPropertyTable(
    const char* identifier)
{
    if (!ImGui::BeginTable(
            identifier,
            2,
            ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_PadOuterX))
    {
        return false;
    }

    ImGui::TableSetupColumn(
        "Property",
        ImGuiTableColumnFlags_WidthFixed,
        10.5F * ImGui::GetFontSize());

    ImGui::TableSetupColumn(
        "Value",
        ImGuiTableColumnFlags_WidthStretch);

    return true;
}

void beginPropertyRow(
    const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextWrapped("%s", label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::PushID(label);
}

void endPropertyRow()
{
    ImGui::PopID();
}

bool drawCheckboxProperty(
    const char* label,
    bool* value)
{
    beginPropertyRow(label);
    const bool changed =
        ImGui::Checkbox("##Value", value);
    endPropertyRow();
    return changed;
}

bool drawComboProperty(
    const char* label,
    int* value,
    const char* const* items,
    const int itemCount)
{
    beginPropertyRow(label);
    const bool changed =
        ImGui::Combo(
            "##Value",
            value,
            items,
            itemCount);
    endPropertyRow();
    return changed;
}

bool drawSliderFloatProperty(
    const char* label,
    float* value,
    const float minimum,
    const float maximum,
    const char* format)
{
    beginPropertyRow(label);
    const bool changed =
        ImGui::SliderFloat(
            "##Value",
            value,
            minimum,
            maximum,
            format);
    endPropertyRow();
    return changed;
}

bool drawColorEdit3Property(
    const char* label,
    float* value,
    const ImGuiColorEditFlags flags = 0)
{
    beginPropertyRow(label);
    const bool changed =
        ImGui::ColorEdit3(
            "##Value",
            value,
            flags);
    endPropertyRow();
    return changed;
}

bool drawColorEdit4Property(
    const char* label,
    float* value,
    const ImGuiColorEditFlags flags = 0)
{
    beginPropertyRow(label);
    const bool changed =
        ImGui::ColorEdit4(
            "##Value",
            value,
            flags);
    endPropertyRow();
    return changed;
}

bool drawDragFloatProperty(
    const char* label,
    float* value,
    const float speed,
    const float minimum,
    const float maximum,
    const char* format)
{
    beginPropertyRow(label);
    const bool changed =
        ImGui::DragFloat(
            "##Value",
            value,
            speed,
            minimum,
            maximum,
            format);
    endPropertyRow();
    return changed;
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
    beginPropertyRow(label);

    if (handle.isNull())
    {
        ImGui::TextDisabled(
            "Missing (%s fallback)",
            fallbackName);
        endPropertyRow();
        return;
    }

    const stylized::asset::TextureAsset* texture =
        assets.get(handle);

    if (texture == nullptr)
    {
        ImGui::TextColored(
            ImVec4{1.0F, 0.3F, 0.3F, 1.0F},
            "Invalid texture asset");
        endPropertyRow();
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

    endPropertyRow();
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

    if (!stylized::viewer::ui::applyViewerTheme(
            window))
    {
        ImGui::DestroyContext();
        return false;
    }

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
    stylized::animation::AnimationPlayer&
        animationPlayer,
    const stylized::render::SkinningPaletteSet&
        skinningPalettes,
    const std::span<stylized::render::RuntimeMeshInstance>
        morphMeshInstances,
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

    const ImGuiViewport* viewport =
        ImGui::GetMainViewport();

    const float maximumSidebarWidth =
        std::max(
            12.0F * ImGui::GetFontSize(),
            viewport->WorkSize.x * 0.65F);

    const float minimumSidebarWidth =
        std::min(
            22.0F * ImGui::GetFontSize(),
            maximumSidebarWidth);

    if (sidebarWidth_ <= 0.0F)
    {
        sidebarWidth_ =
            std::min(
                28.0F * ImGui::GetFontSize(),
                viewport->WorkSize.x * 0.55F);
    }

    sidebarWidth_ =
        std::clamp(
            sidebarWidth_,
            minimumSidebarWidth,
            maximumSidebarWidth);

    const float collapsedWidth =
        ImGui::GetFrameHeight() +
        2.0F * ImGui::GetStyle().WindowPadding.x;

    ImGui::SetNextWindowPos(
        viewport->WorkPos,
        ImGuiCond_Always);

    ImGui::SetNextWindowSize(
        ImVec2{
            sidebarExpanded_
                ? sidebarWidth_
                : collapsedWidth,
            viewport->WorkSize.y},
        ImGuiCond_Always);

    constexpr ImGuiWindowFlags sidebarFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin(
        "StylizedRenderer##ViewerSidebar",
        nullptr,
        sidebarFlags);

    if (!sidebarExpanded_)
    {
        const float buttonWidth =
            ImGui::GetFrameHeight();

        ImGui::SetCursorPosX(
            0.5F *
                (ImGui::GetWindowWidth() -
                 buttonWidth));

        if (ImGui::ArrowButton(
                "##ExpandViewerSidebar",
                ImGuiDir_Right))
        {
            sidebarExpanded_ = true;
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Expand viewer controls");
        }

        ImGui::End();
        return;
    }

    constexpr float resizeHandleWidth = 8.0F;

    const ImVec2 resizeHandleMinimum =
        ImVec2{
            viewport->WorkPos.x +
                sidebarWidth_ -
                0.5F * resizeHandleWidth,
            viewport->WorkPos.y};

    const ImVec2 resizeHandleMaximum =
        ImVec2{
            resizeHandleMinimum.x +
                resizeHandleWidth,
            viewport->WorkPos.y +
                viewport->WorkSize.y};

    const bool resizeHandleHovered =
        ImGui::IsMouseHoveringRect(
            resizeHandleMinimum,
            resizeHandleMaximum,
            false);

    if (resizeHandleHovered &&
        ImGui::IsMouseClicked(
            ImGuiMouseButton_Left))
    {
        sidebarResizing_ = true;
    }

    if (!ImGui::IsMouseDown(
            ImGuiMouseButton_Left))
    {
        sidebarResizing_ = false;
    }

    if (resizeHandleHovered ||
        sidebarResizing_)
    {
        ImGui::SetMouseCursor(
            ImGuiMouseCursor_ResizeEW);
    }

    if (sidebarResizing_)
    {
        sidebarWidth_ =
            std::clamp(
                ImGui::GetIO().MousePos.x -
                    viewport->WorkPos.x,
                minimumSidebarWidth,
                maximumSidebarWidth);
    }

    if (!ImGui::BeginTabBar(
            "##ViewerSections",
            ImGuiTabBarFlags_FittingPolicyResizeDown))
    {
        ImGui::End();
        return;
    }

    if (ImGui::TabItemButton(
            "<<##CollapseViewerSidebar",
            ImGuiTabItemFlags_Trailing |
                ImGuiTabItemFlags_NoTooltip))
    {
        sidebarExpanded_ = false;
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Collapse viewer controls");
    }

    if (ImGui::BeginTabItem("Scene"))
    {
    ImGui::SeparatorText("Scene Overview");

    if (ImGui::BeginTable(
            "##SceneOverview",
            2,
            ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn(
            "Property",
            ImGuiTableColumnFlags_WidthFixed,
            10.5F * ImGui::GetFontSize());

        ImGui::TableSetupColumn(
            "Value",
            ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Model");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped(
            "%s",
            modelPath.string().c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Scene");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(
            scene != nullptr
                ? scene->name.c_str()
                : "Not loaded");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Nodes");
        ImGui::TableSetColumnIndex(1);

        if (scene != nullptr)
        {
            ImGui::Text("%zu", scene->nodes.size());
        }
        else
        {
            ImGui::TextUnformatted("-");
        }

        ImGui::EndTable();
    }

    if (scene != nullptr &&
        ImGui::TreeNodeEx(
            "Animation",
            ImGuiTreeNodeFlags_DefaultOpen |
                ImGuiTreeNodeFlags_NoTreePushOnOpen))
    {
        if (scene->animations.empty())
        {
            ImGui::TextUnformatted(
                "No animation clips");
        }
        else
        {
            const stylized::asset::AnimationClipAsset*
                selectedClip = animationPlayer.clip();

            const char* selectedClipName =
                selectedClip != nullptr &&
                    !selectedClip->name.empty()
                ? selectedClip->name.c_str()
                : "Select a clip";

            if (beginPropertyTable(
                    "##AnimationClipProperties"))
            {
                beginPropertyRow("Clip");

                if (ImGui::BeginCombo(
                        "##Value",
                        selectedClipName))
                {
                    for (const stylized::asset::AnimationClipAsset& clip :
                         scene->animations)
                    {
                        const bool selected =
                            selectedClip == &clip;

                        const char* clipName =
                            clip.name.empty()
                            ? "Unnamed Clip"
                            : clip.name.c_str();

                        if (ImGui::Selectable(
                                clipName,
                                selected))
                        {
                            if (animationPlayer.setClip(&clip))
                            {
                                animationPlayer.play();
                                selectedClip = &clip;
                            }
                        }

                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
                }

                endPropertyRow();
                ImGui::EndTable();
            }

            if (animationPlayer.clip() != nullptr)
            {
                const float playbackButtonWidth =
                    0.5F *
                    (ImGui::GetContentRegionAvail().x -
                     ImGui::GetStyle().ItemSpacing.x);

                if (animationPlayer.isPlaying())
                {
                    if (ImGui::Button(
                            "Pause",
                            ImVec2{
                                playbackButtonWidth,
                                0.0F}))
                    {
                        animationPlayer.pause();
                    }
                }
                else if (ImGui::Button(
                             "Play",
                             ImVec2{
                                 playbackButtonWidth,
                                 0.0F}))
                {
                    animationPlayer.play();
                }

                ImGui::SameLine();

                if (ImGui::Button(
                        "Stop",
                        ImVec2{
                            playbackButtonWidth,
                            0.0F}))
                {
                    animationPlayer.stop();
                }

                bool looping =
                    animationPlayer.isLooping();

                float playbackSpeed =
                    animationPlayer.playbackSpeed();

                const stylized::asset::AnimationClipAsset*
                    clip = animationPlayer.clip();

                float currentTime =
                    animationPlayer.currentTime();

                if (beginPropertyTable(
                        "##AnimationPlaybackProperties"))
                {
                    if (drawCheckboxProperty(
                            "Loop",
                            &looping))
                    {
                        animationPlayer.setLooping(looping);
                    }

                    if (drawSliderFloatProperty(
                            "Speed",
                            &playbackSpeed,
                            0.0F,
                            4.0F,
                            "%.2fx"))
                    {
                        animationPlayer.setPlaybackSpeed(
                            playbackSpeed);
                    }

                    if (clip != nullptr &&
                        drawSliderFloatProperty(
                            "Time",
                            &currentTime,
                            0.0F,
                            clip->durationSeconds,
                            "%.3f s"))
                    {
                        animationPlayer.seek(currentTime);
                    }

                    ImGui::EndTable();
                }

                if (clip != nullptr)
                {
                    ImGui::Text(
                        "Duration: %.3f s | Channels: %zu",
                        clip->durationSeconds,
                        clip->channels.size());
                }
            }
        }

        ImGui::Text(
            "Palettes: %zu | Joints: %zu | Uploads: %zu",
            skinningPalettes.paletteCount(),
            skinningPalettes.jointMatrixCount(),
            skinningPalettes.lastUploadCount());
    }

    if (scene != nullptr &&
        morphMeshInstances.size() ==
            scene->nodes.size())
    {
        std::size_t morphTargetCount = 0;
        std::size_t activeMorphCount = 0;
        std::size_t lastMorphUploadCount = 0;
        std::size_t totalMorphUploadCount = 0;

        for (const stylized::render::RuntimeMeshInstance& instance :
             morphMeshInstances)
        {
            lastMorphUploadCount +=
                instance.lastUploadCount();

            totalMorphUploadCount +=
                instance.totalUploadCount();

            for (std::size_t primitiveIndex = 0;
                 primitiveIndex < instance.primitiveCount();
                 ++primitiveIndex)
            {
                const stylized::animation::MorphState* state =
                    instance.morphState(primitiveIndex);

                if (state == nullptr)
                {
                    continue;
                }

                morphTargetCount += state->targetCount();
                activeMorphCount +=
                    state->activeTargetCount();
            }
        }

        if (morphTargetCount > 0 &&
            ImGui::CollapsingHeader(
                "Expressions / Morph Targets"))
        {
            if (ImGui::Button(
                    "Reset All Morphs",
                    ImVec2{
                        ImGui::GetContentRegionAvail().x,
                        0.0F}))
            {
                for (stylized::render::RuntimeMeshInstance& instance :
                     morphMeshInstances)
                {
                    for (std::size_t primitiveIndex = 0;
                         primitiveIndex < instance.primitiveCount();
                         ++primitiveIndex)
                    {
                        stylized::animation::MorphState* state =
                            instance.morphState(primitiveIndex);

                        if (state != nullptr)
                        {
                            state->reset();
                        }
                    }
                }
            }

            for (std::size_t nodeIndex = 0;
                 nodeIndex < scene->nodes.size();
                 ++nodeIndex)
            {
                stylized::render::RuntimeMeshInstance& instance =
                    morphMeshInstances[nodeIndex];

                if (!instance.isValid())
                {
                    continue;
                }

                const stylized::asset::SceneNodeAsset& node =
                    scene->nodes[nodeIndex];

                const stylized::asset::MeshAsset* mesh =
                    assets.get(node.mesh);

                if (mesh == nullptr ||
                    mesh->primitives.size() !=
                        instance.primitiveCount())
                {
                    continue;
                }

                ImGui::PushID(
                    static_cast<int>(nodeIndex));

                const std::string nodeLabel =
                    node.name.empty()
                    ? "Node " + std::to_string(nodeIndex)
                    : node.name;

                if (ImGui::TreeNode(nodeLabel.c_str()))
                {
                    using MorphBinding =
                        std::pair<
                            stylized::animation::MorphState*,
                            std::size_t>;

                    std::map<
                        std::string,
                        std::vector<MorphBinding>>
                        namedMorphs;

                    for (std::size_t primitiveIndex = 0;
                         primitiveIndex < mesh->primitives.size();
                         ++primitiveIndex)
                    {
                        stylized::animation::MorphState* state =
                            instance.morphState(primitiveIndex);

                        if (state == nullptr)
                        {
                            continue;
                        }

                        const std::vector<stylized::asset::MorphTargetAsset>&
                            targets =
                                mesh->primitives[primitiveIndex]
                                    .morphTargets;

                        for (std::size_t targetIndex = 0;
                             targetIndex < targets.size();
                             ++targetIndex)
                        {
                            namedMorphs[targets[targetIndex].name]
                                .emplace_back(
                                    state,
                                    targetIndex);
                        }
                    }

                    if (beginPropertyTable(
                            "##MorphProperties"))
                    {
                        for (auto& [name, bindings] : namedMorphs)
                        {
                            if (bindings.empty())
                            {
                                continue;
                            }

                            float weight =
                                bindings.front().first->weight(
                                    bindings.front().second);

                            if (drawSliderFloatProperty(
                                    name.c_str(),
                                    &weight,
                                    0.0F,
                                    1.0F,
                                    "%.3f"))
                            {
                                for (const MorphBinding& binding :
                                     bindings)
                                {
                                    if (!binding.first->setWeight(
                                            binding.second,
                                            weight))
                                    {
                                        break;
                                    }
                                }
                            }
                        }

                        ImGui::EndTable();
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            ImGui::Text(
                "Morph Targets: %zu, Active: %zu",
                morphTargetCount,
                activeMorphCount);

            ImGui::Text(
                "Morph Uploads: %zu last, %zu total",
                lastMorphUploadCount,
                totalMorphUploadCount);
        }
    }

    ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Material"))
    {

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

    ImGui::SeparatorText("Material Mode");

    if (beginPropertyTable(
            "##MaterialModeProperties"))
    {
        if (drawComboProperty(
                "Mode",
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

            if (drawComboProperty(
                "Debug View",
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

        ImGui::EndTable();
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

    ImGui::SeparatorText("Materials");
    ImGui::Text(
        "Materials: %zu",
        materialHandles.size());

    if (beginPropertyTable(
            "##MaterialSelectionProperties"))
    {
        beginPropertyRow("Selected");

        if (ImGui::BeginCombo(
                "##Value",
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

        endPropertyRow();
        ImGui::EndTable();
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

        const float actionButtonWidth =
            0.5F *
            (ImGui::GetContentRegionAvail().x -
             ImGui::GetStyle().ItemSpacing.x);

        if (ImGui::Button(
                "Save Materials",
                ImVec2{actionButtonWidth, 0.0F}))
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

        if (ImGui::Button(
                "Load Materials",
                ImVec2{actionButtonWidth, 0.0F}))
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

            if (ImGui::Button(
                    "Reset Selected Material",
                    ImVec2{
                        ImGui::GetContentRegionAvail().x,
                        0.0F}))
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
                if (beginPropertyTable(
                        "##BaseShadeProperties"))
                {
                    drawColorEdit4Property(
                        "Base Color Factor",
                        &materialInstance->baseColorFactor.x);

                    drawTextureStatus(
                        "Base Texture",
                        materialInstance->baseColorTexture,
                        assets,
                        "White");

                    drawColorEdit3Property(
                        "Shade Color",
                        &parameters.shadeColor.x);

                    drawTextureStatus(
                        "Shade Texture",
                        parameters.textures.shadeTexture,
                        assets,
                        "White");

                    drawSliderFloatProperty(
                        "Shading Shift",
                        &parameters.shadingShift,
                        -1.0F,
                        1.0F,
                        "%.3f");

                    drawSliderFloatProperty(
                        "Shift Texture Scale",
                        &parameters.shadingShiftTextureScale,
                        -2.0F,
                        2.0F,
                        "%.3f");

                    drawTextureStatus(
                        "Shift Texture",
                        parameters.textures.shadingShiftTexture,
                        assets,
                        "Black");

                    drawSliderFloatProperty(
                        "Shading Toony",
                        &parameters.shadingToony,
                        0.0F,
                        1.0F,
                        "%.3f");

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Normal"))
            {
                if (beginPropertyTable(
                        "##NormalProperties"))
                {
                    drawSliderFloatProperty(
                        "Scale",
                        &parameters.normalScale,
                        0.0F,
                        2.0F,
                        "%.3f");

                    drawTextureStatus(
                        "Texture",
                        parameters.textures.normalTexture,
                        assets,
                        "Neutral Normal");

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("GI"))
            {
                if (beginPropertyTable(
                        "##GIProperties"))
                {
                    drawSliderFloatProperty(
                        "Equalization",
                        &parameters.giEqualization,
                        0.0F,
                        1.0F,
                        "%.3f");

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("MatCap"))
            {
                if (beginPropertyTable(
                        "##MatCapProperties"))
                {
                    drawColorEdit3Property(
                        "Color",
                        &parameters.matcapColor.x);

                    drawSliderFloatProperty(
                        "Strength",
                        &parameters.matcapStrength,
                        0.0F,
                        4.0F,
                        "%.3f");

                    drawTextureStatus(
                        "Texture",
                        parameters.textures.matcapTexture,
                        assets,
                        "Black");

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Rim"))
            {
                if (beginPropertyTable(
                        "##RimProperties"))
                {
                    drawColorEdit3Property(
                        "Color",
                        &parameters.rimColor.x);

                    drawSliderFloatProperty(
                        "Fresnel Power",
                        &parameters.rimFresnelPower,
                        0.1F,
                        16.0F,
                        "%.3f");

                    drawSliderFloatProperty(
                        "Lift",
                        &parameters.rimLift,
                        -1.0F,
                        1.0F,
                        "%.3f");

                    drawSliderFloatProperty(
                        "Lighting Mix",
                        &parameters.rimLightingMix,
                        0.0F,
                        1.0F,
                        "%.3f");

                    drawTextureStatus(
                        "Mask Texture",
                        parameters.textures.rimMaskTexture,
                        assets,
                        "White");

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Emission"))
            {
                if (beginPropertyTable(
                        "##EmissionProperties"))
                {
                    drawColorEdit3Property(
                        "Color",
                        &parameters.emissionColor.x,
                        ImGuiColorEditFlags_HDR |
                            ImGuiColorEditFlags_Float);

                    drawSliderFloatProperty(
                        "Strength",
                        &parameters.emissionStrength,
                        0.0F,
                        10.0F,
                        "%.3f");

                    drawTextureStatus(
                        "Texture",
                        parameters.textures.emissionTexture,
                        assets,
                        "Black");

                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Outline"))
            {
                int widthMode =
                    parameters.outline.widthMode ==
                            stylized::material::OutlineWidthMode::World
                        ? 0
                        : 1;

                constexpr const char* widthModes[] = {
                    "World",
                    "Screen"
                };

                if (beginPropertyTable(
                        "##OutlineProperties"))
                {
                    drawCheckboxProperty(
                        "Enabled",
                        &parameters.outline.enabled);

                    if (drawComboProperty(
                            "Width Mode",
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

                    drawDragFloatProperty(
                        "Width",
                        &parameters.outline.width,
                        widthSpeed,
                        0.0F,
                        100.0F,
                        "%.3f");

                    drawColorEdit3Property(
                        "Color",
                        &parameters.outline.color.x);

                    drawSliderFloatProperty(
                        "Lighting Mix",
                        &parameters.outline.lightingMix,
                        0.0F,
                        1.0F,
                        "%.3f");

                    drawTextureStatus(
                        "Width Mask",
                        parameters.textures.outlineWidthMaskTexture,
                        assets,
                        "White");

                    ImGui::EndTable();
                }
            }
        }
    }

    ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Render"))
    {

    ImGui::SeparatorText("Screen Space Outline");

    if (screenSpaceOutlinePass != nullptr)
    {
        stylized::render::ScreenSpaceOutlineSettings settings =
            screenSpaceOutlinePass->settings();

        bool changed = false;

        int debugView =
            static_cast<int>(settings.debugView);

        constexpr const char* debugViews[] = {
            "Final",
            "Surface Normal",
            "Linear Depth",
            "Shell Outline Mask",
            "Screen Edge",
            "Combined Outline"
        };

        if (beginPropertyTable(
                "##ScreenOutlineProperties"))
        {
            if (drawComboProperty(
                    "Debug View",
                    &debugView,
                    debugViews,
                    IM_ARRAYSIZE(debugViews)))
            {
                settings.debugView =
                    static_cast<
                        stylized::render::OutlineDebugView>(
                            debugView);

                changed = true;
            }

            changed |= drawCheckboxProperty(
                "Enabled",
                &settings.enabled);

            changed |= drawColorEdit3Property(
                "Color",
                &settings.color.x);

            changed |= drawSliderFloatProperty(
                "Width",
                &settings.width,
                1.0F,
                8.0F,
                "%.1f");

            changed |= drawSliderFloatProperty(
                "Depth Threshold",
                &settings.depthThreshold,
                0.001F,
                0.1F,
                "%.4f");

            changed |= drawSliderFloatProperty(
                "Normal Threshold",
                &settings.normalThreshold,
                0.01F,
                1.0F,
                "%.3f");

            ImGui::EndTable();
        }

        if (changed)
        {
            screenSpaceOutlinePass->setSettings(settings);
        }
    }
    else
    {
        ImGui::TextUnformatted("Unavailable");
    }

    ImGui::SeparatorText("Lighting");

    const stylized::render::DirectionalLightData& mainLight =
        renderWorld.mainView.mainLight;

    if (beginPropertyTable(
            "##LightingProperties"))
    {
        drawCheckboxProperty(
            "Shadows",
            &shadowsEnabled);

        beginPropertyRow("Direction");
        ImGui::Text(
            "(%.2f, %.2f, %.2f)",
            mainLight.direction.x,
            mainLight.direction.y,
            mainLight.direction.z);
        endPropertyRow();

        beginPropertyRow("Color");
        ImGui::Text(
            "(%.2f, %.2f, %.2f)",
            mainLight.color.r,
            mainLight.color.g,
            mainLight.color.b);
        endPropertyRow();

        beginPropertyRow("Intensity");
        ImGui::Text("%.2f", mainLight.intensity);
        endPropertyRow();

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Post Process");

    if (beginPropertyTable(
            "##PostProcessProperties"))
    {
        drawSliderFloatProperty(
            "Exposure",
            &exposure,
            0.0F,
            5.0F,
            "%.2f");

        drawCheckboxProperty(
            "Tone Mapping",
            &toneMappingEnabled);

        ImGui::EndTable();
    }

    ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Stats"))
    {

    ImGui::SeparatorText("Render Pipeline");

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
                : outlineMaskPass->lastDrawCallCount() == 0
                    ? "Idle"
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
                : screenSpaceOutlinePass->settings().debugView !=
                        stylized::render::OutlineDebugView::Final
                    ? "Debug View"
                    : screenSpaceOutlinePass->settings().enabled
                        ? "Active"
                        : "Composite Only",
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

    ImGui::SeparatorText("Render Targets");

    if (shadowPass != nullptr &&
        shadowPass->hasShadowMap())
    {
        const stylized::graphics::Extent2D extent =
            shadowPass->shadowMapExtent();

        ImGui::TextWrapped(
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

        ImGui::TextWrapped(
            "HDR Color: %u x %u, %s, Rebuilds: %zu",
            extent.width,
            extent.height,
            renderTextureFormatName(
                forwardPass->colorFormat()),
            rebuildCount);

        ImGui::TextWrapped(
            "Forward Depth: %u x %u, %s, Rebuilds: %zu",
            extent.width,
            extent.height,
            depthTextureFormatName(
                forwardPass->depthFormat()),
            rebuildCount);

        ImGui::TextWrapped(
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

        ImGui::TextWrapped(
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

        ImGui::TextWrapped(
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

    ImGui::SeparatorText("Frame Statistics");
    ImGui::Text("Assets: %zu", assets.size());
    ImGui::Text("Render Items: %zu", renderWorld.size());
    ImGui::Text("Total Items: %zu", renderWorld.renderStats.totalItems);
    ImGui::Text("Visible Items: %zu", renderWorld.renderStats.visibleItems);
    ImGui::Text("Culled Items: %zu", renderWorld.renderStats.culledItems);
    ImGui::Text("Draw Calls: %zu", drawCallCount);

    ImGui::SeparatorText("Camera");
    ImGui::Text(
        "Camera: (%.2f, %.2f, %.2f)",
        renderWorld.mainView.cameraPosition.x,
        renderWorld.mainView.cameraPosition.y,
        renderWorld.mainView.cameraPosition.z);

    ImGui::EndTabItem();
    }

    ImGui::EndTabBar();

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

