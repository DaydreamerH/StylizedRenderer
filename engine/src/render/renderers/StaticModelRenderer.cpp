#include <render/renderers/StaticModelRenderer.hpp>

#include <asset/AssetRegistry.hpp>
#include <graphics/device/GraphicsCommands.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <graphics/resources/ShaderProgram.hpp>
#include <graphics/resources/DepthTexture.hpp>
#include <graphics/resources/RenderTexture.hpp>
#include <render/resources/RuntimeMesh.hpp>
#include <render/resources/RuntimeResourceCache.hpp>
#include <render/resources/RuntimeMaterial.hpp>
#include <render/resources/SkinningPalette.hpp>
#include <material/MaterialInstance.hpp>
#include <material/MaterialTemplate.hpp>

#include <cstdint>
#include <algorithm>
#include <string_view>

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

namespace stylized::render
{

namespace
{
    constexpr std::uint32_t skinningPaletteBinding = 0;

    glm::vec3 encodeOutlineMaterialId(
        const std::uint32_t materialId) noexcept
    {
        constexpr float inverseByte =
            1.0F / 255.0F;

        return glm::vec3{
            static_cast<float>(
                materialId & 0xFFU) * inverseByte,
            static_cast<float>(
                (materialId >> 8U) & 0xFFU) * inverseByte,
            static_cast<float>(
                (materialId >> 16U) & 0xFFU) * inverseByte
        };
    }

} // namespace


StaticModelRenderer::StaticModelRenderer(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::AssetRegistry& assetRegistry,
    RuntimeResourceCache& resourceCache) noexcept
    : graphicsDevice_(graphicsDevice),
      assetRegistry_(assetRegistry),
      resourceCache_(resourceCache)
{
}

bool StaticModelRenderer::render(
    const RenderWorld& renderWorld,
    const graphics::DepthTexture& shadowMap,
    const bool shadowMapAvailable,
    const graphics::DepthTexture* faceFilteredShadowMap,
    const graphics::RenderTexture* faceHairShadowMask,
    const StaticModelRenderQueue renderQueue)
{
    lastDrawCallCount_ = 0;

    renderItems_.clear();
    renderItems_.reserve(renderWorld.items.size());

    for (const RenderItem& item : renderWorld.items)
    {
        const bool transparent =
            item.materialClass ==
            RenderMaterialClass::Transparent;

        const bool accepted =
            renderQueue ==
                StaticModelRenderQueue::Transparent
                ? transparent
                : !transparent;

        if (accepted)
        {
            renderItems_.push_back(&item);
        }
    }

    if (renderQueue ==
        StaticModelRenderQueue::Transparent)
    {
        const glm::vec3 cameraPosition =
            renderWorld.mainView.cameraPosition;

        std::stable_sort(
            renderItems_.begin(),
            renderItems_.end(),
            [cameraPosition](
                const RenderItem* left,
                const RenderItem* right)
            {
                const glm::vec3 leftOffset =
                    left->worldBounds.center() -
                    cameraPosition;

                const glm::vec3 rightOffset =
                    right->worldBounds.center() -
                    cameraPosition;

                return glm::dot(leftOffset, leftOffset) >
                    glm::dot(rightOffset, rightOffset);
            });
    }

    for (const RenderItem* itemPointer : renderItems_)
    {
        const RenderItem& item = *itemPointer;

        if (item.primitive == nullptr ||
            item.vertexArray == nullptr ||
            item.materialInstance == nullptr ||
            item.runtimeMaterial == nullptr)
        {
            continue;
        }

        if (hasFlag(
                item.flags,
                RenderItemFlags::DoubleSided))
        {
            graphicsDevice_.setCullMode(
                graphics::CullMode::None);
        }
        else
        {
            const bool windingFlipped =
                glm::determinant(
                    glm::mat3(item.world)) < 0.0F;

            graphicsDevice_.setCullMode(
                windingFlipped
                    ? graphics::CullMode::Front
                    : graphics::CullMode::Back);
        }

        RuntimeMaterial& runtimeMaterial =
            *item.runtimeMaterial;

        const material::MaterialInstance&
            materialInstance =
                *item.materialInstance;

        graphics::ShaderProgram* shader =
            runtimeMaterial.shader();

        if (shader == nullptr)
        {
            return false;
        }

        if (!runtimeMaterial.bind(
                materialInstance,
                resourceCache_,
                assetRegistry_))
        {
            return false;
        }

        const bool alphaMaskEnabled =
            item.materialClass == RenderMaterialClass::Masked;

        if (!shader->setInt(
                "uAlphaMaskEnabled",
                alphaMaskEnabled ? 1 : 0
        ))
        {
            return false;
        }

        if (!shader->setFloat(
                "uAlphaCutoff",
                materialInstance.alphaCutoff))
        {
            return false;
        }

        const glm::vec3 outlineMaterialId =
            encodeOutlineMaterialId(
                item.outlinePolicyIndex);

        if (!shader->setVec3(
                "uOutlineMaterialId",
                outlineMaterialId.x,
                outlineMaterialId.y,
                outlineMaterialId.z))
        {
            return false;
        }

        if (!shader->setMat4(
                "uViewProjection",
                renderWorld.mainView.viewProjection))
        {
            return false;
        }

        if (!shader->setMat4(
                "uModel",
                item.world))
        {
            return false;
        }

        if (!shader->setMat3(
                "uNormalMatrix",
                item.normalMatrix))
        {
            return false;
        }

        const bool skinningEnabled =
            item.skinningPalette != nullptr;

        if (skinningEnabled &&
            !item.skinningPalette->isGpuReady())
        {
            return false;
        }

        if (!shader->setInt(
                "uSkinningEnabled",
                skinningEnabled ? 1 : 0))
        {
            return false;
        }

        if (skinningEnabled)
        {
            item.skinningPalette->bind(
                skinningPaletteBinding);
        }

        const material::MaterialKind materialKind =
            runtimeMaterial.kind();

        if (materialKind ==
                material::MaterialKind::BasicPbr ||
            materialKind ==
                material::MaterialKind::MToon)
        {
            const RenderView& view =
                renderWorld.mainView;

            const DirectionalLightData& light =
                view.mainLight;

            if (!shader->setVec3(
                    "uLightDirection",
                    light.direction.x,
                    light.direction.y,
                    light.direction.z))
            {
                return false;
            }

            if (!shader->setVec3(
                    "uLightColor",
                    light.color.x,
                    light.color.y,
                    light.color.z))
            {
                return false;
            }

            if (!shader->setFloat(
                    "uLightIntensity",
                    light.intensity))
            {
                return false;
            }

            if (!shader->setVec3(
                    "uCameraPosition",
                    view.cameraPosition.x,
                    view.cameraPosition.y,
                    view.cameraPosition.z))
            {
                return false;
            }

            if (materialKind ==
                material::MaterialKind::MToon)
            {
                const bool faceSdfEnabled =
                    item.faceSdfFrameValid &&
                    materialInstance.mtoonParameters.has_value() &&
                    materialInstance.mtoonParameters->faceSdf.enabled &&
                    !materialInstance.mtoonParameters
                        ->faceSdf.texture.isNull();

                const bool faceHairShadowEnabled =
                    item.receivesFaceHairShadow &&
                    faceHairShadowMask != nullptr &&
                    faceHairShadowMask->isValid() &&
                    renderWorld.faceHairShadowView.valid;

                if (!shader->setInt(
                        "uFaceSdfEnabled",
                        faceSdfEnabled ? 1 : 0) ||
                    !shader->setVec3(
                        "uFaceForward",
                        item.faceForward) ||
                    !shader->setVec3(
                        "uFaceRight",
                        item.faceRight) ||
                    !shader->setVec3(
                        "uFaceUp",
                        item.faceUp) ||
                    !shader->setInt(
                        "uFaceHairShadowEnabled",
                        faceHairShadowEnabled ? 1 : 0) ||
                    !shader->setMat4(
                        "uFaceHairShadowViewProjection",
                        renderWorld.faceHairShadowView.viewProjection) ||
                    !shader->setVec2(
                        "uFaceHairShadowUvOffset",
                        renderWorld.faceHairShadowView.uvOffset.x,
                        renderWorld.faceHairShadowView.uvOffset.y) ||
                    !shader->setFloat(
                        "uFaceHairShadowSoftness",
                        renderWorld.faceHairShadowView.softness) ||
                    !shader->setFloat(
                        "uFaceHairShadowStrength",
                        renderWorld.faceHairShadowView.strength))
                {
                    return false;
                }

                if (faceHairShadowEnabled)
                {
                    faceHairShadowMask->bind(12);
                }

                if (!shader->setMat4(
                    "uView",
                    view.view
                ))
                {
                    return false;
                }

                if (!shader->setInt(
                        "uMToonDebugView",
                        static_cast<int>(
                            view.mtoonDebugView)))
                {
                    return false;
                }

                const EnvironmentLightData& environment =
                    view.environmentLight;

                if (!shader->setVec3(
                    "uEnvironmentSkyColor",
                    environment.skyColor.r,
                    environment.skyColor.g,
                    environment.skyColor.b
                ))
                {
                    return false;
                }

                if (!shader->setVec3(
                    "uEnvironmentGroundColor",
                    environment.groundColor.r,
                    environment.groundColor.g,
                    environment.groundColor.b))
                {
                    return false;
                }

                if (!shader->setFloat(
                    "uEnvironmentIntensity",
                    environment.intensity))
                {
                    return false;
                }
            }

            const bool materialReceivesShadow =
                materialKind != material::MaterialKind::MToon ||
                !materialInstance.mtoonParameters.has_value() ||
                materialInstance.mtoonParameters->receiveShadow;

            const bool projectedFaceShadowDisabled =
                item.faceSdfFrameValid &&
                materialKind == material::MaterialKind::MToon &&
                materialInstance.mtoonParameters.has_value() &&
                materialInstance.mtoonParameters
                    ->faceSdf.enabled &&
                materialInstance.mtoonParameters
                    ->faceSdf.disableProjectedShadows;

            const bool faceFilteredShadowAvailable =
                item.faceSdfFrameValid &&
                faceFilteredShadowMap != nullptr &&
                faceFilteredShadowMap->isValid();

            const bool shadowEnabled =
                (shadowMapAvailable || faceFilteredShadowAvailable) &&
                materialReceivesShadow &&
                !projectedFaceShadowDisabled &&
                hasFlag(
                    item.flags,
                    RenderItemFlags::ReceiveShadow);

            if (!shader->setInt(
                    "uShadowEnabled",
                    shadowEnabled ? 1 : 0))
            {
                return false;
            }

            if (!shader->setMat4(
                    "uLightViewProjection",
                    renderWorld.shadowView.viewProjection))
            {
                return false;
            }

            if (faceFilteredShadowAvailable)
            {
                faceFilteredShadowMap->bind(1);
            }
            else
            {
                shadowMap.bind(1);
            }
        }

        graphics::DrawIndexedCommand command;

        command.shader = shader;

        command.vertexArray =
            item.vertexArray;

        command.topology =
            graphics::PrimitiveTopology::Triangles;

        command.indexType =
            item.primitive->indexType();

        command.indexCount =
            item.primitive->indexCount();

        command.firstIndex = 0;

        graphicsDevice_.drawIndexed(command);

        ++lastDrawCallCount_;
    }

    graphicsDevice_.setCullMode(
        graphics::CullMode::Back);

    return true;
}

std::size_t StaticModelRenderer::lastDrawCallCount() const noexcept
{
    return lastDrawCallCount_;
}

}
