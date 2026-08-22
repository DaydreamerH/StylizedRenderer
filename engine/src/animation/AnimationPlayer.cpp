#include <animation/AnimationPlayer.hpp>

#include <animation/ScenePose.hpp>
#include <animation/SceneMorphPose.hpp>
#include <asset/AnimationAsset.hpp>
#include <asset/SceneAsset.hpp>
#include <scene/Transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace stylized::animation
{

namespace
{

constexpr float minimumKeyInterval = 1.0e-6F;

template<typename Key>
[[nodiscard]] std::size_t findRightKey(
    const std::vector<Key>& keys,
    const float timeSeconds) noexcept
{
    const auto iterator = std::lower_bound(
        keys.begin(),
        keys.end(),
        timeSeconds,
        [](const Key& key, const float time)
        {
            return key.timeSeconds < time;
        });

    return static_cast<std::size_t>(
        iterator - keys.begin());
}

[[nodiscard]] glm::vec3 sampleVectorTrack(
    const std::vector<asset::VectorAnimationKey>& keys,
    const float timeSeconds,
    const glm::vec3& fallback) noexcept
{
    if (keys.empty())
    {
        return fallback;
    }

    if (keys.size() == 1 ||
        timeSeconds <= keys.front().timeSeconds)
    {
        return keys.front().value;
    }

    if (timeSeconds >= keys.back().timeSeconds)
    {
        return keys.back().value;
    }

    const std::size_t rightIndex =
        findRightKey(keys, timeSeconds);

    if (rightIndex == 0 ||
        rightIndex >= keys.size())
    {
        return keys.back().value;
    }

    const asset::VectorAnimationKey& left =
        keys[rightIndex - 1];

    const asset::VectorAnimationKey& right =
        keys[rightIndex];

    const float interval =
        right.timeSeconds -
        left.timeSeconds;

    if (interval <= minimumKeyInterval)
    {
        return right.value;
    }

    const float factor = std::clamp(
        (timeSeconds - left.timeSeconds) /
            interval,
        0.0F,
        1.0F);

    return glm::mix(
        left.value,
        right.value,
        factor);
}

[[nodiscard]] glm::quat sampleRotationTrack(
    const std::vector<
        asset::QuaternionAnimationKey>& keys,
    const float timeSeconds,
    const glm::quat& fallback) noexcept
{
    if (keys.empty())
    {
        return fallback;
    }

    if (keys.size() == 1 ||
        timeSeconds <= keys.front().timeSeconds)
    {
        return keys.front().value;
    }

    if (timeSeconds >= keys.back().timeSeconds)
    {
        return keys.back().value;
    }

    const std::size_t rightIndex =
        findRightKey(keys, timeSeconds);

    if (rightIndex == 0 ||
        rightIndex >= keys.size())
    {
        return keys.back().value;
    }

    const asset::QuaternionAnimationKey& left =
        keys[rightIndex - 1];

    const asset::QuaternionAnimationKey& right =
        keys[rightIndex];

    const float interval =
        right.timeSeconds -
        left.timeSeconds;

    if (interval <= minimumKeyInterval)
    {
        return right.value;
    }

    const float factor = std::clamp(
        (timeSeconds - left.timeSeconds) /
            interval,
        0.0F,
        1.0F);

    glm::quat rightRotation =
        right.value;

    if (glm::dot(
            left.value,
            rightRotation) < 0.0F)
    {
        rightRotation =
            -rightRotation;
    }

    return glm::normalize(
        glm::slerp(
            left.value,
            rightRotation,
            factor));
}

[[nodiscard]] bool sampleMorphTrack(
    const asset::NodeMorphAnimationChannelAsset& channel,
    const float timeSeconds,
    std::vector<float>& sampledWeights)
{
    if (channel.keys.empty() ||
        channel.targetCount == 0)
    {
        return false;
    }

    const auto copyKey =
        [&sampledWeights](
            const asset::MorphWeightKey& key)
        {
            sampledWeights = key.weights;
        };

    if (channel.keys.size() == 1 ||
        timeSeconds <=
            channel.keys.front().timeSeconds)
    {
        copyKey(channel.keys.front());
        return true;
    }

    if (timeSeconds >=
        channel.keys.back().timeSeconds)
    {
        copyKey(channel.keys.back());
        return true;
    }

    const std::size_t rightIndex =
        findRightKey(channel.keys, timeSeconds);

    if (rightIndex == 0 ||
        rightIndex >= channel.keys.size())
    {
        copyKey(channel.keys.back());
        return true;
    }

    const asset::MorphWeightKey& left =
        channel.keys[rightIndex - 1];
    const asset::MorphWeightKey& right =
        channel.keys[rightIndex];

    if (left.weights.size() != channel.targetCount ||
        right.weights.size() != channel.targetCount)
    {
        return false;
    }

    const float interval =
        right.timeSeconds - left.timeSeconds;

    if (interval <= minimumKeyInterval)
    {
        copyKey(right);
        return true;
    }

    const float factor = std::clamp(
        (timeSeconds - left.timeSeconds) / interval,
        0.0F,
        1.0F);

    sampledWeights.resize(channel.targetCount);

    for (std::size_t targetIndex = 0;
         targetIndex < channel.targetCount;
         ++targetIndex)
    {
        sampledWeights[targetIndex] =
            std::lerp(
                left.weights[targetIndex],
                right.weights[targetIndex],
                factor);
    }

    return true;
}

} // namespace

bool AnimationPlayer::setClip(
    const asset::AnimationClipAsset* clip) noexcept
{
    if (clip == nullptr ||
        !std::isfinite(clip->durationSeconds) ||
        clip->durationSeconds <= 0.0F ||
        (clip->channels.empty() &&
         clip->morphChannels.empty()))
    {
        return false;
    }

    clip_ = clip;
    validatedScene_ = nullptr;

    currentTime_ = 0.0F;
    playing_ = false;

    samplePending_ = true;
    resetPoseOnNextSample_ = true;

    return true;
}

void AnimationPlayer::play() noexcept
{
    if (clip_ != nullptr)
    {
        playing_ = true;
    }
}

void AnimationPlayer::pause() noexcept
{
    playing_ = false;
}

void AnimationPlayer::stop() noexcept
{
    playing_ = false;
    currentTime_ = 0.0F;

    samplePending_ =
        clip_ != nullptr;

    resetPoseOnNextSample_ =
        clip_ != nullptr;
}

void AnimationPlayer::setLooping(
    const bool looping) noexcept
{
    looping_ = looping;
}

void AnimationPlayer::setPlaybackSpeed(
    const float speed) noexcept
{
    if (std::isfinite(speed) &&
        speed >= 0.0F)
    {
        playbackSpeed_ = speed;
    }
}

void AnimationPlayer::seek(
    const float timeSeconds) noexcept
{
    if (clip_ == nullptr ||
        !std::isfinite(timeSeconds))
    {
        return;
    }

    currentTime_ = std::clamp(
        timeSeconds,
        0.0F,
        clip_->durationSeconds);

    samplePending_ = true;
}

bool AnimationPlayer::update(
    const float deltaTime,
    const asset::SceneAsset& sceneAsset,
    ScenePose& pose,
    SceneMorphPose& morphPose) noexcept
{
    if (clip_ == nullptr ||
        !std::isfinite(deltaTime) ||
        deltaTime < 0.0F ||
        !pose.isForScene(sceneAsset) ||
        !morphPose.isForScene(sceneAsset))
    {
        return false;
    }

    if (validatedScene_ != &sceneAsset)
    {
        if (!sceneAsset.isValid() ||
            !clip_->isValid(sceneAsset.nodes.size()))
        {
            return false;
        }

        validatedScene_ = &sceneAsset;
    }

    if (playing_ &&
        playbackSpeed_ > 0.0F &&
        deltaTime > 0.0F)
    {
        const float timeDelta =
            deltaTime * playbackSpeed_;

        if (!std::isfinite(timeDelta))
        {
            return false;
        }

        currentTime_ += timeDelta;

        if (looping_)
        {
            currentTime_ = std::fmod(
                currentTime_,
                clip_->durationSeconds);
        }
        else if (currentTime_ >=
            clip_->durationSeconds)
        {
            currentTime_ =
                clip_->durationSeconds;

            playing_ = false;
        }

        samplePending_ = true;
    }

    if (!samplePending_)
    {
        return true;
    }

    if (resetPoseOnNextSample_)
    {
        if (!pose.resetToBindPose())
        {
            return false;
        }

        morphPose.reset();

        resetPoseOnNextSample_ = false;
    }

    if (!sample(pose, morphPose))
    {
        return false;
    }

    samplePending_ = false;

    return true;
}

const asset::AnimationClipAsset*
AnimationPlayer::clip() const noexcept
{
    return clip_;
}

bool AnimationPlayer::isPlaying() const noexcept
{
    return playing_;
}

bool AnimationPlayer::isLooping() const noexcept
{
    return looping_;
}

float AnimationPlayer::currentTime() const noexcept
{
    return currentTime_;
}

float AnimationPlayer::playbackSpeed() const noexcept
{
    return playbackSpeed_;
}

bool AnimationPlayer::sample(
    ScenePose& pose,
    SceneMorphPose& morphPose) noexcept
{
    if (clip_ == nullptr)
    {
        return false;
    }

    bool transformsChanged = false;

    for (const asset::NodeAnimationChannelAsset& channel :
         clip_->channels)
    {
        const scene::Transform* currentTransform =
            pose.localTransform(channel.nodeIndex);

        if (currentTransform == nullptr)
        {
            return false;
        }

        scene::Transform sampledTransform =
            *currentTransform;

        if (!channel.translations.empty())
        {
            sampledTransform.setTranslation(
                sampleVectorTrack(
                    channel.translations,
                    currentTime_,
                    currentTransform->translation()));
        }

        if (!channel.rotations.empty())
        {
            if (!sampledTransform.setRotation(
                    sampleRotationTrack(
                        channel.rotations,
                        currentTime_,
                        currentTransform->rotation())))
            {
                return false;
            }
        }

        if (!channel.scales.empty())
        {
            sampledTransform.setScale(
                sampleVectorTrack(
                    channel.scales,
                    currentTime_,
                    currentTransform->scale()));
        }

        if (!pose.setLocalTransform(
                channel.nodeIndex,
                sampledTransform))
        {
            return false;
        }


        transformsChanged = true;
    }

    if (transformsChanged &&
        !pose.updateWorldMatrices())
    {
        return false;
    }

    std::vector<float> sampledWeights;

    for (const asset::NodeMorphAnimationChannelAsset& channel :
         clip_->morphChannels)
    {
        if (!sampleMorphTrack(
                channel,
                currentTime_,
                sampledWeights) ||
            !morphPose.setWeights(
                channel.nodeIndex,
                sampledWeights))
        {
            return false;
        }
    }

    return true;
}

} // namespace stylized::animation
