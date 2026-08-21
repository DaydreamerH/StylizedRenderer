#include <animation/MorphState.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace stylized::animation
{

bool MorphState::initialize(
    const std::size_t targetCount)
{
    if (targetCount == 0)
    {
        clear();
        return false;
    }

    weights_.assign(targetCount, 0.0F);
    activeTargetCount_ = 0;

    advanceVersion();

    return true;
}

void MorphState::clear() noexcept
{
    weights_.clear();
    activeTargetCount_ = 0;

    advanceVersion();
}

void MorphState::reset() noexcept
{
    if (activeTargetCount_ == 0)
    {
        return;
    }

    std::fill(
        weights_.begin(),
        weights_.end(),
        0.0F);

    activeTargetCount_ = 0;

    advanceVersion();
}

bool MorphState::setWeight(
    const std::size_t targetIndex,
    const float weight) noexcept
{
    if (targetIndex >= weights_.size() ||
        !std::isfinite(weight))
    {
        return false;
    }

    const float clampedWeight =
        std::clamp(weight, 0.0F, 1.0F);

    const float previousWeight =
        weights_[targetIndex];

    if (previousWeight == clampedWeight)
    {
        return true;
    }

    if (previousWeight == 0.0F)
    {
        ++activeTargetCount_;
    }
    else if (clampedWeight == 0.0F)
    {
        --activeTargetCount_;
    }

    weights_[targetIndex] = clampedWeight;

    advanceVersion();

    return true;
}

float MorphState::weight(
    const std::size_t targetIndex) const noexcept
{
    if (targetIndex >= weights_.size())
    {
        return 0.0F;
    }

    return weights_[targetIndex];
}

std::span<const float>
MorphState::weights() const noexcept
{
    return weights_;
}

std::size_t MorphState::targetCount()
    const noexcept
{
    return weights_.size();
}

std::size_t MorphState::activeTargetCount()
    const noexcept
{
    return activeTargetCount_;
}

std::uint64_t MorphState::version()
    const noexcept
{
    return version_;
}

void MorphState::advanceVersion() noexcept
{
    ++version_;

    if (version_ == 0)
    {
        ++version_;
    }
}

} // namespace stylized::animation
