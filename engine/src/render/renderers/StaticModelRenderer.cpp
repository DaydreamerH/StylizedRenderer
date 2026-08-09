#include <render/renderers/StaticModelRenderer.hpp>

#include <asset/AssetRegistry.hpp>
#include <graphics/device/GraphicsCommands.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <graphics/resources/ShaderProgram.hpp>
#include <graphics/resources/DepthTexture.hpp>
#include <render/resources/RuntimeMesh.hpp>
#include <render/resources/RuntimeResourceCache.hpp>
#include <render/resources/RuntimeMaterial.hpp>
#include <material/MaterialInstance.hpp>
#include <material/MaterialTemplate.hpp>

namespace stylized::render
{
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
    const graphics::DepthTexture* shadowMap)
{
    lastDrawCallCount_ = 0;

    for (const RenderItem& item : renderWorld.items)
    {
        if (item.materialClass !=
            RenderMaterialClass::Opaque)
        {
            continue;
        }

        if (item.primitive == nullptr ||
            item.materialInstance == nullptr ||
            item.runtimeMaterial == nullptr)
        {
            continue;
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

            if (materialKind ==
                material::MaterialKind::BasicPbr)
            {
                if (!shader->setVec3(
                        "uCameraPosition",
                        view.cameraPosition.x,
                        view.cameraPosition.y,
                        view.cameraPosition.z))
                {
                    return false;
                }

                const bool shadowEnabled =
                    shadowMap != nullptr &&
                    shadowMap->isValid() &&
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

                if (shadowEnabled)
                {
                    shadowMap->bind(1);
                }
            }
        }

        graphics::DrawIndexedCommand command;

        command.shader = shader;

        command.vertexArray =
            &item.primitive->vertexArray();

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

    return true;
}

std::size_t StaticModelRenderer::lastDrawCallCount() const noexcept
{
    return lastDrawCallCount_;
}

}
