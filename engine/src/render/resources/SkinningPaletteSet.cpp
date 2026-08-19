#include <render/resources/SkinningPaletteSet.hpp>

#include <animation/ScenePose.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/MeshAsset.hpp>
#include <asset/SceneAsset.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <render/resources/SkinningPalette.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace stylized::render
{

SkinningPaletteSet::SkinningPaletteSet() = default;

SkinningPaletteSet::~SkinningPaletteSet() = default;

bool SkinningPaletteSet::initialize(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::SceneAsset& sceneAsset,
    const asset::AssetRegistry& assetRegistry)
{
    clear();

    if (!sceneAsset.isValid())
    {
        return false;
    }

    palettes_.resize(sceneAsset.nodes.size());

    for (std::size_t nodeIndex = 0;
         nodeIndex < sceneAsset.nodes.size();
         ++nodeIndex)
    {
        const asset::SceneNodeAsset& node =
            sceneAsset.nodes[nodeIndex];

        if (node.mesh.isNull())
        {
            continue;
        }

        const asset::MeshAsset* mesh =
            assetRegistry.get(node.mesh);

        if (mesh == nullptr ||
            !mesh->isValid())
        {
            clear();
            return false;
        }

        std::vector<
            std::unique_ptr<SkinningPalette>>&
            nodePalettes =
                palettes_[nodeIndex];

        nodePalettes.resize(
            mesh->primitives.size());

        for (std::size_t primitiveIndex = 0;
             primitiveIndex <
                 mesh->primitives.size();
             ++primitiveIndex)
        {
            const asset::MeshPrimitiveAsset&
                primitive =
                    mesh->primitives[
                        primitiveIndex];

            if (!primitive.hasSkin())
            {
                continue;
            }

            auto palette =
                std::make_unique<SkinningPalette>();

            if (!palette->initializeGpuBuffer(
                    graphicsDevice,
                    primitive.skin))
            {
                clear();
                return false;
            }

            nodePalettes[primitiveIndex] =
                std::move(palette);

            ++paletteCount_;
        }
    }

    return true;
}

bool SkinningPaletteSet::update(
    const asset::SceneAsset& sceneAsset,
    const asset::AssetRegistry& assetRegistry,
    const animation::ScenePose& scenePose)
{
    lastUploadCount_ = 0;

    if (!sceneAsset.isValid() ||
        !scenePose.isForScene(sceneAsset) ||
        scenePose.worldMatricesDirty() ||
        palettes_.size() !=
            sceneAsset.nodes.size())
    {
        return false;
    }

    for (std::size_t nodeIndex = 0;
         nodeIndex < sceneAsset.nodes.size();
         ++nodeIndex)
    {
        const asset::SceneNodeAsset& node =
            sceneAsset.nodes[nodeIndex];

        if (node.mesh.isNull())
        {
            continue;
        }

        const asset::MeshAsset* mesh =
            assetRegistry.get(node.mesh);

        if (mesh == nullptr)
        {
            return false;
        }

        const std::vector<
            std::unique_ptr<SkinningPalette>>&
            nodePalettes =
                palettes_[nodeIndex];

        if (nodePalettes.size() !=
            mesh->primitives.size())
        {
            return false;
        }

        for (std::size_t primitiveIndex = 0;
             primitiveIndex <
                 mesh->primitives.size();
             ++primitiveIndex)
        {
            const asset::MeshPrimitiveAsset&
                primitive =
                    mesh->primitives[
                        primitiveIndex];

            const std::unique_ptr<SkinningPalette>&
                palette =
                    nodePalettes[primitiveIndex];

            if (!primitive.hasSkin())
            {
                if (palette != nullptr)
                {
                    return false;
                }

                continue;
            }

            if (palette == nullptr)
            {
                return false;
            }

            if (!palette->update(
                    primitive.skin,
                    static_cast<std::uint32_t>(
                        nodeIndex),
                    scenePose))
            {
                return false;
            }

            if (!palette->upload())
            {
                return false;
            }

            ++lastUploadCount_;
        }
    }

    return true;
}

const SkinningPalette* SkinningPaletteSet::find(
    const std::uint32_t nodeIndex,
    const std::size_t primitiveIndex)
    const noexcept
{
    if (nodeIndex >= palettes_.size())
    {
        return nullptr;
    }

    const std::vector<
        std::unique_ptr<SkinningPalette>>&
        nodePalettes =
            palettes_[nodeIndex];

    if (primitiveIndex >= nodePalettes.size())
    {
        return nullptr;
    }

    return nodePalettes[primitiveIndex].get();
}

void SkinningPaletteSet::clear() noexcept
{
    palettes_.clear();

    paletteCount_ = 0;
    lastUploadCount_ = 0;
}

std::size_t SkinningPaletteSet::paletteCount()
    const noexcept
{
    return paletteCount_;
}

std::size_t SkinningPaletteSet::lastUploadCount()
    const noexcept
{
    return lastUploadCount_;
}

} // namespace stylized::render
