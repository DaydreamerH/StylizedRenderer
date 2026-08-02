#include <render/RenderExtractor.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/SceneAsset.hpp>
#include <scene/Camera.hpp>

#include <render/RuntimeMesh.hpp>
#include <render/RuntimeResourceCache.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
namespace stylized::render
{
    
RenderExtractor::RenderExtractor(RuntimeResourceCache& resourceCache) noexcept
    : resourceCache_(resourceCache)
{
}

bool RenderExtractor::extract(
    const asset::SceneAsset& sceneAsset,
    const asset::AssetRegistry& assetRegistry,
    const scene::Camera& camera,
    RenderWorld& renderWorld) const
{
    if (!sceneAsset.isValid()) return false;

    renderWorld.clear();

    renderWorld.mainView.view = camera.viewMatrix();

    renderWorld.mainView.projection = camera.projectionMatrix();

    renderWorld.mainView.viewProjection = camera.viewProjectionMatrix();

    renderWorld.mainView.cameraPosition = camera.position();

    const std::size_t nodeCount = sceneAsset.nodes.size();
    
    std::vector<glm::mat4> worldMatrices(nodeCount, glm::mat4{1.F});

    std::vector<std::uint8_t> states(nodeCount, 0);

    std::function<bool(std::size_t) > resolveWorldMatrix;

    resolveWorldMatrix = [&](const std::size_t nodeIndex) -> bool
    {
        if (nodeIndex >= nodeCount) return false;

        if (states[nodeIndex] == 2) return true;

        if(states[nodeIndex] == 1) return false;

        states[nodeIndex] = 1;

        const asset::SceneNodeAsset& node = sceneAsset.nodes[nodeIndex];

        const glm::mat4 localMatrix = node.localTransform.localMatrix();

        if (!node.hasParent()) 
            worldMatrices[nodeIndex] = localMatrix;
        else 
        {
            const std::size_t parentIndex = static_cast<std::size_t>(node.parentIndex);

            if (!resolveWorldMatrix(parentIndex))
            {
                return false;
            }

            worldMatrices[nodeIndex] = worldMatrices[parentIndex] * localMatrix;
        }

        states[nodeIndex] = 2;
        return true;
    };

    for (std::size_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
    {
        if (!resolveWorldMatrix(nodeIndex)) 
            return false;
    }

    for (std::size_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
    {
        const asset::SceneNodeAsset& node = sceneAsset.nodes[nodeIndex];

        if (node.mesh.isNull()) continue;

        const RuntimeMesh* runtimeMesh = resourceCache_.getOrCreateMesh(node.mesh, assetRegistry);

        if (runtimeMesh == nullptr) continue;

        const glm::mat4& worldMatrix = worldMatrices[nodeIndex];
        const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3{worldMatrix}));

        for (const RuntimeMeshPrimitive& primitive : runtimeMesh->primitives())
        {
            if (!primitive.isValid()) continue;
            
            RenderItem item;
            item.primitive = &primitive;
            item.material = primitive.material();
            item.world = worldMatrix;
            item.normalMatrix = normalMatrix;
            item.worldBounds = primitive.localBounds().transformed(worldMatrix);
            item.objectId = static_cast<std::uint32_t>(nodeIndex);
            renderWorld.items.push_back(item);
        }

    }

    return true;
}

} // namespace stylized::render
