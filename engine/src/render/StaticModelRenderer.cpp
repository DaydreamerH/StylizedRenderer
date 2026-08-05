#include <render/StaticModelRenderer.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/MaterialAsset.hpp>
#include <asset/TextureAsset.hpp>
#include <graphics/GraphicsCommands.hpp>
#include <graphics/GraphicsDevice.hpp>
#include <graphics/Texture2D.hpp>
#include <render/RuntimeMesh.hpp>
#include <render/RuntimeResourceCache.hpp>
#include <material/MaterialTemplate.hpp>

#include <glm/mat3x3.hpp>

namespace stylized::render
{
StaticModelRenderer::StaticModelRenderer(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::AssetRegistry& assetRegistry,
    RuntimeResourceCache& resourceCache,
    const asset::AssetHandle<
        material::MaterialTemplate>
        materialTemplate) noexcept
    : graphicsDevice_(graphicsDevice),
      assetRegistry_(assetRegistry),
      resourceCache_(resourceCache),
      materialTemplate_(materialTemplate)
{
}

bool StaticModelRenderer::initialize()
{
    if (initialized_)
    {
        return shader_.isValid();
    }

    const material::MaterialTemplate* materialTemplate =
        assetRegistry_.get(materialTemplate_);

    if (materialTemplate == nullptr ||
        !materialTemplate->isValid())
    {
        return false;
    }

    graphics::ShaderProgramDesc shaderDesc;

    shaderDesc.vertexShaderPath =
        materialTemplate->vertexShaderPath;

    shaderDesc.fragmentShaderPath =
        materialTemplate->fragmentShaderPath;

    shaderDesc.debugName =
        materialTemplate->name;

    shader_ =
        graphicsDevice_.createShaderProgram(
            shaderDesc);

    if (!shader_.isValid())
    {
        return false;
    }

    if (!shader_.setInt(
            "uBaseColorTexture",
            0))
    {
        return false;
    }

    initialized_ = true;
    return true;
}

bool StaticModelRenderer::render(const RenderWorld& renderWorld)
{
    lastDrawCallCount_ = 0;

    if (!initialized_ || !shader_.isValid()) return false;

    if (!shader_.setMat4("uViewProjection", renderWorld.mainView.viewProjection)) return false;

    for (const RenderItem& item : renderWorld.items)
    {
        if (item.materialClass != RenderMaterialClass::Opaque) continue;

        if (item.primitive == nullptr) continue;

        const asset::MaterialAsset* material = assetRegistry_.get(item.material);

        glm::vec4 baseColorFactor{1.F};

        asset::AssetHandle<asset::TextureAsset> baseColorTexture;

        if (material != nullptr)
        {
            baseColorFactor = material->baseColorFactor;
            baseColorTexture = material->baseColorTexture;
        }

        const graphics::Texture2D& texture = resourceCache_.getOrCreateTexture(baseColorTexture, assetRegistry_);
        
        if (!texture.isValid()) continue;

        if (!shader_.setMat4("uModel", item.world)) return false;

        if (!shader_.setMat3("uNormalMatrix", item.normalMatrix)) return false;

        if (!shader_.setVec4("uBaseColorFactor", baseColorFactor))
        {
            return false;
        }

        texture.bind(0);

        graphics::DrawIndexedCommand command;

        command.shader = &shader_;

        command.vertexArray = &item.primitive->vertexArray();

        command.topology = graphics::PrimitiveTopology::Triangles;

        command.indexType = item.primitive->indexType();

        command.indexCount = item.primitive->indexCount();

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
