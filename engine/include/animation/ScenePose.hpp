#pragma once

#include <scene/Transform.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>

namespace stylized::asset
{
struct SceneAsset;
}

namespace stylized::animation
{

class ScenePose final
{
public:
    ScenePose() = default;

    [[nodiscard]] bool initialize(
        const asset::SceneAsset& sceneAsset);

    void clear() noexcept;

    [[nodiscard]] bool resetToBindPose() noexcept;

    [[nodiscard]] bool setLocalTransform(
        std::uint32_t nodeIndex,
        const scene::Transform& transform) noexcept;

    [[nodiscard]] const scene::Transform*
        localTransform(
            std::uint32_t nodeIndex) const noexcept;

    [[nodiscard]] const glm::mat4*
        worldMatrix(
            std::uint32_t nodeIndex) const noexcept;

    [[nodiscard]] bool updateWorldMatrices() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept;

    [[nodiscard]] bool isForScene(
        const asset::SceneAsset& sceneAsset) const noexcept;

    [[nodiscard]] bool worldMatricesDirty() const noexcept;

    [[nodiscard]] std::uint64_t version() const noexcept;

    [[nodiscard]] std::size_t nodeCount() const noexcept;

private:
    void advanceVersion() noexcept;

    [[nodiscard]] bool resolveWorldMatrix(
        std::size_t nodeIndex) noexcept;

    const asset::SceneAsset* sceneAsset_ = nullptr;

    std::vector<scene::Transform> localTransforms_;
    std::vector<glm::mat4> worldMatrices_;

    std::vector<std::uint8_t> resolutionStates_;

    bool worldMatricesDirty_ = true;
    std::uint64_t version_ = 0;
};

} // namespace stylized::animation
