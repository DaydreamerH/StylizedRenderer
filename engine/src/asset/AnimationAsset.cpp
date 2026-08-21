#include <asset/AnimationAsset.hpp>

#include <cmath>
#include <cstddef>
#include <vector>

#include <glm/geometric.hpp>

namespace stylized::asset
{

namespace
{

constexpr float maximumTimeError = 1.0e-4F;
constexpr float minimumQuaternionLengthSquared = 1.0e-8F;
constexpr float quaternionNormalizationError = 1.0e-3F;

[[nodiscard]] bool isFinite(
    const glm::vec3& value) noexcept
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

[[nodiscard]] bool isFinite(
    const glm::quat& value) noexcept
{
    return
        std::isfinite(value.w) &&
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

template<typename Key, typename ValueValidator>
[[nodiscard]] bool isTrackValid(
    const std::vector<Key>& keys,
    const float durationSeconds,
    ValueValidator&& valueValidator) noexcept
{
    float previousTime = 0.0F;
    bool hasPreviousTime = false;

    for (const Key& key : keys)
    {
        if (!std::isfinite(key.timeSeconds) ||
            key.timeSeconds < 0.0F ||
            key.timeSeconds >
                durationSeconds + maximumTimeError)
        {
            return false;
        }

        if (hasPreviousTime &&
            key.timeSeconds < previousTime)
        {
            return false;
        }

        if (!valueValidator(key.value))
        {
            return false;
        }

        previousTime = key.timeSeconds;
        hasPreviousTime = true;
    }

    return true;
}

[[nodiscard]] bool isQuaternionValid(
    const glm::quat& value) noexcept
{
    if (!isFinite(value))
    {
        return false;
    }

    const float lengthSquared =
        glm::dot(value, value);

    return
        lengthSquared > minimumQuaternionLengthSquared &&
        std::abs(lengthSquared - 1.0F) <=
            quaternionNormalizationError;
}

} // namespace

bool NodeAnimationChannelAsset::isValid(
    const std::size_t nodeCount,
    const float durationSeconds) const noexcept
{
    if (nodeIndex >= nodeCount)
    {
        return false;
    }

    if (translations.empty() &&
        rotations.empty() &&
        scales.empty())
    {
        return false;
    }

    if (!isTrackValid(
            translations,
            durationSeconds,
            [](const glm::vec3& value)
            {
                return isFinite(value);
            }))
    {
        return false;
    }

    if (!isTrackValid(
            rotations,
            durationSeconds,
            [](const glm::quat& value)
            {
                return isQuaternionValid(value);
            }))
    {
        return false;
    }

    return isTrackValid(
        scales,
        durationSeconds,
        [](const glm::vec3& value)
        {
            return isFinite(value);
        });
}

bool AnimationClipAsset::isValid(
    const std::size_t nodeCount) const noexcept
{
    if (name.empty() ||
        !std::isfinite(durationSeconds) ||
        durationSeconds <= 0.0F ||
        channels.empty())
    {
        return false;
    }

    for (std::size_t channelIndex = 0;
         channelIndex < channels.size();
         ++channelIndex)
    {
        const NodeAnimationChannelAsset& channel =
            channels[channelIndex];

        if (!channel.isValid(
                nodeCount,
                durationSeconds))
        {
            return false;
        }

        for (std::size_t previousIndex = 0;
             previousIndex < channelIndex;
             ++previousIndex)
        {
            if (channels[previousIndex].nodeIndex ==
                channel.nodeIndex)
            {
                return false;
            }
        }
    }

    return true;
}

} // namespace stylized::asset
