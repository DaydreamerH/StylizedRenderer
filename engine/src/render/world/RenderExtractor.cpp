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

[[nodiscard]] std::uint32_t registerOutlinePolicy(
    const material::MaterialInstance& materialInstance,
    RenderWorld& renderWorld)
{
    const auto existing =
        renderWorld.outlinePolicyIndices.find(
            &materialInstance);

    if (existing !=
        renderWorld.outlinePolicyIndices.end())
    {
        return existing->second;
    }

    const std::uint32_t policyIndex =
        static_cast<std::uint32_t>(
            renderWorld.outlinePolicies.size() + 1U);

    std::uint32_t groupId = policyIndex;

    const std::string& group =
        materialInstance.screenOutline.group;

    if (!group.empty())
    {
        const auto groupIterator =
            renderWorld.outlineGroupIds.find(group);

        if (groupIterator !=
            renderWorld.outlineGroupIds.end())
        {
            groupId = groupIterator->second;
        }
        else
        {
            groupId =
                0x80000000U |
                static_cast<std::uint32_t>(
                    renderWorld.outlineGroupIds.size() + 1U);

            renderWorld.outlineGroupIds.emplace(
                group,
                groupId);
        }
    }

    renderWorld.outlinePolicies.push_back(
        ScreenOutlinePolicy{
            .materialInstance = &materialInstance,
            .groupId = groupId
        });

    renderWorld.outlinePolicyIndices.emplace(
        &materialInstance,
        policyIndex);

    return policyIndex;
}

constexpr float minimumShadowRadius = 1.0e-5F;
constexpr float minimumDirectionLength = 1.0e-6F;

constexpr float shadowEyeDistanceScale = 2.0F;
constexpr float shadowProjectionMarginScale = 0.02F;

[[nodiscard]] RenderItemFlags removeFlag(
    const RenderItemFlags value,
    const RenderItemFlags flag) noexcept
{
    return static_cast<RenderItemFlags>(
        static_cast<std::uint32_t>(value) &
        ~static_cast<std::uint32_t>(flag));
}

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
    const math::Bounds& casterBounds,
    const math::Bounds& receiverBounds,
    const DirectionalLightData& light,
    ShadowView& destination
) noexcept
{
    if (!casterBounds.isValid()) return false;

    const float radius =
        glm::length(casterBounds.extent());

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
        casterBounds.center();

    const float eyeDistance =
        radius * shadowEyeDistanceScale;

    glm::vec3 lightPosition =
        center - lightDirection * eyeDistance;

    const glm::vec3 up =
        std::abs(lightDirection.y) > 0.99F
        ? glm::vec3{1.F, 0.F, 0.F}
        : glm::vec3{0.F, 1.F, 0.F};

    glm::mat4 view =
        glm::lookAtRH(
            lightPosition,
            center,
            up);

    const float projectionMargin = std::max(
        radius * shadowProjectionMarginScale,
        minimumShadowRadius);

    if (receiverBounds.isValid())
    {
        const math::Bounds initialReceiverLightBounds =
            receiverBounds.transformed(view);

        if (!initialReceiverLightBounds.isValid())
        {
            return false;
        }

        const float receiverMaximumZ =
            initialReceiverLightBounds.maximum().z;

        if (!std::isfinite(receiverMaximumZ))
        {
            return false;
        }

        const float backwardShift = std::max(
            receiverMaximumZ + projectionMargin +
                minimumShadowRadius,
            0.0F);

        if (backwardShift > 0.0F)
        {
            lightPosition -= lightDirection * backwardShift;
            view = glm::lookAtRH(
                lightPosition,
                center,
                up);
        }
    }

    // A bounding sphere produces a square shadow projection with substantial
    // unused area for most character poses.  Fit the orthographic projection
    // to the caster bounds in light space so the available shadow texels are
    // concentrated on the actual geometry.
    const math::Bounds lightSpaceBounds =
        casterBounds.transformed(view);

    if (!lightSpaceBounds.isValid())
    {
        return false;
    }

    const glm::vec3 lightSpaceMinimum =
        lightSpaceBounds.minimum();

    const glm::vec3 lightSpaceMaximum =
        lightSpaceBounds.maximum();

    if (!std::isfinite(lightSpaceMinimum.x) ||
        !std::isfinite(lightSpaceMinimum.y) ||
        !std::isfinite(lightSpaceMinimum.z) ||
        !std::isfinite(lightSpaceMaximum.x) ||
        !std::isfinite(lightSpaceMaximum.y) ||
        !std::isfinite(lightSpaceMaximum.z))
    {
        return false;
    }

    float left =
        lightSpaceMinimum.x - projectionMargin;

    float right =
        lightSpaceMaximum.x + projectionMargin;

    float bottom =
        lightSpaceMinimum.y - projectionMargin;

    float top =
        lightSpaceMaximum.y + projectionMargin;

    // Snap the light-space centre to shadow-map texels.  The projection stays
    // tight, while small pose changes no longer translate every shadow edge by
    // a fractional texel between frames.
    if (destination.extent.width > 0 &&
        destination.extent.height > 0)
    {
        const float projectionWidth = right - left;
        const float projectionHeight = top - bottom;

        const float texelWidth =
            projectionWidth /
            static_cast<float>(destination.extent.width);

        const float texelHeight =
            projectionHeight /
            static_cast<float>(destination.extent.height);

        if (std::isfinite(texelWidth) &&
            std::isfinite(texelHeight) &&
            texelWidth > 0.0F &&
            texelHeight > 0.0F)
        {
            const float snappedCenterX =
                std::round((left + right) * 0.5F / texelWidth) *
                texelWidth;

            const float snappedCenterY =
                std::round((bottom + top) * 0.5F / texelHeight) *
                texelHeight;

            left = snappedCenterX - projectionWidth * 0.5F;
            right = snappedCenterX + projectionWidth * 0.5F;
            bottom = snappedCenterY - projectionHeight * 0.5F;
            top = snappedCenterY + projectionHeight * 0.5F;
        }
    }

    // OpenGL right-handed view space looks down -Z.  Keep the near and far
    // planes just outside the complete light-space caster volume.
    float depthMinimumZ = lightSpaceMinimum.z;
    float depthMaximumZ = lightSpaceMaximum.z;

    if (receiverBounds.isValid())
    {
        const math::Bounds receiverLightBounds =
            receiverBounds.transformed(view);

        if (!receiverLightBounds.isValid())
        {
            return false;
        }

        depthMinimumZ = std::min(
            depthMinimumZ,
            receiverLightBounds.minimum().z);
        depthMaximumZ = std::max(
            depthMaximumZ,
            receiverLightBounds.maximum().z);
    }

    const float nearPlane = std::max(
        -depthMaximumZ - projectionMargin,
        minimumShadowRadius);

    const float farPlane =
        -depthMinimumZ + projectionMargin;

    if (!(left < right) ||
        !(bottom < top) ||
        !(nearPlane < farPlane) ||
        !std::isfinite(nearPlane) ||
        !std::isfinite(farPlane))
    {
        return false;
    }

    const glm::mat4 projection =
        glm::orthoRH_NO(
            left,
            right,
            bottom,
            top,
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
    return beginFrame(
            camera,
            mainLight,
            renderWorld) &&
        appendScene(
            sceneAsset,
            scenePose,
            skinningPalettes,
            morphMeshInstances,
            glm::mat4{1.0F},
            nullptr,
            assetRegistry,
            materialTemplate,
            renderWorld) &&
        endFrame(renderWorld);

}

bool RenderExtractor::beginFrame(
    const scene::Camera& camera,
    const DirectionalLightData& mainLight,
    RenderWorld& renderWorld) const
{
    renderWorld.clear();

    renderWorld.mainView.view =
        camera.viewMatrix();

    renderWorld.mainView.projection =
        camera.projectionMatrix();

    renderWorld.mainView.viewProjection =
        camera.viewProjectionMatrix();

    renderWorld.mainView.cameraPosition =
        camera.position();

    renderWorld.mainView.nearPlane =
        camera.nearPlane();

    renderWorld.mainView.farPlane =
        camera.farPlane();

    renderWorld.mainView.frustum =
        math::Frustum::fromViewProjection(
            renderWorld.mainView.viewProjection);

    if (!renderWorld.mainView.frustum.isValid())
    {
        return false;
    }

    renderWorld.mainView.mainLight = mainLight;

    return true;
}

bool RenderExtractor::appendScene(
    const asset::SceneAsset& sceneAsset,
    const animation::ScenePose& scenePose,
    const SkinningPaletteSet& skinningPalettes,
    std::span<const RuntimeMeshInstance>
        morphMeshInstances,
    const glm::mat4& instanceWorldMatrix,
    const FaceSdfExtractionData* faceSdf,
    const asset::AssetRegistry& assetRegistry,
    asset::AssetHandle<
        material::MaterialTemplate>
        materialTemplate,
    RenderWorld& renderWorld) const
{
    if (!scenePose.isForScene(sceneAsset) ||
        scenePose.worldMatricesDirty() ||
        scenePose.nodeCount() != sceneAsset.nodes.size() ||
        morphMeshInstances.size() != sceneAsset.nodes.size())
    {
        return false;
    }

    const std::size_t nodeCount =
        sceneAsset.nodes.size();

    const std::uint32_t objectIdBase =
        renderWorld.nextObjectId;

    renderWorld.nextObjectId +=
        static_cast<std::uint32_t>(nodeCount);

    bool faceSdfFrameValid = false;
    glm::vec3 faceForward{0.0F, 0.0F, 1.0F};
    glm::vec3 faceRight{1.0F, 0.0F, 0.0F};
    glm::vec3 faceUp{0.0F, 1.0F, 0.0F};
    glm::mat4 headWorld{1.0F};

    if (faceSdf != nullptr &&
        !faceSdf->material.isNull() &&
        faceSdf->headNodeIndex < sceneAsset.nodes.size())
    {
        const glm::mat4* headPoseMatrix =
            scenePose.worldMatrix(faceSdf->headNodeIndex);

        if (headPoseMatrix == nullptr)
        {
            return false;
        }

        headWorld = instanceWorldMatrix * *headPoseMatrix;
        const glm::mat3 headBasis{headWorld};

        faceForward =
            headBasis * faceSdf->headForward;
        glm::vec3 rightCandidate =
            headBasis * faceSdf->headRight;

        if (glm::dot(faceForward, faceForward) > 1.0e-8F)
        {
            faceForward = glm::normalize(faceForward);
            rightCandidate -= faceForward *
                glm::dot(rightCandidate, faceForward);

            if (glm::dot(rightCandidate, rightCandidate) > 1.0e-8F)
            {
                faceRight = glm::normalize(rightCandidate);
                faceUp = glm::cross(faceForward, faceRight);

                if (glm::dot(faceUp, faceUp) > 1.0e-8F)
                {
                    faceUp = glm::normalize(faceUp);
                    faceRight = glm::normalize(
                        glm::cross(faceUp, faceForward));
                    faceSdfFrameValid = true;
                }
            }
        }
    }

    if (faceSdfFrameValid &&
        faceSdf != nullptr &&
        faceSdf->hairShadowEnabled)
    {
        if (renderWorld.faceHairShadowView.valid ||
            faceSdf->hairShadowCaster.isNull())
        {
            return false;
        }

        const glm::vec3 center = glm::vec3(
            headWorld *
            glm::vec4(faceSdf->hairShadowLocalCenter, 1.0F));

        const glm::vec3 cameraPosition =
            center +
            faceForward * faceSdf->hairShadowCameraDistance;

        const glm::mat4 view = glm::lookAtRH(
            cameraPosition,
            center,
            faceUp);

        const glm::mat4 projection = glm::orthoRH_NO(
            -faceSdf->hairShadowWidth,
            faceSdf->hairShadowWidth,
            -faceSdf->hairShadowHeight,
            faceSdf->hairShadowHeight,
            minimumShadowRadius,
            faceSdf->hairShadowDepth);

        renderWorld.faceHairShadowView = {
            .valid = true,
            .viewProjection = projection * view,
            .extent = {
                faceSdf->hairShadowResolution,
                faceSdf->hairShadowResolution},
            .uvOffset = faceSdf->hairShadowUvOffset,
            .alphaCutoff = faceSdf->hairShadowAlphaCutoff,
            .softness = faceSdf->hairShadowSoftness,
            .strength = faceSdf->hairShadowStrength};
    }

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

        const glm::mat4 worldMatrix =
            instanceWorldMatrix *
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

            if (faceSdfFrameValid &&
                faceSdf != nullptr &&
                sourceMaterialHandle == faceSdf->material)
            {
                item.faceSdfFrameValid = true;
                item.receivesFaceHairShadow =
                    faceSdf->hairShadowEnabled;
                item.faceForward = faceForward;
                item.faceRight = faceRight;
                item.faceUp = faceUp;
            }

            item.runtimeMaterial =
                resourceCache_.getOrCreateRuntimeMaterial(
                    item.materialInstance->templateHandle,
                    assetRegistry
                );

            if (item.runtimeMaterial == nullptr)
                return false;

            if (faceSdfFrameValid &&
                faceSdf != nullptr &&
                faceSdf->hairShadowEnabled &&
                sourceMaterialHandle == faceSdf->hairShadowCaster)
            {
                renderWorld.faceHairShadowItems.push_back(
                    FaceHairShadowRenderItem{
                        .primitive = &primitive,
                        .vertexArray = vertexArray,
                        .materialInstance = item.materialInstance,
                        .skinningPalette = skinningPalette,
                        .world = worldMatrix});
            }

            if (item.materialInstance->mtoonParameters.has_value() &&
                !item.materialInstance->mtoonParameters->castShadow)
            {
                item.flags = removeFlag(
                    item.flags,
                    RenderItemFlags::CastShadow);
            }

            if (item.materialInstance->mtoonParameters.has_value() &&
                !item.materialInstance->mtoonParameters->receiveShadow)
            {
                item.flags = removeFlag(
                    item.flags,
                    RenderItemFlags::ReceiveShadow);
            }

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
                renderWorld.shadowCasterBounds.expand(item.worldBounds);

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
                shadowItem.excludeFromFaceFilteredShadow =
                    faceSdf != nullptr &&
                    faceSdf->hairShadowEnabled &&
                    sourceMaterialHandle ==
                        faceSdf->hairShadowCaster;
                renderWorld.shadowItems.push_back(shadowItem);
            }

            if (hasFlag(item.flags, RenderItemFlags::ReceiveShadow))
            {
                renderWorld.shadowReceiverBounds.expand(
                    item.worldBounds);
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

            item.objectId =
                objectIdBase +
                static_cast<std::uint32_t>(nodeIndex);

            item.outlinePolicyIndex =
                registerOutlinePolicy(
                    *item.materialInstance,
                    renderWorld);

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

bool RenderExtractor::endFrame(
    RenderWorld& renderWorld) const
{
    if (!renderWorld.shadowCasterBounds.isValid())
    {
        return true;
    }

    return buildDirectionalShadowView(
        renderWorld.shadowCasterBounds,
        renderWorld.shadowReceiverBounds,
        renderWorld.mainView.mainLight,
        renderWorld.shadowView);
}

} // namespace stylized::render
