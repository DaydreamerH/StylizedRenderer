#include "ViewerPanels.hpp"

#include <asset/AssetRegistry.hpp>
#include <asset/SceneAsset.hpp>
#include <render/RenderWorld.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <string>

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
    stylized::material::MaterialKind& materialKind) const
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

    int materialMode =
        materialKind ==
            stylized::material::MaterialKind::DebugNormal
        ? 1
        : 0;

    constexpr const char* materialModes[] = {
        "Unlit",
        "Debug Normal"
    };

    ImGui::Separator();

    if (ImGui::Combo(
            "Material Mode",
            &materialMode,
            materialModes,
            IM_ARRAYSIZE(materialModes)))
    {
        materialKind = materialMode == 1
            ? stylized::material::MaterialKind::DebugNormal
            : stylized::material::MaterialKind::Unlit;
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

