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
#include <vector>

namespace stylized::asset
{

struct SceneAsset;

} // namespace stylized::asset

namespace stylized::viewer
{

struct SceneRuntimeInstance final
    : core::NonCopyable
{
    std::filesystem::path sourcePath;

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