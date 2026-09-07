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
#include <limits>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace stylized::asset
{

struct MaterialAsset;
struct SceneAsset;

} // namespace stylized::asset

namespace stylized::viewer
{

struct FaceSdfRuntimeConfig
{
    static constexpr std::uint32_t invalidNodeIndex =
        std::numeric_limits<std::uint32_t>::max();

    asset::AssetHandle<asset::MaterialAsset> material;
    std::uint32_t headNodeIndex = invalidNodeIndex;
    glm::vec3 headRight{1.0F, 0.0F, 0.0F};
    glm::vec3 headForward{0.0F, 0.0F, 1.0F};

    bool hairShadowEnabled = false;
    asset::AssetHandle<asset::MaterialAsset> hairShadowCaster;
    std::uint32_t hairShadowResolution = 512;
    glm::vec3 hairShadowLocalCenter{0.0F};
    glm::vec2 hairShadowUvOffset{0.0F};
    float hairShadowWidth = 0.11F;
    float hairShadowHeight = 0.14F;
    float hairShadowDepth = 0.25F;
    float hairShadowCameraDistance = 0.12F;
    float hairShadowAlphaCutoff = 0.72F;
    float hairShadowSoftness = 0.004F;
    float hairShadowStrength = 0.8F;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !material.isNull() &&
            headNodeIndex != invalidNodeIndex;
    }
};

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

    FaceSdfRuntimeConfig faceSdf;

    std::vector<render::RuntimeMeshInstance>
        morphMeshInstances;

    std::uint64_t appliedMorphPoseVersion = 0;
};

} // namespace stylized::viewer
