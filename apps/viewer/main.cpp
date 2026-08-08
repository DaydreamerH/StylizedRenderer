#include <asset/AssetRegistry.hpp>
#include <asset/SceneAsset.hpp>
#include <asset/importers/ModelImporter.hpp>

#include <core/Application.hpp>

#include <math/Bounds.hpp>

#include <platform/Window.hpp>

#include <render/world/RenderExtractor.hpp>
#include <render/world/RenderWorld.hpp>
#include <render/resources/RuntimeResourceCache.hpp>
#include <render/renderers/StaticModelRenderer.hpp>
#include <render/passes/ForwardOpaquePass.hpp>
#include <render/pipeline/FrameContext.hpp>
#include <render/pipeline/FramePipeline.hpp>
#include <render/passes/ShadowPass.hpp>
#include <render/passes/PostProcessPass.hpp>

#include <scene/Camera.hpp>

#include <material/MaterialTemplate.hpp>

#include "OrbitCameraController.hpp"
#include "ViewerPanels.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

namespace
{

stylized::core::ApplicationDesc makeApplicationDesc(
    const bool smokeTest)
{
    stylized::core::ApplicationDesc desc;

    desc.title = "StylizedRenderer";
    desc.width = 1280;
    desc.height = 720;
    desc.visible = !smokeTest;
    desc.vsync = !smokeTest;

    return desc;
}

class ViewerApplication final
    : public stylized::core::Application
{
public:
    ViewerApplication(
        const bool smokeTest,
        std::filesystem::path modelPath)
        : Application(makeApplicationDesc(smokeTest)),
          smokeTest_(smokeTest),
          modelPath_(std::move(modelPath))
    {
    }

protected:
    bool onInit() override
    {
        if (!smokeTest_ && modelPath_.empty())
        {
            std::cerr
                << "Usage: stylized_viewer "
                << "<model.gltf|model.glb>\n";

            return false;
        }

        if (!createRuntimeResources())
        {
            return false;
        }

        if (!viewerPanels_.initialize(
                window().nativeHandle()))
        {
            std::cerr
                << "Failed to initialize Viewer panels.\n";

            return false;
        }

        if (!smokeTest_)
        {
            if (!loadScene())
            {
                return false;
            }
        }

        return true;
    }

    void onUpdate(const float) override
    {
        cameraController_.update(window());

        if (window().isKeyPressed(
                stylized::platform::Key::Escape))
        {
            requestExit();
        }
    }

    void onRender() override
    {
        std::uint32_t framebufferWidth = 0;
        std::uint32_t framebufferHeight = 0;

        window().getFramebufferSize(framebufferWidth, framebufferHeight);

        if (framebufferWidth == 0 || framebufferHeight == 0) return;

        if (smokeTest_)
        {
            ++renderedFrameCount_;

            if (renderedFrameCount_ >= 3) requestExit();

            return;
        }

        const stylized::asset::SceneAsset* sceneAsset =
            assetRegistry_.get(sceneHandle_);

        if (sceneAsset == nullptr)
        {
            std::cerr << "SceneAsset is no longer available.\n";

            requestExit();
            return;
        }

        if (!extractor_->extract(
                *sceneAsset,
                assetRegistry_,
                camera_,
                mainLight_,
                activeMaterialTemplateHandle_,
                renderWorld_))
        {
            std::cerr << "Failed to extract RenderWorld.\n";

            requestExit();
            return;
        }

        if (!cameraFocused_)
        {
            stylized::math::Bounds sceneBounds;
            for (const stylized::render::RenderItem& item : renderWorld_.items)
                sceneBounds.expand(item.worldBounds);

            if (sceneBounds.isValid())
            {
                cameraController_.focus(sceneBounds);
                cameraFocused_ = true;
            }
        }

        const stylized::graphics::Extent2D framebufferExtent{
            framebufferWidth,
            framebufferHeight
        };

        const bool extentChanged =
            pipelineExtent_.width != framebufferExtent.width ||
            pipelineExtent_.height != framebufferExtent.height;

        if (extentChanged)
        {
            if (!framePipeline_->resize(framebufferExtent))
            {
                std::cerr << "Failed to resize frame pipeline.\n";

                requestExit();
                return;
            }

            pipelineExtent_ = framebufferExtent;
        }

        stylized::render::FrameContext frame;
        frame.framebufferSize = framebufferExtent;
        frame.renderWorld = &renderWorld_;
        frame.framebuffer = nullptr;
        frame.hdrColor = nullptr;
        frame.deltaTime = 0.F;
        frame.shadowsEnabled =
            shadowsEnabled_;
        frame.exposure = exposure_;
        frame.toneMappingEnabled =
            toneMappingEnabled_;

        if (!framePipeline_->execute(frame))
        {
            std::cerr << "Failed to execute frame pipeline.\n";

            requestExit();
            return;
        }

        if (forwardOpaquePass_ != nullptr)
        {
            renderWorld_.renderStats.drawCalls =
                forwardOpaquePass_->lastDrawCallCount();
        }

        viewerPanels_.beginFrame();

        viewerPanels_.draw(
            modelPath_,
            assetRegistry_,
            sceneAsset,
            renderWorld_,
            renderWorld_.renderStats.drawCalls,
            framePipeline_.get(),
            shadowPass_,
            forwardOpaquePass_,
            postProcessPass_,
            activeMaterialKind_,
            shadowsEnabled_,
            exposure_,
            toneMappingEnabled_);

        if (!updateActiveMaterialTemplate())
        {
            std::cerr
                << "Failed to select material template.\n";

            viewerPanels_.endFrame();
            requestExit();
            return;
        }

        viewerPanels_.endFrame();

        ++statsPrintFrameCount_;

        if (statsPrintFrameCount_ % 60 == 0)
        {
            const stylized::render::RenderStats& stats =
                renderWorld_.renderStats;

            std::cout
                << "Render stats: total="
                << stats.totalItems
                << ", visible="
                << stats.visibleItems
                << ", culled="
                << stats.culledItems
                << ", drawCalls="
                << stats.drawCalls
                << '\n';
        }
    }

    void onShutdown() override
    {
        viewerPanels_.shutdown();
        framePipeline_.reset();
        shadowPass_ = nullptr;
        forwardOpaquePass_ = nullptr;
        postProcessPass_ = nullptr;
        extractor_.reset();
        resourceCache_.reset();
    }

private:
    bool createRuntimeResources()
    {
        if (!createMaterialTemplates())
        {
            std::cerr
                << "Failed to create material templates.\n";

            return false;
        }

        resourceCache_ =
            std::make_unique<
                stylized::render::RuntimeResourceCache>(
                graphicsDevice());

        if (!resourceCache_->initialize())
        {
            std::cerr
                << "Failed to initialize runtime "
                << "resource cache.\n";

            return false;
        }

        extractor_ =
            std::make_unique<
                stylized::render::RenderExtractor>(
                *resourceCache_);

        framePipeline_ =
            std::make_unique<stylized::render::FramePipeline>(
                graphicsDevice());

        auto shadowPass =
            std::make_unique<
                stylized::render::ShadowPass>(
                    graphicsDevice());

        if (!shadowPass->initialize())
        {
            return false;
        }

        shadowPass_ = shadowPass.get();

        if (!framePipeline_->addPass(
                std::move(shadowPass)))
        {
            return false;
        }

        auto forwardPass = std::make_unique<
            stylized::render::ForwardOpaquePass>(
                graphicsDevice(),
                assetRegistry_,
                *resourceCache_);
        if (!forwardPass->initialize()) return false;

        forwardOpaquePass_ = forwardPass.get();

        if (!framePipeline_->addPass(std::move(forwardPass)))
            return false;

        auto postProcessPass =
            std::make_unique<
                stylized::render::PostProcessPass>(
                    graphicsDevice());

        if (!postProcessPass->initialize())
        {
            std::cerr
                << "Failed to initialize "
                << "PostProcessPass.\n";

            return false;
        }

        postProcessPass_ = postProcessPass.get();

        if (!framePipeline_->addPass(
                std::move(postProcessPass)))
        {
            return false;
        }

        return true;
    }

    bool loadScene()
    {
        stylized::asset::importers::ModelImporter importer{
            assetRegistry_
        };

        sceneHandle_ =
            importer.import(modelPath_);

        if (sceneHandle_.isNull())
        {
            std::cerr
                << "Failed to import model: "
                << modelPath_
                << '\n';

            return false;
        }

        const stylized::asset::SceneAsset* sceneAsset =
            assetRegistry_.get(sceneHandle_);

        if (sceneAsset == nullptr ||
            !sceneAsset->isValid())
        {
            std::cerr
                << "Imported SceneAsset is invalid.\n";

            return false;
        }

        std::cout
            << "Scene loaded successfully: "
            << sceneAsset->name
            << '\n'
            << "Node count: "
            << sceneAsset->nodes.size()
            << '\n';

        return true;
    }

    bool createMaterialTemplates()
    {
        stylized::material::MaterialTemplate unlitTemplate;

        unlitTemplate.name = "Default Unlit";
        unlitTemplate.kind = stylized::material::MaterialKind::Unlit;

        unlitTemplate.vertexShaderPath =
            "assets/shaders/static_model/static_model.vert";
        unlitTemplate.fragmentShaderPath =
            "assets/shaders/material/unlit.frag";

        if (!unlitTemplate.isValid()) return false;

        unlitTemplateHandle_ = assetRegistry_.emplace<
            stylized::material::MaterialTemplate>(
                std::move(unlitTemplate));
        if (unlitTemplateHandle_.isNull()) return false;

        stylized::material::MaterialTemplate
            debugNormalTemplate;

        debugNormalTemplate.name = "Debug Normal";

        debugNormalTemplate.kind =
            stylized::material::MaterialKind::DebugNormal;

        debugNormalTemplate.vertexShaderPath =
            "assets/shaders/static_model/static_model.vert";

        debugNormalTemplate.fragmentShaderPath =
            "assets/shaders/material/debug_normal.frag";

        if (!debugNormalTemplate.isValid()) return false;

        debugNormalTemplateHandle_ = assetRegistry_.emplace<
            stylized::material::MaterialTemplate>(
                std::move(debugNormalTemplate));

        if (debugNormalTemplateHandle_.isNull()) return false;

        stylized::material::MaterialTemplate basicPbrTemplate;

        basicPbrTemplate.name = "Basic PBR";

        basicPbrTemplate.kind =
            stylized::material::MaterialKind::BasicPbr;

        basicPbrTemplate.vertexShaderPath =
            "assets/shaders/static_model/static_model.vert";

        basicPbrTemplate.fragmentShaderPath =
            "assets/shaders/material/basic_pbr.frag";

        if (!basicPbrTemplate.isValid())
        {
            return false;
        }

        basicPbrTemplateHandle_ = assetRegistry_.emplace<
            stylized::material::MaterialTemplate>(
                std::move(basicPbrTemplate));

        if (basicPbrTemplateHandle_.isNull())
        {
            return false;
        }

        activeMaterialKind_ =
            stylized::material::MaterialKind::Unlit;

        return updateActiveMaterialTemplate();
    }

    bool updateActiveMaterialTemplate() noexcept
    {
        switch (activeMaterialKind_)
        {
        case stylized::material::MaterialKind::Unlit:
            activeMaterialTemplateHandle_ =
                unlitTemplateHandle_;
            break;

        case stylized::material::MaterialKind::DebugNormal:
            activeMaterialTemplateHandle_ =
                debugNormalTemplateHandle_;
            break;

        case stylized::material::MaterialKind::BasicPbr:
            activeMaterialTemplateHandle_ =
                basicPbrTemplateHandle_;
            break;
        }

        return !activeMaterialTemplateHandle_.isNull();
    }

    bool smokeTest_ = false;
    int renderedFrameCount_ = 0;
    std::uint64_t statsPrintFrameCount_ = 0;

    std::filesystem::path modelPath_;

    stylized::asset::AssetRegistry assetRegistry_;

    stylized::asset::AssetHandle<
        stylized::asset::SceneAsset>
        sceneHandle_;

    std::unique_ptr<
        stylized::render::RuntimeResourceCache>
        resourceCache_;

    std::unique_ptr<
        stylized::render::RenderExtractor>
        extractor_;

    std::unique_ptr<
        stylized::render::FramePipeline>
        framePipeline_;

    stylized::asset::AssetHandle<
        stylized::material::MaterialTemplate>
        unlitTemplateHandle_;

    stylized::asset::AssetHandle<
        stylized::material::MaterialTemplate>
        debugNormalTemplateHandle_;

    stylized::asset::AssetHandle<
        stylized::material::MaterialTemplate>
        basicPbrTemplateHandle_;

    stylized::asset::AssetHandle<
        stylized::material::MaterialTemplate>
        activeMaterialTemplateHandle_;

    stylized::material::MaterialKind activeMaterialKind_ =
        stylized::material::MaterialKind::Unlit;

    bool shadowsEnabled_ = true;
    float exposure_ = 1.0F;
    bool toneMappingEnabled_ = true;

    stylized::render::ShadowPass* shadowPass_ = nullptr;
    stylized::render::ForwardOpaquePass* forwardOpaquePass_ = nullptr;
    stylized::render::PostProcessPass* postProcessPass_ = nullptr;

    stylized::graphics::Extent2D pipelineExtent_{};

    stylized::render::RenderWorld renderWorld_;

    stylized::render::DirectionalLightData mainLight_{
        .direction = {
            -0.4F,
            -1.0F,
            -0.6F
        },
        .color = {
            1.0F,
            1.0F,
            1.0F
        },
        .intensity = 2.0F
    };

    ViewerPanels viewerPanels_;

    stylized::scene::Camera camera_;
    OrbitCameraController cameraController_{
        camera_
    };

    bool cameraFocused_ = false;
};

} // namespace

int main(
    const int argc,
    char* argv[])
{
    bool smokeTest = false;
    std::filesystem::path modelPath;

    for (int argumentIndex = 1;
         argumentIndex < argc;
         ++argumentIndex)
    {
        const std::string_view argument{
            argv[argumentIndex]
        };

        if (argument == "--smoke-test")
        {
            smokeTest = true;
            continue;
        }

        if (modelPath.empty())
        {
            modelPath =
                std::filesystem::path{
                    argument
                };
        }
    }

    ViewerApplication application{
        smokeTest,
        modelPath
    };

    return application.run();
}
