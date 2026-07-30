#pragma once

#include <stylized/asset/AssetHandle.hpp>
#include <stylized/scene/Transform.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace stylized::asset
{
struct MeshAsset;

struct SceneNodeAsset
{
    static constexpr std::uint32_t invalidNodeIndex = 
        std::numeric_limits<std::uint32_t>::max();

    std::string name;
    
    scene::Transform localTransform;
    
    AssetHandle<MeshAsset> mesh;

    std::uint32_t parentIndex = invalidNodeIndex;

    [[nodiscard]] bool hasParent() const noexcept
    {
        return parentIndex != invalidNodeIndex;
    }
};

struct SceneAsset
{
    std::string name;
    std::vector<SceneNodeAsset> nodes;
    [[nodiscard]] bool isValid() const noexcept;
};


} // namespace stylized::asset
