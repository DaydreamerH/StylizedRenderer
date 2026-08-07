#include <render/RenderExtractor.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/SceneAsset.hpp>
#include <scene/Camera.hpp>
#include <asset/MaterialAsset.hpp>

#include <render/RuntimeMesh.hpp>
#include <render/RuntimeResourceCache.hpp>

#include <material/MaterialInstance.hpp>
#include <material/MaterialTemplate.hpp>

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
    const DirectionalLightData& mainLight,
    const asset::AssetHandle<
        material::MaterialTemplate>
        materialTemplate,
    RenderWorld& renderWorld) const
{
    if (!sceneAsset.isValid()) return false;

    renderWorld.clear();

    renderWorld.mainView.view = camera.viewMatrix();

    renderWorld.mainView.projection = camera.projectionMatrix();

    renderWorld.mainView.viewProjection = camera.viewProjectionMatrix();

    renderWorld.mainView.cameraPosition = camera.position();

    renderWorld.mainView.frustum = 
        math::Frustum::fromViewProjection(renderWorld.mainView.viewProjection);
    if (!renderWorld.mainView.frustum.isValid()) return false;

    renderWorld.mainView.mainLight = mainLight;

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

            ++renderWorld.renderStats.totalItems;
            
            RenderItem item;

            item.worldBounds = primitive.localBounds().transformed(worldMatrix);
            if (!renderWorld.mainView.frustum.intersects(item.worldBounds))
            {
                ++renderWorld.renderStats.culledItems;
                continue;
            }

            ++renderWorld.renderStats.visibleItems;

            item.primitive = &primitive;

            const asset::AssetHandle<asset::MaterialAsset>
                sourceMaterialHandle =
                    primitive.material();

            item.materialInstance =
                resourceCache_.getOrCreateMaterialInstance(
                    sourceMaterialHandle,
                    materialTemplate,
                    assetRegistry
                );

            if (item.materialInstance == nullptr)
                return false;

            item.runtimeMaterial =
                resourceCache_.getOrCreateRuntimeMaterial(
                    item.materialInstance->templateHandle,
                    assetRegistry
                );

            if (item.runtimeMaterial == nullptr)
                return false;

            const asset::MaterialAsset* sourceMaterial =
                assetRegistry.get(sourceMaterialHandle);

            if (sourceMaterial != nullptr)
            {
                switch (sourceMaterial->alphaMode)
                {
                case asset::AlphaMode::Opaque:
                    item.materialClass = RenderMaterialClass::Opaque;
                    break;
                case asset::AlphaMode::Mask:
                    item.materialClass = RenderMaterialClass::Masked;
                    break;
                case asset::AlphaMode::Blend:
                    item.materialClass = RenderMaterialClass::Transparent;
                    break;
                }

                if (sourceMaterial->doubleSided)
                {
                    item.flags = item.flags | RenderItemFlags::DoubleSided;
                }
            }
            
            item.world = worldMatrix;
            item.normalMatrix = normalMatrix;

            item.objectId = static_cast<std::uint32_t>(nodeIndex);

            switch (item.materialClass)
            {
            case RenderMaterialClass::Opaque:
                ++renderWorld.renderStats.opaqueItems;
                break;

            case RenderMaterialClass::Masked:
                ++renderWorld.renderStats.maskedItems;
                break;

            case RenderMaterialClass::Transparent:
                ++renderWorld.renderStats.transparentItems;
                break;
            }

            renderWorld.items.push_back(item);
        }

    }

    return true;
}

} // namespace stylized::render
