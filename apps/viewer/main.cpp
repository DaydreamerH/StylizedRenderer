#include <asset/AssetRegistry.hpp>
#include <asset/SceneAsset.hpp>
#include <asset/importers/ModelImporter.hpp>

#include <core/Application.hpp>

#include <math/Bounds.hpp>

#include <platform/Window.hpp>

#include <render/RenderExtractor.hpp>
#include <render/RenderWorld.hpp>
#include <render/RuntimeResourceCache.hpp>
#include <render/StaticModelRenderer.hpp>

#include <scene/Camera.hpp>

#include "OrbitCameraController.hpp"
#include "ViewerPanels.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>

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
        graphicsDevice().clear(
            {
                0.06F,
                0.07F,
                0.10F,
                1.0F
            });

        if (smokeTest_)
        {
            ++renderedFrameCount_;

            if (renderedFrameCount_ >= 3)
            {
                requestExit();
            }

            return;
        }

        const stylized::asset::SceneAsset* sceneAsset =
            assetRegistry_.get(sceneHandle_);

        if (sceneAsset == nullptr)
        {
            requestExit();
            return;
        }

        if (!extractor_->extract(
                *sceneAsset,
                assetRegistry_,
                camera_,
                renderWorld_))
        {
            std::cerr
                << "Failed to extract RenderWorld.\n";

            requestExit();
            return;
        }

        if (!cameraFocused_)
        {
            stylized::math::Bounds sceneBounds;

            for (const stylized::render::RenderItem& item :
                 renderWorld_.items)
            {
                sceneBounds.expand(item.worldBounds);
            }

            if (sceneBounds.isValid())
            {
                cameraController_.focus(sceneBounds);
                cameraFocused_ = true;
            }
        }

        if (!renderer_->render(renderWorld_))
        {
            std::cerr
                << "Failed to render RenderWorld.\n";

            requestExit();
            return;
        }

        renderWorld_.renderStats.drawCalls =
            renderer_->lastDrawCallCount();

        viewerPanels_.beginFrame();
        viewerPanels_.draw(
            modelPath_,
            assetRegistry_,
            sceneAsset,
            renderWorld_,
            renderer_->lastDrawCallCount());
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
        renderer_.reset();
        extractor_.reset();
        resourceCache_.reset();
    }

private:
    bool createRuntimeResources()
    {
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

        renderer_ =
            std::make_unique<
                stylized::render::StaticModelRenderer>(
                graphicsDevice(),
                assetRegistry_,
                *resourceCache_);

        if (!renderer_->initialize())
        {
            std::cerr
                << "Failed to initialize static model renderer.\n";

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
        stylized::render::StaticModelRenderer>
        renderer_;

    stylized::render::RenderWorld renderWorld_;

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
