#include <asset/SceneAsset.hpp>

#include <cmath>
#include <cstddef>

#include <glm/geometric.hpp>

namespace stylized::asset
{

bool SceneAsset::isValid() const noexcept
{
    if (nodes.empty()) return false;

    for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
    {
        const SceneNodeAsset& node = nodes[nodeIndex];

        if (!node.hasParent()) continue;
        if (node.parentIndex >= nodes.size()) return false;
        if (node.parentIndex == nodeIndex) return false;
    }

    for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
    {
        std::uint32_t current = static_cast<std::uint32_t>(nodeIndex);

        std::size_t visitedNodeCount = 0;

        while (current != SceneNodeAsset::invalidNodeIndex)
        {
            if (visitedNodeCount >= nodes.size()) return false;

            ++visitedNodeCount;

            current = nodes[current].parentIndex;
        }
    }

    for (const AnimationClipAsset& animation : animations)
    {
        if (!animation.isValid(nodes.size()))
        {
            return false;
        }
    }

    constexpr float directionEpsilon = 1.0e-8F;

    for (const CameraAsset& camera : cameras)
    {
        if (camera.nodeIndex >= nodes.size() ||
            !std::isfinite(camera.verticalFieldOfView) ||
            !std::isfinite(camera.aspectRatio) ||
            !std::isfinite(camera.nearPlane) ||
            !std::isfinite(camera.farPlane) ||
            camera.verticalFieldOfView <= 0.0F ||
            camera.verticalFieldOfView >= 180.0F ||
            camera.aspectRatio < 0.0F ||
            camera.nearPlane <= 0.0F ||
            camera.farPlane <= camera.nearPlane ||
            glm::dot(
                camera.localForward,
                camera.localForward) <= directionEpsilon ||
            glm::dot(
                camera.localUp,
                camera.localUp) <= directionEpsilon ||
            glm::dot(
                glm::cross(
                    camera.localForward,
                    camera.localUp),
                glm::cross(
                    camera.localForward,
                    camera.localUp)) <= directionEpsilon)
        {
            return false;
        }
    }

    return true;
}

} // namespace stylized::asset
