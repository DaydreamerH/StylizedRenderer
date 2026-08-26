#include <asset/AssetRegistry.hpp>
#include <asset/MaterialAsset.hpp>
#include <asset/MeshAsset.hpp>
#include <asset/SceneAsset.hpp>
#include <asset/importers/ModelImporter.hpp>

#include <core/Application.hpp>

#include <math/Bounds.hpp>

#include <platform/Window.hpp>

#include <render/world/RenderExtractor.hpp>
#include <render/world/RenderWorld.hpp>
#include <render/resources/RuntimeResourceCache.hpp>
#include <render/resources/RuntimeMeshInstance.hpp>
#include <render/resources/SkinningPaletteSet.hpp>
#include <render/renderers/StaticModelRenderer.hpp>
#include <render/passes/ForwardOpaquePass.hpp>
#include <render/passes/ForwardTransparentPass.hpp>
#include <render/passes/FaceHairShadowPass.hpp>
#include <render/passes/FxaaPass.hpp>
#include <render/pipeline/FrameContext.hpp>
#include <render/pipeline/FramePipeline.hpp>
#include <render/passes/ShadowPass.hpp>
#include <render/passes/PostProcessPass.hpp>
#include <render/passes/OutlineMaskPass.hpp>
#include <render/passes/ScreenSpaceOutlinePass.hpp>

#include <scene/Camera.hpp>

#include <material/MaterialTemplate.hpp>

#include <animation/AnimationPlayer.hpp>
#include <animation/SceneMorphPose.hpp>
#include <animation/ScenePose.hpp>

#include <nlohmann/json.hpp>

#include "camera/OrbitCameraController.hpp"
#include "ui/ViewerPanels.hpp"
#include "scene/SceneRuntimeInstance.hpp"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace
{

using CpuClock = std::chrono::steady_clock;

void updateSmoothedTiming(
    double& averageMilliseconds,
    const double sampleMilliseconds) noexcept
{
    constexpr double smoothingFactor = 0.1;

    if (averageMilliseconds <= 0.0)
    {
        averageMilliseconds = sampleMilliseconds;
        return;
    }

    averageMilliseconds +=
        (sampleMilliseconds - averageMilliseconds) *
        smoothingFactor;
}

void updateCpuTiming(
    double& averageMilliseconds,
    const CpuClock::time_point startTime) noexcept
{
    const double sampleMilliseconds =
        std::chrono::duration<double, std::milli>{
            CpuClock::now() - startTime
        }.count();

    updateSmoothedTiming(
        averageMilliseconds,
        sampleMilliseconds);
}

[[nodiscard]]
double elapsedMilliseconds(
    const CpuClock::time_point startTime) noexcept
{
    return std::chrono::duration<double, std::milli>{
        CpuClock::now() - startTime
    }.count();
}

struct CameraJsonKeyframe
{
    int frame = 0;
    float timeFromStartSeconds = 0.0F;
    glm::vec3 translation{0.0F};
    glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
    float verticalFieldOfView = 30.04F;
    std::string interpolation;
};

struct CameraJsonTrack
{
    std::filesystem::path sourcePath;
    std::string cameraName;
    std::string coordinateSystem;
    float fps = 0.0F;
    int frameStart = 0;
    int frameEnd = 0;
    float durationSeconds = 0.0F;
    float nearPlane = 0.1F;
    float farPlane = 2000.0F;
    std::vector<CameraJsonKeyframe> keyframes;
};

enum class CameraJsonRotationOrder
{
    Xyzw,
    Wxyz
};

[[nodiscard]]
bool finiteVector(const glm::vec3& value) noexcept
{
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

[[nodiscard]]
bool finiteQuaternion(const glm::quat& value) noexcept
{
    return std::isfinite(value.w) &&
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

[[nodiscard]]
bool loadCameraJson(
    const std::filesystem::path& jsonPath,
    CameraJsonTrack& track)
{
    track = {};
    track.sourcePath = jsonPath;

    if (!std::filesystem::exists(jsonPath))
    {
        std::cerr
            << "Camera JSON does not exist: "
            << jsonPath
            << '\n';
        return false;
    }

    std::ifstream input(jsonPath);
    if (!input.is_open())
    {
        std::cerr
            << "Failed to open camera JSON: "
            << jsonPath
            << '\n';
        return false;
    }

    try
    {
        const nlohmann::json document =
            nlohmann::json::parse(input);

        if (!document.is_object() ||
            document.value("version", 0) != 2 ||
            document.value("coordinateSystem", std::string{}) !=
                "gltf-y-up" ||
            !document.contains("rotationOrder") ||
            !document["rotationOrder"].is_string() ||
            !document.contains("camera") ||
            !document["camera"].is_string() ||
            !document.contains("keyframes") ||
            !document["keyframes"].is_array())
        {
            throw std::runtime_error(
                "camera JSON must be version 2 gltf-y-up with "
                "a rotationOrder and keyframes");
        }

        const std::string rotationOrder =
            document.at("rotationOrder").get<std::string>();

        CameraJsonRotationOrder parsedRotationOrder;

        if (rotationOrder == "xyzw")
        {
            parsedRotationOrder =
                CameraJsonRotationOrder::Xyzw;
        }
        else if (rotationOrder == "wxyz")
        {
            parsedRotationOrder =
                CameraJsonRotationOrder::Wxyz;
        }
        else
        {
            throw std::runtime_error(
                "camera JSON rotationOrder must be xyzw or wxyz");
        }

        track.cameraName =
            document.at("camera").get<std::string>();
        track.coordinateSystem =
            document.at("coordinateSystem").get<std::string>();
        track.fps = document.at("fps").get<float>();
        track.frameStart = document.at("frameStart").get<int>();
        track.frameEnd = document.at("frameEnd").get<int>();
        track.durationSeconds =
            document.at("durationSeconds").get<float>();
        track.nearPlane = document.at("nearPlane").get<float>();
        track.farPlane = document.at("farPlane").get<float>();

        if (track.cameraName.empty() ||
            !std::isfinite(track.fps) ||
            track.fps <= 0.0F ||
            track.frameStart > track.frameEnd ||
            !std::isfinite(track.durationSeconds) ||
            track.durationSeconds < 0.0F ||
            !std::isfinite(track.nearPlane) ||
            !std::isfinite(track.farPlane) ||
            track.nearPlane <= 0.0F ||
            track.farPlane <= track.nearPlane)
        {
            throw std::runtime_error(
                "invalid camera timeline or projection values");
        }

        const nlohmann::json& keyframes =
            document.at("keyframes");

        if (keyframes.empty())
        {
            throw std::runtime_error(
                "camera JSON has no keyframes");
        }

        track.keyframes.reserve(keyframes.size());

        int previousFrame =
            std::numeric_limits<int>::min();
        float previousTime =
            -std::numeric_limits<float>::infinity();

        for (const nlohmann::json& source : keyframes)
        {
            if (!source.is_object() ||
                !source.contains("frame") ||
                !source.contains("timeFromStartSeconds") ||
                !source.contains("translation") ||
                !source.contains("rotation") ||
                !source.contains("verticalFovDegrees") ||
                !source.contains("interpolation") ||
                !source["translation"].is_array() ||
                source["translation"].size() != 3 ||
                !source["rotation"].is_array() ||
                source["rotation"].size() != 4)
            {
                throw std::runtime_error(
                    "invalid camera keyframe structure");
            }

            CameraJsonKeyframe keyframe;
            keyframe.frame = source.at("frame").get<int>();
            keyframe.timeFromStartSeconds =
                source.at("timeFromStartSeconds").get<float>();
            keyframe.translation = {
                source["translation"][0].get<float>(),
                source["translation"][1].get<float>(),
                source["translation"][2].get<float>()};

            const glm::vec4 sourceRotation{
                source["rotation"][0].get<float>(),
                source["rotation"][1].get<float>(),
                source["rotation"][2].get<float>(),
                source["rotation"][3].get<float>()};

            keyframe.rotation =
                parsedRotationOrder ==
                        CameraJsonRotationOrder::Xyzw
                    ? glm::quat{
                        sourceRotation.w,
                        sourceRotation.x,
                        sourceRotation.y,
                        sourceRotation.z}
                    : glm::quat{
                        sourceRotation.x,
                        sourceRotation.y,
                        sourceRotation.z,
                        sourceRotation.w};

            keyframe.verticalFieldOfView =
                source.at("verticalFovDegrees").get<float>();
            keyframe.interpolation =
                source.at("interpolation").get<std::string>();

            if ((keyframe.interpolation != "LINEAR" &&
                 keyframe.interpolation != "STEP") ||
                !std::isfinite(keyframe.timeFromStartSeconds) ||
                keyframe.frame <= previousFrame ||
                keyframe.timeFromStartSeconds <= previousTime ||
                !finiteVector(keyframe.translation) ||
                !finiteQuaternion(keyframe.rotation) ||
                glm::dot(
                    keyframe.rotation,
                    keyframe.rotation) <= 1.0e-8F ||
                keyframe.verticalFieldOfView < 1.0F ||
                keyframe.verticalFieldOfView > 179.0F)
            {
                throw std::runtime_error(
                    "invalid camera keyframe values");
            }

            keyframe.rotation = glm::normalize(
                keyframe.rotation);

            track.keyframes.push_back(keyframe);
            previousFrame = keyframe.frame;
            previousTime = keyframe.timeFromStartSeconds;
        }

        if (track.keyframes.front().frame != track.frameStart ||
            track.keyframes.back().frame != track.frameEnd ||
            std::abs(
                track.keyframes.front().timeFromStartSeconds) >
                1.0e-4F ||
            std::abs(
                track.keyframes.back().timeFromStartSeconds -
                track.durationSeconds) > 1.0e-4F)
        {
            throw std::runtime_error(
                "camera keyframes do not cover frameStart/frameEnd");
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Failed to parse camera JSON: "
            << jsonPath
            << " ("
            << exception.what()
            << ")\n";
        return false;
    }

    std::cout
        << "Camera JSON loaded: "
        << jsonPath
        << " ("
        << track.keyframes.size()
        << " keyframes, frames "
        << track.frameStart
        << ".."
        << track.frameEnd
        << ")\n";

    return true;
}

[[nodiscard]]
bool updateCameraFromJson(
    const CameraJsonTrack& track,
    const float timeSeconds,
    const glm::mat4& rootMatrix,
    stylized::scene::Camera& camera) noexcept
{
    const auto& keyframes = track.keyframes;
    const CameraJsonKeyframe* left = &keyframes.front();
    const CameraJsonKeyframe* right = left;
    float factor = 0.0F;

    if (keyframes.size() > 1 &&
        timeSeconds > keyframes.front().timeFromStartSeconds &&
        timeSeconds < keyframes.back().timeFromStartSeconds)
    {
        const auto rightIterator = std::lower_bound(
            keyframes.begin(),
            keyframes.end(),
            timeSeconds,
            [](const CameraJsonKeyframe& keyframe,
               const float time)
            {
                return keyframe.timeFromStartSeconds < time;
            });

        if (rightIterator != keyframes.end())
        {
            if (std::abs(
                    rightIterator->timeFromStartSeconds -
                    timeSeconds) <= 1.0e-6F)
            {
                left = &*rightIterator;
                right = left;
            }
            else
            {
                right = &*rightIterator;
                left = &*(rightIterator - 1);

                const float interval =
                    right->timeFromStartSeconds -
                    left->timeFromStartSeconds;

                if (interval > 1.0e-6F)
                {
                    factor = std::clamp(
                        (timeSeconds -
                            left->timeFromStartSeconds) /
                            interval,
                        0.0F,
                        1.0F);
                }
            }
        }
    }
    else if (timeSeconds >= keyframes.back().timeFromStartSeconds)
    {
        left = &keyframes.back();
        right = left;
    }

    glm::vec3 translation = left->translation;
    glm::quat rotation = left->rotation;
    float verticalFieldOfView = left->verticalFieldOfView;

    if (left != right && left->interpolation == "LINEAR")
    {
        translation = glm::mix(
            left->translation,
            right->translation,
            factor);

        glm::quat rightRotation = right->rotation;
        if (glm::dot(rotation, rightRotation) < 0.0F)
        {
            rightRotation = -rightRotation;
        }

        rotation = glm::normalize(glm::slerp(
            rotation,
            rightRotation,
            factor));

        verticalFieldOfView = std::lerp(
            left->verticalFieldOfView,
            right->verticalFieldOfView,
            factor);
    }

    const glm::vec3 position = glm::vec3(
        rootMatrix *
        glm::vec4(translation, 1.0F));

    const glm::mat3 rootBasis{rootMatrix};
    glm::vec3 forward =
        rootBasis *
        (rotation * glm::vec3{0.0F, 0.0F, -1.0F});
    glm::vec3 up =
        rootBasis *
        (rotation * glm::vec3{0.0F, 1.0F, 0.0F});

    if (glm::dot(forward, forward) <= 1.0e-8F ||
        glm::dot(up, up) <= 1.0e-8F)
    {
        return false;
    }

    forward = glm::normalize(forward);
    up = glm::normalize(up);

    return camera.setPerspective(
               verticalFieldOfView,
               camera.aspectRatio(),
               track.nearPlane,
               track.farPlane) &&
        camera.setView(position, position + forward, up);
}

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
        std::vector<std::filesystem::path> modelPaths,
        std::filesystem::path cameraJsonPath)
        : Application(makeApplicationDesc(smokeTest)),
          smokeTest_(smokeTest),
          modelPaths_(std::move(modelPaths)),
          cameraJsonPath_(std::move(cameraJsonPath))
    {
    }

protected:
    bool onInit() override
    {
        if (modelPaths_.empty())
        {
            std::cerr
                << "Usage: stylized_viewer "
                << "<model-file> [additional-model-files...] "
                << "[--camera-json camera.json]\n";

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

        for (const std::filesystem::path& modelPath :
             modelPaths_)
        {
            if (!loadScene(modelPath))
            {
                return false;
            }
        }

        if (!cameraJsonPath_.empty() &&
            !loadCameraTrack())
        {
            return false;
        }

        return true;
    }

    void onUpdate(
        const float deltaTime) override
    {
        const CpuClock::time_point updateStart =
            CpuClock::now();

        updateSmoothedTiming(
            cpuTimings_.frameIntervalMilliseconds,
            static_cast<double>(deltaTime) * 1000.0);

        stylized::viewer::SceneRuntimeInstance*
            sceneInstance =
                primarySceneInstance();

        if (sceneInstance == nullptr)
        {
            requestExit();
            return;
        }

        const bool spaceKeyPressed =
            window().isKeyPressed(
                stylized::platform::Key::Space);

        if (spaceKeyPressed &&
            !spaceKeyPressed_)
        {
            bool anyAnimationPlaying = false;

            for (const auto& currentInstance :
                 sceneInstances_)
            {
                if (currentInstance->animationPlayer.clip() != nullptr &&
                    currentInstance->animationPlayer.isPlaying())
                {
                    anyAnimationPlaying = true;
                    break;
                }
            }

            for (const auto& currentInstance :
                 sceneInstances_)
            {
                if (currentInstance->animationPlayer.clip() == nullptr)
                {
                    continue;
                }

                if (anyAnimationPlaying)
                {
                    currentInstance->animationPlayer.pause();
                }
                else
                {
                    currentInstance->animationPlayer.play();
                }
            }

        }

        spaceKeyPressed_ = spaceKeyPressed;

        double animationMilliseconds = 0.0;
        double skinningMilliseconds = 0.0;
        double morphMilliseconds = 0.0;

        for (const auto& currentInstance :
             sceneInstances_)
        {
            if (!updateSceneInstance(
                    deltaTime,
                    *currentInstance,
                    animationMilliseconds,
                    skinningMilliseconds,
                    morphMilliseconds))
            {
                requestExit();
                return;
            }
        }

        updateSmoothedTiming(
            cpuTimings_.animationMilliseconds,
            animationMilliseconds);

        updateSmoothedTiming(
            cpuTimings_.skinningMilliseconds,
            skinningMilliseconds);

        updateSmoothedTiming(
            cpuTimings_.morphMilliseconds,
            morphMilliseconds);

        if (!cameraTrack_.has_value())
        {
            // A scene camera exists only when an independent JSON track was
            // supplied. Character GLB cameras are deliberately ignored.
            useSceneCamera_ = false;
        }

        if (cameraTrack_.has_value())
        {
            std::uint32_t framebufferWidth = 0;
            std::uint32_t framebufferHeight = 0;

            window().getFramebufferSize(
                framebufferWidth,
                framebufferHeight);

            if (framebufferHeight > 0)
            {
                const float aspectRatio =
                    static_cast<float>(framebufferWidth) /
                    static_cast<float>(framebufferHeight);

                if (!sceneCamera_.setAspectRatio(aspectRatio))
                {
                    requestExit();
                    return;
                }
            }

            const float animationTimeSeconds =
                sceneInstance->animationPlayer.clip() != nullptr
                ? sceneInstance->animationPlayer.currentTime()
                : 0.0F;

            const float cameraTimeSeconds = std::min(
                animationTimeSeconds,
                cameraTrack_->durationSeconds);

            if (!updateCameraFromJson(
                    *cameraTrack_,
                    cameraTimeSeconds,
                    sceneInstance->rootTransform.localMatrix(),
                    sceneCamera_))
            {
                std::cerr
                    << "Failed to update scene camera from JSON.\n";
                requestExit();
                return;
            }
        }

        if (!useSceneCamera_)
        {
            cameraController_.update(
                window(),
                !viewerPanels_.wantsMouseCapture());
        }

        if (window().isKeyPressed(
                stylized::platform::Key::Escape))
        {
            requestExit();
        }

        updateCpuTiming(
            cpuTimings_.updateMilliseconds,
            updateStart);
    }

    void onRender() override
    {
        const CpuClock::time_point renderStart =
            CpuClock::now();

        std::uint32_t framebufferWidth = 0;
        std::uint32_t framebufferHeight = 0;

        window().getFramebufferSize(framebufferWidth, framebufferHeight);

        if (framebufferWidth == 0 || framebufferHeight == 0) return;

        stylized::viewer::SceneRuntimeInstance*
            sceneInstance =
                selectedSceneInstance();

        if (sceneInstance == nullptr)
        {
            requestExit();
            return;
        }

        const stylized::asset::SceneAsset* sceneAsset =
            assetRegistry_.get(sceneInstance->sceneHandle);

        if (sceneAsset == nullptr)
        {
            std::cerr << "SceneAsset is no longer available.\n";

            requestExit();
            return;
        }

        const CpuClock::time_point extractionStart =
            CpuClock::now();

        bool extracted = extractor_->beginFrame(
            activeCamera(),
            mainLight_,
            renderWorld_);

        if (extracted)
        {
            for (const auto& currentInstance :
                sceneInstances_)
            {
                const stylized::asset::SceneAsset*
                    currentSceneAsset =
                        assetRegistry_.get(
                            currentInstance->sceneHandle);

                if (currentSceneAsset == nullptr)
                {
                    extracted = false;
                    break;
                }

                std::optional<stylized::render::FaceSdfExtractionData>
                    faceSdf;

                if (currentInstance->faceSdf.isValid())
                {
                    faceSdf = stylized::render::FaceSdfExtractionData{
                        .material = currentInstance->faceSdf.material,
                        .headNodeIndex =
                            currentInstance->faceSdf.headNodeIndex,
                        .headRight = currentInstance->faceSdf.headRight,
                        .headForward =
                            currentInstance->faceSdf.headForward,
                        .hairShadowEnabled =
                            currentInstance->faceSdf.hairShadowEnabled,
                        .hairShadowCaster =
                            currentInstance->faceSdf.hairShadowCaster,
                        .hairShadowResolution =
                            currentInstance->faceSdf.hairShadowResolution,
                        .hairShadowLocalCenter = currentInstance
                            ->faceSdf.hairShadowLocalCenter,
                        .hairShadowUvOffset = currentInstance
                            ->faceSdf.hairShadowUvOffset,
                        .hairShadowWidth =
                            currentInstance->faceSdf.hairShadowWidth,
                        .hairShadowHeight =
                            currentInstance->faceSdf.hairShadowHeight,
                        .hairShadowDepth =
                            currentInstance->faceSdf.hairShadowDepth,
                        .hairShadowCameraDistance = currentInstance
                            ->faceSdf.hairShadowCameraDistance,
                        .hairShadowAlphaCutoff = currentInstance
                            ->faceSdf.hairShadowAlphaCutoff,
                        .hairShadowSoftness = currentInstance
                            ->faceSdf.hairShadowSoftness,
                        .hairShadowStrength = currentInstance
                            ->faceSdf.hairShadowStrength};
                }

                if (!extractor_->appendScene(
                        *currentSceneAsset,
                        currentInstance->scenePose,
                        currentInstance->skinningPalettes,
                        currentInstance->morphMeshInstances,
                        currentInstance->rootTransform.localMatrix(),
                        faceSdf.has_value() ? &*faceSdf : nullptr,
                        assetRegistry_,
                        activeMaterialTemplateHandle_,
                        renderWorld_))
                {
                    extracted = false;
                    break;
                }
            }
        }

        if (extracted)
        {
            extracted =
                extractor_->endFrame(renderWorld_);
        }

        updateCpuTiming(
            cpuTimings_.extractionMilliseconds,
            extractionStart);

        if (!extracted)
        {
            std::cerr << "Failed to extract RenderWorld.\n";

            requestExit();
            return;
        }

        if (!cameraFocused_ && !useSceneCamera_)
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
        frame.fxaaEnabled = fxaaEnabled_;

        const CpuClock::time_point pipelineStart =
            CpuClock::now();

        const bool pipelineExecuted =
            framePipeline_->execute(frame);

        updateCpuTiming(
            cpuTimings_.pipelineMilliseconds,
            pipelineStart);

        if (!pipelineExecuted)
        {
            std::cerr << "Failed to execute frame pipeline.\n";

            requestExit();
            return;
        }

        renderWorld_.renderStats.drawCalls = 0;

        if (forwardOpaquePass_ != nullptr)
        {
            renderWorld_.renderStats.drawCalls +=
                forwardOpaquePass_->lastDrawCallCount();
        }

        if (forwardTransparentPass_ != nullptr)
        {
            renderWorld_.renderStats.drawCalls +=
                forwardTransparentPass_->lastDrawCallCount();
        }

        const CpuClock::time_point uiStart =
            CpuClock::now();

        viewerPanels_.beginFrame();

        viewerPanels_.draw(
            std::span<const std::filesystem::path>{modelPaths_},
            selectedSceneInstanceIndex_,
            sceneInstance->rootTransform,
            assetRegistry_,
            *resourceCache_,
            activeMaterialTemplateHandle_,
            sceneAsset,
            cameraTrack_.has_value()
                ? std::string_view(cameraTrack_->cameraName)
                : std::string_view{},
            cameraTrack_.has_value(),
            sceneInstance->animationPlayer,
            sceneInstance->skinningPalettes,
            sceneInstance->morphMeshInstances,
            renderWorld_,
            mainLight_,
            renderWorld_.renderStats.drawCalls,
            cpuTimings_,
            framePipeline_.get(),
            shadowPass_,
            forwardOpaquePass_,
            forwardTransparentPass_,
            outlineMaskPass_,
            screenSpaceOutlinePass_,
            postProcessPass_,
            activeMaterialKind_,
            shadowsEnabled_,
            exposure_,
            toneMappingEnabled_,
            fxaaEnabled_,
            useSceneCamera_);

        if (!updateActiveMaterialTemplate())
        {
            std::cerr
                << "Failed to select material template.\n";

            viewerPanels_.endFrame();
            requestExit();
            return;
        }

        viewerPanels_.endFrame();

        updateCpuTiming(
            cpuTimings_.uiMilliseconds,
            uiStart);

        updateCpuTiming(
            cpuTimings_.renderMilliseconds,
            renderStart);

        if (smokeTest_)
        {
            ++renderedFrameCount_;

            if (renderedFrameCount_ >= 3)
            {
                std::size_t lastMorphUploads = 0;
                std::size_t totalMorphUploads = 0;

                for (const stylized::render::RuntimeMeshInstance& instance :
                     sceneInstance->morphMeshInstances)
                {
                    lastMorphUploads +=
                        instance.lastUploadCount();

                    totalMorphUploads +=
                        instance.totalUploadCount();
                }

                std::cout
                    << "Morph uploads: last="
                    << lastMorphUploads
                    << ", total="
                    << totalMorphUploads
                    << '\n';

                requestExit();
            }
        }
    }

    void onShutdown() override
    {
        viewerPanels_.shutdown();

        for (const auto& sceneInstance :
             sceneInstances_)
        {
            sceneInstance->morphMeshInstances.clear();
            sceneInstance->morphPose.clear();
        }

        sceneInstances_.clear();

        framePipeline_.reset();
        shadowPass_ = nullptr;
        forwardOpaquePass_ = nullptr;
        forwardTransparentPass_ = nullptr;
        outlineMaskPass_ = nullptr;
        screenSpaceOutlinePass_ = nullptr;
        postProcessPass_ = nullptr;
        extractor_.reset();
        resourceCache_.reset();
    }

private:
    bool createRuntimeResources()
    {
        stylized::graphics::ShaderProgramDesc
            morphComputeProgramDesc;
        morphComputeProgramDesc.computeShaderPath =
            "assets/shaders/morph/morph.comp";
        morphComputeProgramDesc.debugName =
            "GPU Morph";

        morphComputeProgram_ =
            graphicsDevice().createShaderProgram(
                morphComputeProgramDesc);

        if (!morphComputeProgram_.isValid())
        {
            std::cerr
                << "Failed to create GPU morph program.\n";

            return false;
        }

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
                    graphicsDevice(),
                    assetRegistry_,
                    *resourceCache_);

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

        auto faceHairShadowPass =
            std::make_unique<
                stylized::render::FaceHairShadowPass>(
                    graphicsDevice(),
                    assetRegistry_,
                    *resourceCache_);

        if (!faceHairShadowPass->initialize() ||
            !framePipeline_->addPass(
                std::move(faceHairShadowPass)))
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

        auto transparentPass =
            std::make_unique<
                stylized::render::ForwardTransparentPass>(
                    graphicsDevice(),
                    assetRegistry_,
                    *resourceCache_);

        if (!transparentPass->initialize())
        {
            return false;
        }

        forwardTransparentPass_ =
            transparentPass.get();

        if (!framePipeline_->addPass(
                std::move(transparentPass)))
        {
            return false;
        }

        auto outlineMaskPass =
            std::make_unique<
                stylized::render::OutlineMaskPass>(
                    graphicsDevice(),
                    assetRegistry_,
                    *resourceCache_);

        if (!outlineMaskPass->initialize())
        {
            std::cerr
                << "Failed to initialize "
                << "OutlineMaskPass.\n";

            return false;
        }

        outlineMaskPass_ =
            outlineMaskPass.get();

        if (!framePipeline_->addPass(
                std::move(outlineMaskPass)))
        {
            return false;
        }

        auto screenSpaceOutlinePass =
            std::make_unique<
                stylized::render::ScreenSpaceOutlinePass>(
                    graphicsDevice());

        if (!screenSpaceOutlinePass->initialize())
        {
            std::cerr
                << "Failed to initialize "
                << "ScreenSpaceOutlinePass.\n";

            return false;
        }

        screenSpaceOutlinePass_ =
            screenSpaceOutlinePass.get();

        if (!framePipeline_->addPass(
                std::move(screenSpaceOutlinePass)))
        {
            return false;
        }

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

        auto fxaaPass =
            std::make_unique<
                stylized::render::FxaaPass>(
                    graphicsDevice());

        if (!fxaaPass->initialize())
        {
            std::cerr
                << "Failed to initialize FxaaPass.\n";

            return false;
        }

        if (!framePipeline_->addPass(
                std::move(fxaaPass)))
        {
            return false;
        }

        return true;
    }

    bool loadScene(const std::filesystem::path& modelPath)
    {
        stylized::asset::importers::ModelImporter importer{
            assetRegistry_
        };

        const bool primaryScene =
            sceneInstances_.empty();

        auto sceneInstance =
            std::make_unique<
                stylized::viewer::SceneRuntimeInstance>();

        sceneInstance->sourcePath = modelPath;

        sceneInstance->sceneHandle =
            importer.import(modelPath);

        if (sceneInstance->sceneHandle.isNull())
        {
            std::cerr
                << "Failed to import model: "
                << modelPath
                << '\n';

            return false;
        }

        const stylized::asset::SceneAsset* sceneAsset =
            assetRegistry_.get(sceneInstance->sceneHandle);

        if (sceneAsset == nullptr ||
            !sceneAsset->isValid())
        {
            std::cerr
                << "Imported SceneAsset is invalid.\n";

            return false;
        }

        if (!viewerPanels_.loadMaterialSidecarForScene(
                modelPath,
                *sceneAsset,
                assetRegistry_,
                *resourceCache_,
                mtoonTemplateHandle_))
        {
            return false;
        }

        if (!loadCharacterSidecar(
                modelPath,
                *sceneAsset,
                *sceneInstance))
        {
            return false;
        }

        if (!sceneInstance->scenePose.initialize(*sceneAsset))
        {
            std::cerr
                << "Failed to initialize scene pose.\n";

            return false;
        }

        if (!sceneInstance->morphPose.initialize(*sceneAsset))
        {
            std::cerr
                << "Failed to initialize scene Morph pose.\n";

            return false;
        }

        sceneInstance->appliedMorphPoseVersion = 0;

        if (!sceneInstance->skinningPalettes.initialize(
                graphicsDevice(),
                *sceneAsset,
                assetRegistry_))
        {
            std::cerr
                << "Failed to initialize "
                << "skinning palettes.\n";

            return false;
        }

        if (!sceneInstance->skinningPalettes.update(
                *sceneAsset,
                sceneInstance->scenePose))
        {
            std::cerr
                << "Failed to upload bind-pose "
                << "skinning palettes.\n";

            return false;
        }

        sceneInstance->morphMeshInstances.clear();
        sceneInstance->morphMeshInstances.resize(
            sceneAsset->nodes.size());

        std::size_t morphPrimitiveCount = 0;

        for (std::size_t nodeIndex = 0;
             nodeIndex < sceneAsset->nodes.size();
             ++nodeIndex)
        {
            const stylized::asset::SceneNodeAsset& node =
                sceneAsset->nodes[nodeIndex];

            if (node.mesh.isNull())
            {
                continue;
            }

            const stylized::asset::MeshAsset* meshAsset =
                assetRegistry_.get(node.mesh);

            if (meshAsset == nullptr)
            {
                return false;
            }

            bool hasMorphTargets = false;

            for (const stylized::asset::MeshPrimitiveAsset& primitive :
                 meshAsset->primitives)
            {
                hasMorphTargets |=
                    primitive.hasMorphTargets();
            }

            if (!hasMorphTargets)
            {
                continue;
            }

            const stylized::render::RuntimeMesh* runtimeMesh =
                resourceCache_->getOrCreateMesh(
                    node.mesh,
                    assetRegistry_);

            if (runtimeMesh == nullptr ||
                !sceneInstance->morphMeshInstances[nodeIndex].initialize(
                    graphicsDevice(),
                    morphComputeProgram_,
                    *meshAsset,
                    *runtimeMesh))
            {
                std::cerr
                    << "Failed to initialize Morph mesh instance "
                    << "for node "
                    << node.name
                    << ".\n";

                return false;
            }

            morphPrimitiveCount +=
                sceneInstance->morphMeshInstances[nodeIndex]
                    .morphPrimitiveCount();
        }

        std::cout
            << "Morph primitive count: "
            << morphPrimitiveCount
            << '\n';

        if (smokeTest_ &&
            morphPrimitiveCount > 0)
        {
            for (stylized::render::RuntimeMeshInstance& instance :
                 sceneInstance->morphMeshInstances)
            {
                bool selectedTarget = false;

                for (std::size_t primitiveIndex = 0;
                     primitiveIndex < instance.primitiveCount();
                     ++primitiveIndex)
                {
                    stylized::animation::MorphState* state =
                        instance.morphState(primitiveIndex);

                    if (state == nullptr ||
                        state->targetCount() == 0)
                    {
                        continue;
                    }

                    if (!state->setWeight(0, 1.0F))
                    {
                        return false;
                    }

                    selectedTarget = true;
                    break;
                }

                if (selectedTarget)
                {
                    break;
                }
            }
        }

        std::cout
            << "Skinning palette count: "
            << sceneInstance->skinningPalettes.paletteCount()
            << '\n';

        if (!sceneAsset->animations.empty())
        {
            const stylized::asset::AnimationClipAsset&
                animationClip =
                    sceneAsset->animations.front();

            if (!sceneInstance->animationPlayer.setClip(
                    &animationClip))
            {
                std::cerr
                    << "Failed to select animation clip.\n";

                return false;
            }

            sceneInstance->animationPlayer.setLooping(true);
            sceneInstance->animationPlayer.play();

            std::cout
                << "Animation selected: "
                << animationClip.name
                << '\n';
        }

        std::cout
            << "Scene loaded successfully: "
            << sceneAsset->name
            << '\n'
            << "Node count: "
            << sceneAsset->nodes.size()
            << '\n';

        if (primaryScene)
        {
            // Scene cameras are supplied only through the independent camera
            // input. Loading a character GLB never changes camera state.
            useSceneCamera_ = false;

            cameraFocused_ = false;
        }

        sceneInstances_.push_back(
            std::move(sceneInstance));

        return true;
    }

    bool loadCharacterSidecar(
        const std::filesystem::path& modelPath,
        const stylized::asset::SceneAsset& sceneAsset,
        stylized::viewer::SceneRuntimeInstance& sceneInstance)
    {
        std::filesystem::path sidecarPath = modelPath;
        sidecarPath.replace_extension(".character.json");

        if (!std::filesystem::exists(sidecarPath))
        {
            return true;
        }

        try
        {
            std::ifstream input(sidecarPath);
            if (!input.is_open())
            {
                throw std::runtime_error(
                    "failed to open character sidecar");
            }

            const nlohmann::json document =
                nlohmann::json::parse(input);

            if (!document.is_object() ||
                document.value("version", 0) != 1 ||
                !document.contains("headNode") ||
                !document["headNode"].is_string() ||
                !document.contains("faceMaterial") ||
                !document["faceMaterial"].is_string() ||
                !document.contains("headRight") ||
                !document["headRight"].is_array() ||
                document["headRight"].size() != 3 ||
                !document.contains("headForward") ||
                !document["headForward"].is_array() ||
                document["headForward"].size() != 3)
            {
                throw std::runtime_error(
                    "invalid version 1 character sidecar structure");
            }

            const std::string headNodeName =
                document.at("headNode").get<std::string>();
            const std::string faceMaterialName =
                document.at("faceMaterial").get<std::string>();

            const auto readVector = [](
                const nlohmann::json& source)
            {
                return glm::vec3{
                    source[0].get<float>(),
                    source[1].get<float>(),
                    source[2].get<float>()};
            };

            const glm::vec3 headRight =
                readVector(document.at("headRight"));
            const glm::vec3 headForward =
                readVector(document.at("headForward"));

            if (headNodeName.empty() ||
                faceMaterialName.empty() ||
                !finiteVector(headRight) ||
                !finiteVector(headForward) ||
                glm::dot(headRight, headRight) <= 1.0e-8F ||
                glm::dot(headForward, headForward) <= 1.0e-8F ||
                glm::dot(
                    glm::cross(headRight, headForward),
                    glm::cross(headRight, headForward)) <= 1.0e-8F)
            {
                throw std::runtime_error(
                    "invalid character head axes or names");
            }

            std::uint32_t headNodeIndex =
                stylized::viewer::FaceSdfRuntimeConfig::invalidNodeIndex;

            for (std::size_t index = 0;
                 index < sceneAsset.nodes.size();
                 ++index)
            {
                if (sceneAsset.nodes[index].name != headNodeName)
                {
                    continue;
                }

                if (headNodeIndex !=
                    stylized::viewer::FaceSdfRuntimeConfig::invalidNodeIndex)
                {
                    throw std::runtime_error(
                        "character headNode is not unique");
                }

                headNodeIndex = static_cast<std::uint32_t>(index);
            }

            if (headNodeIndex ==
                stylized::viewer::FaceSdfRuntimeConfig::invalidNodeIndex)
            {
                throw std::runtime_error(
                    "character headNode was not found");
            }

            stylized::asset::AssetHandle<
                stylized::asset::MaterialAsset> faceMaterial;

            for (const stylized::asset::SceneNodeAsset& node :
                 sceneAsset.nodes)
            {
                const stylized::asset::MeshAsset* mesh =
                    assetRegistry_.get(node.mesh);

                if (mesh == nullptr)
                {
                    continue;
                }

                for (const stylized::asset::MeshPrimitiveAsset& primitive :
                     mesh->primitives)
                {
                    const stylized::asset::MaterialAsset* material =
                        assetRegistry_.get(primitive.material);

                    if (material == nullptr ||
                        material->name != faceMaterialName)
                    {
                        continue;
                    }

                    if (!faceMaterial.isNull() &&
                        faceMaterial != primitive.material)
                    {
                        throw std::runtime_error(
                            "character faceMaterial is not unique");
                    }

                    faceMaterial = primitive.material;
                }
            }

            if (faceMaterial.isNull())
            {
                throw std::runtime_error(
                    "character faceMaterial was not found");
            }

            if (document.contains("faceHairShadow"))
            {
                const auto& hairShadow =
                    document.at("faceHairShadow");

                if (!hairShadow.is_object())
                {
                    throw std::runtime_error(
                        "faceHairShadow must be an object");
                }

                if (hairShadow.value("enabled", false))
                {
                    const std::string casterName =
                        hairShadow.value(
                            "casterMaterial",
                            std::string{});

                    stylized::asset::AssetHandle<
                        stylized::asset::MaterialAsset> caster;

                    for (const auto& node : sceneAsset.nodes)
                    {
                        const auto* mesh =
                            assetRegistry_.get(node.mesh);
                        if (mesh == nullptr) continue;

                        for (const auto& primitive : mesh->primitives)
                        {
                            const auto* material =
                                assetRegistry_.get(primitive.material);
                            if (material != nullptr &&
                                material->name == casterName)
                            {
                                if (!caster.isNull() &&
                                    caster != primitive.material)
                                {
                                    throw std::runtime_error(
                                        "faceHairShadow caster is not unique");
                                }
                                caster = primitive.material;
                            }
                        }
                    }

                    const std::uint32_t resolution =
                        hairShadow.value("resolution", 512U);
                    const float width =
                        hairShadow.value("width", 0.11F);
                    const float height =
                        hairShadow.value("height", 0.14F);
                    const float depth =
                        hairShadow.value("depth", 0.25F);
                    const float cameraDistance =
                        hairShadow.value("cameraDistance", 0.12F);
                    const float alphaCutoff =
                        hairShadow.value("alphaCutoff", 0.72F);
                    const float softness =
                        hairShadow.value("softness", 0.004F);
                    const float strength =
                        hairShadow.value("strength", 0.8F);

                    const glm::vec3 localCenter =
                        readVector(hairShadow.at("localCenter"));

                    const auto& offsetSource =
                        hairShadow.at("uvOffset");
                    if (!offsetSource.is_array() ||
                        offsetSource.size() != 2)
                    {
                        throw std::runtime_error(
                            "faceHairShadow uvOffset must have two values");
                    }

                    const glm::vec2 uvOffset{
                        offsetSource[0].get<float>(),
                        offsetSource[1].get<float>()};

                    if (casterName.empty() || caster.isNull() ||
                        (resolution != 256U && resolution != 512U) ||
                        !finiteVector(localCenter) ||
                        !std::isfinite(uvOffset.x) ||
                        !std::isfinite(uvOffset.y) ||
                        !std::isfinite(width) || width <= 0.0F ||
                        !std::isfinite(height) || height <= 0.0F ||
                        !std::isfinite(depth) || depth <= 0.0F ||
                        !std::isfinite(cameraDistance) ||
                        cameraDistance <= 0.0F ||
                        !std::isfinite(alphaCutoff) ||
                        alphaCutoff < 0.0F || alphaCutoff > 1.0F ||
                        !std::isfinite(softness) || softness < 0.0F ||
                        !std::isfinite(strength) ||
                        strength < 0.0F || strength > 1.0F)
                    {
                        throw std::runtime_error(
                            "invalid faceHairShadow configuration");
                    }

                    sceneInstance.faceSdf.hairShadowEnabled = true;
                    sceneInstance.faceSdf.hairShadowCaster = caster;
                    sceneInstance.faceSdf.hairShadowResolution = resolution;
                    sceneInstance.faceSdf.hairShadowLocalCenter = localCenter;
                    sceneInstance.faceSdf.hairShadowUvOffset = uvOffset;
                    sceneInstance.faceSdf.hairShadowWidth = width;
                    sceneInstance.faceSdf.hairShadowHeight = height;
                    sceneInstance.faceSdf.hairShadowDepth = depth;
                    sceneInstance.faceSdf.hairShadowCameraDistance =
                        cameraDistance;
                    sceneInstance.faceSdf.hairShadowAlphaCutoff = alphaCutoff;
                    sceneInstance.faceSdf.hairShadowSoftness = softness;
                    sceneInstance.faceSdf.hairShadowStrength = strength;
                }
            }

            stylized::material::MaterialInstance* instance =
                resourceCache_->getOrCreateMaterialInstance(
                    faceMaterial,
                    mtoonTemplateHandle_,
                    assetRegistry_);

            if (instance == nullptr ||
                !instance->mtoonParameters.has_value())
            {
                throw std::runtime_error(
                    "face material has no MToon instance");
            }

            sceneInstance.faceSdf.material = faceMaterial;
            sceneInstance.faceSdf.headNodeIndex = headNodeIndex;
            sceneInstance.faceSdf.headRight = glm::normalize(headRight);
            sceneInstance.faceSdf.headForward =
                glm::normalize(headForward);

            std::cout
                << "Character face frame loaded: "
                << sidecarPath
                << " (material "
                << faceMaterialName
                << ")\n";
        }
        catch (const std::exception& exception)
        {
            std::cerr
                << "Failed to load character sidecar: "
                << sidecarPath
                << " ("
                << exception.what()
                << ")\n";
            return false;
        }

        return true;
    }

    bool loadCameraTrack()
    {
        CameraJsonTrack track;

        if (!loadCameraJson(cameraJsonPath_, track))
        {
            return false;
        }

        cameraTrack_ = std::move(track);
        useSceneCamera_ = false;

        return true;
    }

    bool updateSceneInstance(
        const float deltaTime,
        stylized::viewer::SceneRuntimeInstance&
            sceneInstance,
        double& animationMilliseconds,
        double& skinningMilliseconds,
        double& morphMilliseconds)
    {
        const stylized::asset::SceneAsset* sceneAsset =
            assetRegistry_.get(
                sceneInstance.sceneHandle);

        if (sceneAsset == nullptr)
        {
            std::cerr
                << "SceneAsset is no longer available: "
                << sceneInstance.sourcePath
                << '\n';

            return false;
        }

        if (sceneInstance.animationPlayer.clip() != nullptr)
        {
            const CpuClock::time_point animationStart =
                CpuClock::now();

            const bool animationUpdated =
                sceneInstance.animationPlayer.update(
                    deltaTime,
                    *sceneAsset,
                    sceneInstance.scenePose,
                    sceneInstance.morphPose);

            animationMilliseconds +=
                elapsedMilliseconds(animationStart);

            if (!animationUpdated)
            {
                std::cerr
                    << "Failed to update animation: "
                    << sceneInstance.sourcePath
                    << '\n';

                return false;
            }

            if (!applyMorphAnimation(
                    *sceneAsset,
                    sceneInstance))
            {
                std::cerr
                    << "Failed to apply Morph animation: "
                    << sceneInstance.sourcePath
                    << '\n';

                return false;
            }

            const CpuClock::time_point skinningStart =
                CpuClock::now();

            const bool skinningUpdated =
                sceneInstance.skinningPalettes.update(
                    *sceneAsset,
                    sceneInstance.scenePose);

            skinningMilliseconds +=
                elapsedMilliseconds(skinningStart);

            if (!skinningUpdated)
            {
                std::cerr
                    << "Failed to update skinning palettes: "
                    << sceneInstance.sourcePath
                    << '\n';

                return false;
            }
        }

        const CpuClock::time_point morphStart =
            CpuClock::now();

        for (stylized::render::RuntimeMeshInstance& instance :
             sceneInstance.morphMeshInstances)
        {
            if (instance.primitiveCount() == 0)
            {
                continue;
            }

            if (!instance.update())
            {
                std::cerr
                    << "Failed to update Morph mesh instance: "
                    << sceneInstance.sourcePath
                    << '\n';

                return false;
            }
        }

        morphMilliseconds +=
            elapsedMilliseconds(morphStart);

        return true;
    }

    bool applyMorphAnimation(
        const stylized::asset::SceneAsset& sceneAsset,
        stylized::viewer::SceneRuntimeInstance&
            sceneInstance)
    {
        const std::uint64_t morphPoseVersion =
            sceneInstance.morphPose.version();

        if (sceneInstance.appliedMorphPoseVersion == morphPoseVersion)
        {
            return true;
        }

        if (sceneInstance.morphMeshInstances.size() !=
            sceneAsset.nodes.size())
        {
            return false;
        }

        for (std::size_t nodeIndex = 0;
             nodeIndex < sceneInstance.morphMeshInstances.size();
             ++nodeIndex)
        {
            stylized::render::RuntimeMeshInstance& instance =
                sceneInstance.morphMeshInstances[nodeIndex];

            const std::span<const float> animatedWeights =
                sceneInstance.morphPose.weights(
                    static_cast<std::uint32_t>(nodeIndex));

            for (std::size_t primitiveIndex = 0;
                 primitiveIndex < instance.primitiveCount();
                 ++primitiveIndex)
            {
                stylized::animation::MorphState* state =
                    instance.morphState(primitiveIndex);

                if (state == nullptr)
                {
                    continue;
                }

                if (animatedWeights.empty())
                {
                    state->reset();
                    continue;
                }

                for (std::size_t targetIndex = 0;
                     targetIndex < state->targetCount();
                     ++targetIndex)
                {
                    const float weight =
                        targetIndex < animatedWeights.size()
                            ? animatedWeights[targetIndex]
                            : 0.0F;

                    if (!state->setWeight(
                            targetIndex,
                            weight))
                    {
                        return false;
                    }
                }
            }
        }

        sceneInstance.appliedMorphPoseVersion = morphPoseVersion;

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

        stylized::material::MaterialTemplate mtoonTemplate;

        mtoonTemplate.name = "MToon";

        mtoonTemplate.kind =
            stylized::material::MaterialKind::MToon;

        mtoonTemplate.vertexShaderPath =
            "assets/shaders/static_model/static_model.vert";

        mtoonTemplate.fragmentShaderPath =
            "assets/shaders/material/mtoon.frag";

        if (!mtoonTemplate.isValid())
        {
            return false;
        }

        mtoonTemplateHandle_ = assetRegistry_.emplace<
            stylized::material::MaterialTemplate>(
                std::move(mtoonTemplate));

        if (mtoonTemplateHandle_.isNull())
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

        case stylized::material::MaterialKind::MToon:
            activeMaterialTemplateHandle_ =
                mtoonTemplateHandle_;
            break;
        }

        return !activeMaterialTemplateHandle_.isNull();
    }

    [[nodiscard]]
    stylized::viewer::SceneRuntimeInstance*
    primarySceneInstance() noexcept
    {
        if (sceneInstances_.empty())
        {
            return nullptr;
        }

        return sceneInstances_.front().get();
    }

    [[nodiscard]]
    stylized::viewer::SceneRuntimeInstance*
    selectedSceneInstance() noexcept
    {
        if (sceneInstances_.empty())
        {
            return nullptr;
        }

        selectedSceneInstanceIndex_ =
            std::min(
                selectedSceneInstanceIndex_,
                sceneInstances_.size() - 1);

        return sceneInstances_[
            selectedSceneInstanceIndex_].get();
    }

    [[nodiscard]]
    const stylized::scene::Camera& activeCamera() const noexcept
    {
        return useSceneCamera_
            ? sceneCamera_
            : controlCamera_;
    }

    bool smokeTest_ = false;
    int renderedFrameCount_ = 0;

    std::vector<std::filesystem::path> modelPaths_;
    std::filesystem::path cameraJsonPath_;
    std::optional<CameraJsonTrack> cameraTrack_;

    std::size_t selectedSceneInstanceIndex_ = 0;

    stylized::asset::AssetRegistry assetRegistry_;

    std::vector<
        std::unique_ptr<
            stylized::viewer::SceneRuntimeInstance>>
        sceneInstances_;

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
        mtoonTemplateHandle_;

    stylized::asset::AssetHandle<
        stylized::material::MaterialTemplate>
        activeMaterialTemplateHandle_;

    stylized::material::MaterialKind activeMaterialKind_ =
        stylized::material::MaterialKind::Unlit;

    bool shadowsEnabled_ = true;
    float exposure_ = 1.0F;
    bool toneMappingEnabled_ = true;
    bool fxaaEnabled_ = true;

    stylized::render::ShadowPass* shadowPass_ = nullptr;
    stylized::render::ForwardOpaquePass* forwardOpaquePass_ = nullptr;
    stylized::render::ForwardTransparentPass*
        forwardTransparentPass_ = nullptr;
    stylized::render::OutlineMaskPass* outlineMaskPass_ = nullptr;
    stylized::render::ScreenSpaceOutlinePass* screenSpaceOutlinePass_ = nullptr;
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

    ViewerCpuTimings cpuTimings_;

    stylized::scene::Camera controlCamera_;
    OrbitCameraController cameraController_{
        controlCamera_
    };

    bool cameraFocused_ = false;

    stylized::scene::Camera sceneCamera_;
    bool useSceneCamera_ = false;

    bool spaceKeyPressed_ = false;

    stylized::graphics::ShaderProgram
        morphComputeProgram_;
};

} // namespace

int main(
    const int argc,
    char* argv[])
{
    bool smokeTest = false;

    std::vector<std::filesystem::path>
        modelPaths;

    std::filesystem::path cameraJsonPath;

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

        if (argument == "--camera-glb")
        {
            std::cerr
                << "--camera-glb is no longer supported; "
                << "use --camera-json.\n";
            return 1;
        }

        if (argument == "--camera-json")
        {
            if (argumentIndex + 1 >= argc)
            {
                std::cerr
                    << argument
                    << " requires a file path.\n";

                return 1;
            }

            const std::filesystem::path value =
                std::filesystem::path{
                    argv[++argumentIndex]
                };

            cameraJsonPath = value;

            continue;
        }

        modelPaths.push_back(
            std::filesystem::path{
                argument
            });
    }

    ViewerApplication application{
        smokeTest,
        std::move(modelPaths),
        std::move(cameraJsonPath)
    };

    return application.run();
}
