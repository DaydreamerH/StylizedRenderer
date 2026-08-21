#include <asset/SceneAsset.hpp>

#include <cstddef>

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

    return true;
}

} // namespace stylized::asset
