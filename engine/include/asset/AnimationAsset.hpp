#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace stylized::asset
{

struct VectorAnimationKey
{
    float timeSeconds = 0.0F;
    glm::vec3 value{0.0F};
};

struct QuaternionAnimationKey
{
    float timeSeconds = 0.0F;
    glm::quat value{
        1.0F,
        0.0F,
        0.0F,
        0.0F
    };
};

struct NodeAnimationChannelAsset
{
    static constexpr std::uint32_t invalidNodeIndex =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t nodeIndex = invalidNodeIndex;

    std::vector<VectorAnimationKey> translations;
    std::vector<QuaternionAnimationKey> rotations;
    std::vector<VectorAnimationKey> scales;

    [[nodiscard]] bool isValid(
        std::size_t nodeCount,
        float durationSeconds) const noexcept;
};

struct AnimationClipAsset
{
    std::string name;

    float durationSeconds = 0.0F;

    std::vector<NodeAnimationChannelAsset> channels;

    [[nodiscard]] bool isValid(
        std::size_t nodeCount) const noexcept;
};

} // namespace stylized::asset
