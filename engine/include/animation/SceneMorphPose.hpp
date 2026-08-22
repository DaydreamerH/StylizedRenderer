#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace stylized::asset
{
struct SceneAsset;
}

namespace stylized::animation
{

class SceneMorphPose final
{
public:
    [[nodiscard]] bool initialize(
        const asset::SceneAsset& sceneAsset);

    void clear() noexcept;
    void reset() noexcept;

    [[nodiscard]] bool setWeights(
        std::uint32_t nodeIndex,
        std::span<const float> weights);

    [[nodiscard]] std::span<const float> weights(
        std::uint32_t nodeIndex) const noexcept;

    [[nodiscard]] bool isForScene(
        const asset::SceneAsset& sceneAsset) const noexcept;

    [[nodiscard]] std::uint64_t version() const noexcept;

private:
    void advanceVersion() noexcept;

    const asset::SceneAsset* sceneAsset_ = nullptr;

    std::vector<std::vector<float>> nodeWeights_;

    std::uint64_t version_ = 0;
};

} // namespace stylized::animation
