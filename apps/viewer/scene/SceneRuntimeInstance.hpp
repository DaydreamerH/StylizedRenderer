#pragma once

#include <animation/AnimationPlayer.hpp>
#include <animation/SceneMorphPose.hpp>
#include <animation/ScenePose.hpp>

#include <asset/AssetHandle.hpp>

#include <core/NonCopyable.hpp>

#include <render/resources/RuntimeMeshInstance.hpp>
#include <render/resources/SkinningPaletteSet.hpp>

#include <scene/Transform.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace stylized::asset
{

struct SceneAsset;

} // namespace stylized::asset

namespace stylized::viewer
{

struct CameraFovSample
{
    float timeSeconds = 0.0F;
    float verticalFovDegrees = 30.04F;
};

struct SceneRuntimeInstance final
    : core::NonCopyable
{
    std::filesystem::path sourcePath;

    std::string cameraFovCameraName;
    std::vector<CameraFovSample> cameraFovSamples;

    scene::Transform rootTransform;

    asset::AssetHandle<asset::SceneAsset>
        sceneHandle;

    animation::AnimationPlayer
        animationPlayer;

    animation::ScenePose
        scenePose;

    animation::SceneMorphPose
        morphPose;

    render::SkinningPaletteSet
        skinningPalettes;

    std::vector<render::RuntimeMeshInstance>
        morphMeshInstances;

    std::uint64_t appliedMorphPoseVersion = 0;
};

} // namespace stylized::viewer
