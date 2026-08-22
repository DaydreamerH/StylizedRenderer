#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <assimp/anim.h>
#include <assimp/scene.h>

namespace stylized::asset::importers::detail
{

namespace
{

constexpr double fallbackTicksPerSecond = 25.0;
constexpr float minimumQuaternionLengthSquared = 1.0e-8F;

[[nodiscard]] bool convertTime(
    const double timeTicks,
    const double ticksPerSecond,
    float& timeSeconds) noexcept
{
    const double seconds =
        timeTicks / ticksPerSecond;

    if (!std::isfinite(seconds) ||
        seconds < 0.0 ||
        seconds >
            static_cast<double>(
                std::numeric_limits<float>::max()))
    {
        return false;
    }

    timeSeconds =
        static_cast<float>(seconds);

    return true;
}

[[nodiscard]] bool convertVectorKeys(
    const aiVectorKey* sourceKeys,
    const unsigned int keyCount,
    const double ticksPerSecond,
    std::vector<VectorAnimationKey>& destination)
{
    if (keyCount > 0 &&
        sourceKeys == nullptr)
    {
        return false;
    }

    destination.clear();
    destination.reserve(keyCount);

    for (unsigned int keyIndex = 0;
         keyIndex < keyCount;
         ++keyIndex)
    {
        const aiVectorKey& sourceKey =
            sourceKeys[keyIndex];

        VectorAnimationKey key;

        if (!convertTime(
                sourceKey.mTime,
                ticksPerSecond,
                key.timeSeconds))
        {
            return false;
        }

        key.value = {
            sourceKey.mValue.x,
            sourceKey.mValue.y,
            sourceKey.mValue.z
        };

        destination.push_back(key);
    }

    return true;
}

[[nodiscard]] bool convertQuaternionKeys(
    const aiQuatKey* sourceKeys,
    const unsigned int keyCount,
    const double ticksPerSecond,
    std::vector<QuaternionAnimationKey>& destination)
{
    if (keyCount > 0 &&
        sourceKeys == nullptr)
    {
        return false;
    }

    destination.clear();
    destination.reserve(keyCount);

    for (unsigned int keyIndex = 0;
         keyIndex < keyCount;
         ++keyIndex)
    {
        const aiQuatKey& sourceKey =
            sourceKeys[keyIndex];

        QuaternionAnimationKey key;

        if (!convertTime(
                sourceKey.mTime,
                ticksPerSecond,
                key.timeSeconds))
        {
            return false;
        }

        const float w = sourceKey.mValue.w;
        const float x = sourceKey.mValue.x;
        const float y = sourceKey.mValue.y;
        const float z = sourceKey.mValue.z;

        if (!std::isfinite(w) ||
            !std::isfinite(x) ||
            !std::isfinite(y) ||
            !std::isfinite(z))
        {
            return false;
        }

        const float lengthSquared =
            w * w +
            x * x +
            y * y +
            z * z;

        if (lengthSquared <=
            minimumQuaternionLengthSquared)
        {
            return false;
        }

        const float inverseLength =
            1.0F / std::sqrt(lengthSquared);

        key.value = {
            w * inverseLength,
            x * inverseLength,
            y * inverseLength,
            z * inverseLength
        };

        destination.push_back(key);
    }

    return true;
}

[[nodiscard]] bool stageChannel(
    const aiNodeAnim& sourceChannel,
    const double ticksPerSecond,
    const StagedScene& scene,
    NodeAnimationChannelAsset& channel)
{
    const std::string nodeName =
        sourceChannel.mNodeName.C_Str();

    const SceneNodeLookupResult lookupResult =
        findSceneNodeIndex(
            scene,
            nodeName,
            channel.nodeIndex);

    if (lookupResult !=
        SceneNodeLookupResult::Found)
    {
        std::cerr
            << "Cannot uniquely match animation node: "
            << nodeName
            << '\n';

        return false;
    }

    return
        convertVectorKeys(
            sourceChannel.mPositionKeys,
            sourceChannel.mNumPositionKeys,
            ticksPerSecond,
            channel.translations) &&
        convertQuaternionKeys(
            sourceChannel.mRotationKeys,
            sourceChannel.mNumRotationKeys,
            ticksPerSecond,
            channel.rotations) &&
        convertVectorKeys(
            sourceChannel.mScalingKeys,
            sourceChannel.mNumScalingKeys,
            ticksPerSecond,
            channel.scales);
}

[[nodiscard]] SceneNodeLookupResult findMorphNodeIndex(
    const aiScene& importedScene,
    const StagedScene& scene,
    const std::string& channelName,
    std::uint32_t& nodeIndex) noexcept
{
    const SceneNodeLookupResult nodeLookup =
        findSceneNodeIndex(
            scene,
            channelName,
            nodeIndex);

    if (nodeLookup != SceneNodeLookupResult::Missing)
    {
        return nodeLookup;
    }

    std::optional<std::uint32_t> matchedNode;

    for (std::size_t candidateIndex = 0;
         candidateIndex < scene.sourceNodes.size();
         ++candidateIndex)
    {
        const aiNode* sourceNode =
            scene.sourceNodes[candidateIndex];

        if (sourceNode == nullptr ||
            (sourceNode->mNumMeshes > 0 &&
             sourceNode->mMeshes == nullptr))
        {
            return SceneNodeLookupResult::Missing;
        }

        bool matches = false;

        for (unsigned int meshOffset = 0;
             meshOffset < sourceNode->mNumMeshes;
             ++meshOffset)
        {
            const unsigned int meshIndex =
                sourceNode->mMeshes[meshOffset];

            if (meshIndex >= importedScene.mNumMeshes ||
                importedScene.mMeshes == nullptr ||
                importedScene.mMeshes[meshIndex] == nullptr)
            {
                return SceneNodeLookupResult::Missing;
            }

            if (channelName ==
                importedScene.mMeshes[meshIndex]
                    ->mName.C_Str())
            {
                matches = true;
                break;
            }
        }

        if (!matches)
        {
            continue;
        }

        if (matchedNode.has_value())
        {
            nodeIndex =
                SceneNodeAsset::invalidNodeIndex;

            return SceneNodeLookupResult::Ambiguous;
        }

        matchedNode = static_cast<std::uint32_t>(
            candidateIndex);
    }

    if (!matchedNode.has_value())
    {
        nodeIndex = SceneNodeAsset::invalidNodeIndex;
        return SceneNodeLookupResult::Missing;
    }

    nodeIndex = *matchedNode;
    return SceneNodeLookupResult::Found;
}

[[nodiscard]] bool findMorphTargetCount(
    const aiScene& importedScene,
    const StagedScene& scene,
    const std::uint32_t nodeIndex,
    std::size_t& targetCount) noexcept
{
    targetCount = 0;

    if (nodeIndex >= scene.sourceNodes.size() ||
        importedScene.mMeshes == nullptr)
    {
        return false;
    }

    const aiNode* sourceNode = scene.sourceNodes[nodeIndex];

    if (sourceNode == nullptr ||
        sourceNode->mNumMeshes == 0 ||
        sourceNode->mMeshes == nullptr)
    {
        return false;
    }

    for (unsigned int meshOffset = 0;
         meshOffset < sourceNode->mNumMeshes;
         ++meshOffset)
    {
        const unsigned int meshIndex =
            sourceNode->mMeshes[meshOffset];

        if (meshIndex >= importedScene.mNumMeshes ||
            importedScene.mMeshes[meshIndex] == nullptr)
        {
            return false;
        }

        const std::size_t meshTargetCount =
            importedScene.mMeshes[meshIndex]
                ->mNumAnimMeshes;

        if (meshTargetCount == 0)
        {
            continue;
        }

        if (targetCount == 0)
        {
            targetCount = meshTargetCount;
        }
        else if (targetCount != meshTargetCount)
        {
            return false;
        }
    }

    return targetCount > 0;
}

[[nodiscard]] bool stageMorphChannel(
    const aiScene& importedScene,
    const aiMeshMorphAnim& sourceChannel,
    const double ticksPerSecond,
    const StagedScene& scene,
    NodeMorphAnimationChannelAsset& channel)
{
    const std::string channelName =
        sourceChannel.mName.C_Str();

    if (findMorphNodeIndex(
            importedScene,
            scene,
            channelName,
            channel.nodeIndex) !=
        SceneNodeLookupResult::Found)
    {
        std::cerr
            << "Cannot uniquely match Morph animation node: "
            << channelName
            << '\n';

        return false;
    }

    if (!findMorphTargetCount(
            importedScene,
            scene,
            channel.nodeIndex,
            channel.targetCount) ||
        sourceChannel.mNumKeys == 0 ||
        sourceChannel.mKeys == nullptr)
    {
        return false;
    }

    channel.keys.clear();
    channel.keys.reserve(sourceChannel.mNumKeys);

    for (unsigned int keyIndex = 0;
         keyIndex < sourceChannel.mNumKeys;
         ++keyIndex)
    {
        const aiMeshMorphKey& sourceKey =
            sourceChannel.mKeys[keyIndex];

        if (sourceKey.mNumValuesAndWeights > 0 &&
            (sourceKey.mValues == nullptr ||
             sourceKey.mWeights == nullptr))
        {
            return false;
        }

        MorphWeightKey key;

        if (!convertTime(
                sourceKey.mTime,
                ticksPerSecond,
                key.timeSeconds))
        {
            return false;
        }

        key.weights.assign(
            channel.targetCount,
            0.0F);

        std::vector<bool> assigned(
            channel.targetCount,
            false);

        for (unsigned int valueIndex = 0;
             valueIndex <
                 sourceKey.mNumValuesAndWeights;
             ++valueIndex)
        {
            const unsigned int targetIndex =
                sourceKey.mValues[valueIndex];
            const double sourceWeight =
                sourceKey.mWeights[valueIndex];

            if (targetIndex >= channel.targetCount ||
                assigned[targetIndex] ||
                !std::isfinite(sourceWeight) ||
                sourceWeight >
                    static_cast<double>(
                        std::numeric_limits<float>::max()) ||
                sourceWeight <
                    -static_cast<double>(
                        std::numeric_limits<float>::max()))
            {
                return false;
            }

            key.weights[targetIndex] =
                static_cast<float>(sourceWeight);
            assigned[targetIndex] = true;
        }

        channel.keys.push_back(std::move(key));
    }

    return true;
}

} // namespace

bool stageAnimations(
    const aiScene& importedScene,
    StagedScene& scene)
{
    scene.asset.animations.clear();
    scene.asset.animations.reserve(
        importedScene.mNumAnimations);

    if (importedScene.mNumAnimations > 0 &&
        importedScene.mAnimations == nullptr)
    {
        return false;
    }

    for (unsigned int animationIndex = 0;
         animationIndex < importedScene.mNumAnimations;
         ++animationIndex)
    {
        const aiAnimation* sourceAnimation =
            importedScene.mAnimations[animationIndex];

        if (sourceAnimation == nullptr)
        {
            return false;
        }

        if (sourceAnimation->mNumChannels == 0 &&
            sourceAnimation->mNumMorphMeshChannels == 0)
        {
            continue;
        }

        if ((sourceAnimation->mNumChannels > 0 &&
             sourceAnimation->mChannels == nullptr) ||
            (sourceAnimation->mNumMorphMeshChannels > 0 &&
             sourceAnimation->mMorphMeshChannels == nullptr))
        {
            return false;
        }

        AnimationClipAsset clip;

        clip.name =
            sourceAnimation->mName.length > 0
                ? sourceAnimation->mName.C_Str()
                : "Animation_" +
                    std::to_string(animationIndex);

        double ticksPerSecond =
            sourceAnimation->mTicksPerSecond;

        if (!std::isfinite(ticksPerSecond) ||
            ticksPerSecond < 0.0)
        {
            return false;
        }

        if (ticksPerSecond == 0.0)
        {
            ticksPerSecond =
                fallbackTicksPerSecond;

            std::cerr
                << "Animation \""
                << clip.name
                << "\" has no ticks-per-second value; "
                << "using "
                << fallbackTicksPerSecond
                << ".\n";
        }

        if (!convertTime(
                sourceAnimation->mDuration,
                ticksPerSecond,
                clip.durationSeconds) ||
            clip.durationSeconds <= 0.0F)
        {
            return false;
        }

        clip.channels.reserve(
            sourceAnimation->mNumChannels);
        clip.morphChannels.reserve(
            sourceAnimation->mNumMorphMeshChannels);

        for (unsigned int channelIndex = 0;
             channelIndex <
                 sourceAnimation->mNumChannels;
             ++channelIndex)
        {
            const aiNodeAnim* sourceChannel =
                sourceAnimation->mChannels[channelIndex];

            if (sourceChannel == nullptr)
            {
                return false;
            }

            if (sourceChannel->mNumPositionKeys == 0 &&
                sourceChannel->mNumRotationKeys == 0 &&
                sourceChannel->mNumScalingKeys == 0)
            {
                continue;
            }

            NodeAnimationChannelAsset channel;

            if (!stageChannel(
                    *sourceChannel,
                    ticksPerSecond,
                    scene,
                    channel))
            {
                return false;
            }

            clip.channels.push_back(
                std::move(channel));
        }

        for (unsigned int channelIndex = 0;
             channelIndex <
                 sourceAnimation->mNumMorphMeshChannels;
             ++channelIndex)
        {
            const aiMeshMorphAnim* sourceChannel =
                sourceAnimation
                    ->mMorphMeshChannels[channelIndex];

            if (sourceChannel == nullptr)
            {
                return false;
            }

            if (sourceChannel->mNumKeys == 0)
            {
                continue;
            }

            NodeMorphAnimationChannelAsset channel;

            if (!stageMorphChannel(
                    importedScene,
                    *sourceChannel,
                    ticksPerSecond,
                    scene,
                    channel))
            {
                return false;
            }

            clip.morphChannels.push_back(
                std::move(channel));
        }

        if (clip.channels.empty() &&
            clip.morphChannels.empty())
        {
            continue;
        }

        if (!clip.isValid(
                scene.asset.nodes.size()))
        {
            return false;
        }

        scene.asset.animations.push_back(
            std::move(clip));
    }

    return true;
}

} // namespace stylized::asset::importers::detail
