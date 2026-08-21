#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace stylized::animation
{

class MorphState final
{
public:
    [[nodiscard]] bool initialize(
        std::size_t targetCount);

    void clear() noexcept;
    void reset() noexcept;

    [[nodiscard]] bool setWeight(
        std::size_t targetIndex,
        float weight) noexcept;

    [[nodiscard]] float weight(
        std::size_t targetIndex) const noexcept;

    [[nodiscard]] std::span<const float>
        weights() const noexcept;

    [[nodiscard]] std::size_t targetCount()
        const noexcept;

    [[nodiscard]] std::size_t activeTargetCount()
        const noexcept;

    [[nodiscard]] std::uint64_t version()
        const noexcept;

private:
    void advanceVersion() noexcept;

    std::vector<float> weights_;

    std::size_t activeTargetCount_ = 0;
    std::uint64_t version_ = 0;
};

} // namespace stylized::animation
