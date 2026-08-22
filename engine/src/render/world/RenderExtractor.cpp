#include <render/world/RenderExtractor.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/MeshAsset.hpp>
#include <asset/SceneAsset.hpp>
#include <scene/Camera.hpp>
#include <asset/MaterialAsset.hpp>

#include <render/resources/RuntimeMesh.hpp>
#include <render/resources/RuntimeMeshInstance.hpp>
#include <render/resources/RuntimeResourceCache.hpp>
#include <render/resources/SkinningPalette.hpp>
#include <render/resources/SkinningPaletteSet.hpp>

#include <animation/ScenePose.hpp>

#include <material/MaterialInstance.hpp>
#include <material/MaterialTemplate.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <span>
#include <vector>

namespace stylized::render
{

namespace
{

constexpr float minimumShadowRadius = 1.0e-5F;
constexpr float minimumDirectionLength = 1.0e-6F;

constexpr float shadowNearPlaneScale = 0.01F;
constexpr float shadowEyeDistanceScale = 2.0F;
constexpr float shadowFarMarginScale = 2.0F;

[[nodiscard]] float maximumLinearScale(
    const std::vector<glm::mat4>& matrices) noexcept
{
    float maximumScale = 0.0F;

    for (const glm::mat4& matrix : matrices)
    {
        float squaredFrobeniusNorm = 0.0F;

        for (glm::length_t column = 0;
             column < 3;
             ++column)
        {
            for (glm::length_t row = 0;
                 row < 3;
                 ++row)
            {
                const float value =
                    matrix[column][row];

                squaredFrobeniusNorm +=
                    value * value;
            }
        }

        maximumScale = std::max(
            maximumScale,
            std::sqrt(squaredFrobeniusNorm));
    }

    return maximumScale;
}

[[nodiscard]] math::Bounds expandBounds(
    const math::Bounds& bounds,
    const float radius) noexcept
{
    if (!bounds.isValid() ||
        !std::isfinite(radius) ||
        radius <= 0.0F)
    {
        return bounds;
    }

    const glm::vec3 expansion{radius};

    return math::Bounds{
        bounds.minimum() - expansion,
        bounds.maximum() + expansion};
}

bool buildDirectionalShadowView(
    const math::Bounds& bounds,
    const DirectionalLightData& light,
    ShadowView& destination
) noexcept
{
    if (!bounds.isValid()) return false;

    const float radius =
        glm::length(bounds.extent());

    if (!std::isfinite(radius) ||
        radius <= minimumShadowRadius)
    {
        return false;
    }

    const float directionLength =
        glm::length(light.direction);

    if (!std::isfinite(directionLength) ||
        directionLength <= minimumDirectionLength)
    {
        return false;
    }

    const glm::vec3 lightDirection =
        glm::normalize(light.direction);

    const glm::vec3 center =
        bounds.center();

    const float eyeDistance =
        radius * shadowEyeDistanceScale;

    const glm::vec3 lightPosition =
        center - lightDirection * eyeDistance;

    const glm::vec3 up =
        std::abs(lightDirection.y) > 0.99F
        ? glm::vec3{1.F, 0.F, 0.F}
        : glm::vec3{0.F, 1.F, 0.F};

    const float nearPlane = std::max(
        radius * shadowNearPlaneScale,
        minimumShadowRadius
    );

    const float farPlane =
        eyeDistance + radius * shadowFarMarginScale;

    const glm::mat4 view =
        glm::lookAtRH(
            lightPosition,
            center,
            up);

    const glm::mat4 projection =
        glm::orthoRH_NO(
            -radius,
            radius,
            -radius,
            radius,
            nearPlane,
            farPlane);

    destination.viewProjection =
        projection * view;

    return true;
}

} // namespace
    
RenderExtractor::RenderExtractor(RuntimeResourceCache& resourceCache) noexcept
    : resourceCache_(resourceCache)
{
}

bool RenderExtractor::extract(
    const asset::SceneAsset& sceneAsset,
    const animation::ScenePose& scenePose,
    const SkinningPaletteSet& skinningPalettes,
    const std::span<const RuntimeMeshInstance>
        morphMeshInstances,
    const asset::AssetRegistry& assetRegistry,
    const scene::Camera& camera,
    const DirectionalLightData& mainLight,
    const asset::AssetHandle<
        material::MaterialTemplate>
        materialTemplate,
    RenderWorld& renderWorld) const
{
    if (!scenePose.isForScene(sceneAsset) ||
        scenePose.worldMatricesDirty() ||
        scenePose.nodeCount() !=
            sceneAsset.nodes.size() ||
        morphMeshInstances.size() !=
            sceneAsset.nodes.size())
    {
        return false;
    }

    renderWorld.clear();

    renderWorld.mainView.view = camera.viewMatrix();

    renderWorld.mainView.projection = camera.projectionMatrix();

    renderWorld.mainView.viewProjection = camera.viewProjectionMatrix();

    renderWorld.mainView.cameraPosition = camera.position();

    renderWorld.mainView.nearPlane = camera.nearPlane();

    renderWorld.mainView.farPlane = camera.farPlane();

    renderWorld.mainView.frustum = 
        math::Frustum::fromViewProjection(renderWorld.mainView.viewProjection);
    if (!renderWorld.mainView.frustum.isValid()) return false;

    renderWorld.mainView.mainLight = mainLight;

    const std::size_t nodeCount =
        sceneAsset.nodes.size();

    math::Bounds shadowCasterBounds;

    for (std::size_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
    {
        const asset::SceneNodeAsset& node = sceneAsset.nodes[nodeIndex];

        if (node.mesh.isNull()) continue;

        const asset::MeshAsset* sourceMesh =
            assetRegistry.get(node.mesh);

        if (sourceMesh == nullptr)
        {
            return false;
        }

        const RuntimeMesh* runtimeMesh =
            resourceCache_.getOrCreateMesh(
                node.mesh,
                assetRegistry);

        if (runtimeMesh == nullptr)
        {
            return false;
        }

        const RuntimeMeshInstance& morphMeshInstance =
            morphMeshInstances[nodeIndex];

        const std::vector<RuntimeMeshPrimitive>&
            runtimePrimitives =
                runtimeMesh->primitives();

        if (runtimePrimitives.size() !=
            sourceMesh->primitives.size())
        {
            return false;
        }

        const glm::mat4* poseWorldMatrix =
            scenePose.worldMatrix(
                static_cast<std::uint32_t>(
                    nodeIndex));

        if (poseWorldMatrix == nullptr)
        {
            return false;
        }

        const glm::mat4& worldMatrix =
            *poseWorldMatrix;

        const glm::mat3 normalMatrix =
            glm::transpose(
                glm::inverse(
                    glm::mat3{worldMatrix}));

        for (std::size_t primitiveIndex = 0;
             primitiveIndex <
                 runtimePrimitives.size();
             ++primitiveIndex)
        {
            const RuntimeMeshPrimitive& primitive =
                runtimePrimitives[primitiveIndex];

            const asset::MeshPrimitiveAsset&
                sourcePrimitive =
                    sourceMesh->primitives[
                        primitiveIndex];

            if (!primitive.isValid()) continue;

            const graphics::VertexArray* vertexArray =
                &primitive.vertexArray();

            const math::Bounds* morphedLocalBounds =
                &primitive.localBounds();

            if (sourcePrimitive.hasMorphTargets())
            {
                if (!morphMeshInstance.isValid())
                {
                    return false;
                }

                vertexArray =
                    morphMeshInstance.vertexArray(
                        primitiveIndex);

                morphedLocalBounds =
                    morphMeshInstance.localBounds(
                        primitiveIndex);

                if (vertexArray == nullptr ||
                    !vertexArray->isValid() ||
                    morphedLocalBounds == nullptr ||
                    !morphedLocalBounds->isValid())
                {
                    return false;
                }
            }

            const SkinningPalette* skinningPalette =
                skinningPalettes.find(
                    static_cast<std::uint32_t>(
                        nodeIndex),
                    primitiveIndex);

            if (sourcePrimitive.hasSkin() !=
                (skinningPalette != nullptr))
            {
                return false;
            }

            ++renderWorld.renderStats.totalItems;
            
            RenderItem item;

            item.vertexArray = vertexArray;
            item.skinningPalette =
                skinningPalette;

            if (!sourcePrimitive.hasSkin())
            {
                item.worldBounds =
                    morphedLocalBounds->transformed(
                        worldMatrix);
            }
            else
            {
                math::Bounds skinnedLocalBounds =
                    skinningPalette
                        ->currentLocalBounds();

                if (sourcePrimitive.hasMorphTargets())
                {
                    const float morphRadius =
                        morphMeshInstance
                            .maximumPositionDelta(
                                primitiveIndex) *
                        maximumLinearScale(
                            skinningPalette->matrices());

                    skinnedLocalBounds = expandBounds(
                        skinnedLocalBounds,
                        morphRadius);
                }

                item.worldBounds =
                    skinnedLocalBounds.transformed(
                        worldMatrix);
            }

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

            if (hasFlag(item.flags, RenderItemFlags::CastShadow)
                && item.materialClass !=
                    RenderMaterialClass::Transparent)
            {
                shadowCasterBounds.expand(item.worldBounds);

                ShadowRenderItem shadowItem;
                shadowItem.primitive = &primitive;
                shadowItem.vertexArray = vertexArray;
                shadowItem.materialInstance =
                    item.materialInstance;
                shadowItem.skinningPalette =
                    skinningPalette;
                shadowItem.world = worldMatrix;
                shadowItem.materialClass =
                    item.materialClass;
                renderWorld.shadowItems.push_back(shadowItem);
            }

            if (!renderWorld.mainView.frustum.intersects(item.worldBounds))
            {
                ++renderWorld.renderStats.culledItems;
                continue;
            }

            ++renderWorld.renderStats.visibleItems;

            item.primitive = &primitive;

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

    if (shadowCasterBounds.isValid())
    {
        if (!buildDirectionalShadowView(
            shadowCasterBounds,
            mainLight,
            renderWorld.shadowView
        ))
        {
            return false;
        }
    }

    return true;
}

} // namespace stylized::render
