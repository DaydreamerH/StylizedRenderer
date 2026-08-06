#include <render/StaticModelRenderer.hpp>

#include <asset/AssetRegistry.hpp>
#include <graphics/GraphicsCommands.hpp>
#include <graphics/GraphicsDevice.hpp>
#include <graphics/ShaderProgram.hpp>
#include <render/RuntimeMesh.hpp>
#include <render/RuntimeResourceCache.hpp>
#include <render/RuntimeMaterial.hpp>
#include <material/MaterialInstance.hpp>

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
    const RenderWorld& renderWorld)
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
