#include <animation/SceneMorphPose.hpp>

#include <asset/SceneAsset.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace stylized::animation
{

bool SceneMorphPose::initialize(
    const asset::SceneAsset& sceneAsset)
{
    if (!sceneAsset.isValid())
    {
        clear();
        return false;
    }

    sceneAsset_ = &sceneAsset;
    nodeWeights_.clear();
    nodeWeights_.resize(sceneAsset.nodes.size());
    version_ = 0;

    return true;
}

void SceneMorphPose::clear() noexcept
{
    sceneAsset_ = nullptr;
    nodeWeights_.clear();
    version_ = 0;
}

void SceneMorphPose::reset() noexcept
{
    bool changed = false;

    for (std::vector<float>& weights : nodeWeights_)
    {
        if (!weights.empty())
        {
            weights.clear();
            changed = true;
        }
    }

    if (changed)
    {
        advanceVersion();
    }
}

bool SceneMorphPose::setWeights(
    const std::uint32_t nodeIndex,
    const std::span<const float> weights)
{
    if (sceneAsset_ == nullptr ||
        nodeIndex >= nodeWeights_.size() ||
        weights.empty())
    {
        return false;
    }

    for (const float weight : weights)
    {
        if (!std::isfinite(weight))
        {
            return false;
        }
    }

    std::vector<float>& destination =
        nodeWeights_[nodeIndex];

    if (std::equal(
            destination.begin(),
            destination.end(),
            weights.begin(),
            weights.end()))
    {
        return true;
    }

    destination.assign(
        weights.begin(),
        weights.end());

    advanceVersion();

    return true;
}

std::span<const float> SceneMorphPose::weights(
    const std::uint32_t nodeIndex) const noexcept
{
    if (nodeIndex >= nodeWeights_.size())
    {
        return {};
    }

    return nodeWeights_[nodeIndex];
}

bool SceneMorphPose::isForScene(
    const asset::SceneAsset& sceneAsset) const noexcept
{
    return sceneAsset_ == &sceneAsset;
}

std::uint64_t SceneMorphPose::version() const noexcept
{
    return version_;
}

void SceneMorphPose::advanceVersion() noexcept
{
    ++version_;

    if (version_ == 0)
    {
        ++version_;
    }
}

} // namespace stylized::animation
