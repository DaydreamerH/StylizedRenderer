#include <animation/ScenePose.hpp>

#include <asset/SceneAsset.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace stylized::animation
{

bool ScenePose::initialize(
    const asset::SceneAsset& sceneAsset)
{
    if (!sceneAsset.isValid())
    {
        clear();
        return false;
    }

    sceneAsset_ = &sceneAsset;

    localTransforms_.clear();
    localTransforms_.reserve(
        sceneAsset.nodes.size());

    for (const asset::SceneNodeAsset& node :
         sceneAsset.nodes)
    {
        localTransforms_.push_back(node.localTransform);
    }

    worldMatrices_.assign(
        sceneAsset.nodes.size(),
        glm::mat4{1.0F});

    resolutionStates_.assign(
        sceneAsset.nodes.size(),
        std::uint8_t{0});

    worldMatricesDirty_ = true;

    if (!updateWorldMatrices())
    {
        clear();
        return false;
    }

    return true;
}

bool ScenePose::isForScene(
    const asset::SceneAsset& sceneAsset) const noexcept
{
    return sceneAsset_ == &sceneAsset;
}

void ScenePose::clear() noexcept
{
    sceneAsset_ = nullptr;

    localTransforms_.clear();
    worldMatrices_.clear();
    resolutionStates_.clear();

    worldMatricesDirty_ = true;
    version_ = 0;
}

bool ScenePose::resetToBindPose() noexcept
{
    if (sceneAsset_ == nullptr ||
        sceneAsset_->nodes.size() !=
            localTransforms_.size())
    {
        return false;
    }

    for (std::size_t nodeIndex = 0;
         nodeIndex < localTransforms_.size();
         ++nodeIndex)
    {
        localTransforms_[nodeIndex] =
            sceneAsset_->nodes[nodeIndex].localTransform;
    }

    worldMatricesDirty_ = true;

    return updateWorldMatrices();
}

bool ScenePose::setLocalTransform(
    const std::uint32_t nodeIndex,
    const scene::Transform& transform) noexcept
{
    if (nodeIndex >= localTransforms_.size())
    {
        return false;
    }

    localTransforms_[nodeIndex] = transform;
    worldMatricesDirty_ = true;

    return true;
}

const scene::Transform*
ScenePose::localTransform(
    const std::uint32_t nodeIndex) const noexcept
{
    if (nodeIndex >= localTransforms_.size())
    {
        return nullptr;
    }

    return &localTransforms_[nodeIndex];
}

bool ScenePose::updateWorldMatrices() noexcept
{
    if (sceneAsset_ == nullptr ||
        sceneAsset_->nodes.size() !=
            localTransforms_.size() ||
        worldMatrices_.size() !=
            localTransforms_.size() ||
        resolutionStates_.size() !=
            localTransforms_.size())
    {
        return false;
    }

    std::fill(
        resolutionStates_.begin(),
        resolutionStates_.end(),
        std::uint8_t{0});

    for (std::size_t nodeIndex = 0;
         nodeIndex < localTransforms_.size();
         ++nodeIndex)
    {
        if (!resolveWorldMatrix(nodeIndex))
        {
            worldMatricesDirty_ = true;
            return false;
        }
    }

    worldMatricesDirty_ = false;
    advanceVersion();

    return true;
}

const glm::mat4*
ScenePose::worldMatrix(
    const std::uint32_t nodeIndex) const noexcept
{
    if (worldMatricesDirty_ ||
        nodeIndex >= worldMatrices_.size())
    {
        return nullptr;
    }

    return &worldMatrices_[nodeIndex];
}

bool ScenePose::isInitialized() const noexcept
{
    return sceneAsset_ != nullptr;
}

bool ScenePose::worldMatricesDirty() const noexcept
{
    return worldMatricesDirty_;
}

std::uint64_t ScenePose::version() const noexcept
{
    return version_;
}

std::size_t ScenePose::nodeCount() const noexcept
{
    return localTransforms_.size();
}

void ScenePose::advanceVersion() noexcept
{
    ++version_;

    if (version_ == 0)
    {
        ++version_;
    }
}

bool ScenePose::resolveWorldMatrix(
    const std::size_t nodeIndex) noexcept
{
    if (sceneAsset_ == nullptr ||
        nodeIndex >= localTransforms_.size())
    {
        return false;
    }

    constexpr std::uint8_t resolving = 1;
    constexpr std::uint8_t resolved = 2;

    if (resolutionStates_[nodeIndex] == resolved)
    {
        return true;
    }

    if (resolutionStates_[nodeIndex] == resolving)
    {
        return false;
    }

    resolutionStates_[nodeIndex] = resolving;

    const asset::SceneNodeAsset& node =
        sceneAsset_->nodes[nodeIndex];

    const glm::mat4 localMatrix =
        localTransforms_[nodeIndex].localMatrix();

    if (!node.hasParent())
    {
        worldMatrices_[nodeIndex] =
            localMatrix;
    }
    else
    {
        const std::size_t parentIndex =
            static_cast<std::size_t>(
                node.parentIndex);

        if (parentIndex >=
                localTransforms_.size() ||
            !resolveWorldMatrix(parentIndex))
        {
            return false;
        }

        worldMatrices_[nodeIndex] =
            worldMatrices_[parentIndex] *
            localMatrix;
    }

    resolutionStates_[nodeIndex] = resolved;

    return true;
}

} // namespace stylized::animation
