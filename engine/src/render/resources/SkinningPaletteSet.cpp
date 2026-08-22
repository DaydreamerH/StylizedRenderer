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
#include <vector>

#include <glm/mat4x4.hpp>

namespace stylized::render
{

class SkinningPaletteGroup final
{
public:
    std::uint32_t meshNodeIndex = 0;
    asset::SkinAsset skin;
    SkinningPalette palette;
};

namespace
{

[[nodiscard]] bool matricesEqual(
    const glm::mat4& left,
    const glm::mat4& right) noexcept
{
    for (glm::length_t column = 0;
         column < 4;
         ++column)
    {
        for (glm::length_t row = 0;
             row < 4;
             ++row)
        {
            if (left[column][row] !=
                right[column][row])
            {
                return false;
            }
        }
    }

    return true;
}

[[nodiscard]] bool skinDefinitionsEqual(
    const asset::SkinAsset& left,
    const asset::SkinAsset& right) noexcept
{
    if (left.jointNodeIndices !=
            right.jointNodeIndices ||
        left.inverseBindMatrices.size() !=
            right.inverseBindMatrices.size())
    {
        return false;
    }

    for (std::size_t jointIndex = 0;
         jointIndex <
             left.inverseBindMatrices.size();
         ++jointIndex)
    {
        if (!matricesEqual(
                left.inverseBindMatrices[jointIndex],
                right.inverseBindMatrices[jointIndex]))
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool mergeJointBounds(
    asset::SkinAsset& destination,
    const asset::SkinAsset& source) noexcept
{
    if (!skinDefinitionsEqual(
            destination,
            source) ||
        destination.jointLocalBounds.size() !=
            source.jointLocalBounds.size())
    {
        return false;
    }

    for (std::size_t jointIndex = 0;
         jointIndex <
             destination.jointLocalBounds.size();
         ++jointIndex)
    {
        destination.jointLocalBounds[jointIndex]
            .expand(
                source.jointLocalBounds[jointIndex]);
    }

    return true;
}

} // namespace

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

    paletteLookup_.resize(
        sceneAsset.nodes.size());

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

        std::vector<SkinningPalette*>&
            nodePalettes =
                paletteLookup_[nodeIndex];

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

            SkinningPaletteGroup* group = nullptr;

            for (const std::unique_ptr<
                     SkinningPaletteGroup>&
                     candidate : paletteGroups_)
            {
                if (candidate->meshNodeIndex ==
                        nodeIndex &&
                    skinDefinitionsEqual(
                        candidate->skin,
                        primitive.skin))
                {
                    group = candidate.get();
                    break;
                }
            }

            if (group == nullptr)
            {
                auto newGroup =
                    std::make_unique<
                        SkinningPaletteGroup>();

                newGroup->meshNodeIndex =
                    static_cast<std::uint32_t>(
                        nodeIndex);

                newGroup->skin = primitive.skin;

                if (!newGroup->palette
                         .initializeGpuBuffer(
                             graphicsDevice,
                             newGroup->skin))
                {
                    clear();
                    return false;
                }

                group = newGroup.get();

                paletteGroups_.push_back(
                    std::move(newGroup));

                ++paletteCount_;
                jointMatrixCount_ +=
                    primitive.skin
                        .jointNodeIndices.size();
            }
            else if (!mergeJointBounds(
                         group->skin,
                         primitive.skin))
            {
                clear();
                return false;
            }

            nodePalettes[primitiveIndex] =
                &group->palette;
        }
    }

    return true;
}

bool SkinningPaletteSet::update(
    const asset::SceneAsset& sceneAsset,
    const animation::ScenePose& scenePose)
{
    lastUploadCount_ = 0;

    if (!scenePose.isForScene(sceneAsset) ||
        scenePose.worldMatricesDirty() ||
        paletteLookup_.size() !=
            sceneAsset.nodes.size())
    {
        return false;
    }

    const std::uint64_t poseVersion =
        scenePose.version();

    if (poseVersion != 0 &&
        poseVersion == lastPoseVersion_)
    {
        return true;
    }

    for (const std::unique_ptr<
             SkinningPaletteGroup>& group :
         paletteGroups_)
    {
        if (group == nullptr)
        {
            return false;
        }

        if (!group->palette.update(
                group->skin,
                group->meshNodeIndex,
                scenePose))
        {
            return false;
        }

        if (!group->palette.upload())
        {
            return false;
        }

        ++lastUploadCount_;
    }

    lastPoseVersion_ = poseVersion;

    return true;
}

const SkinningPalette* SkinningPaletteSet::find(
    const std::uint32_t nodeIndex,
    const std::size_t primitiveIndex)
    const noexcept
{
    if (nodeIndex >= paletteLookup_.size())
    {
        return nullptr;
    }

    const std::vector<SkinningPalette*>&
        nodePalettes =
            paletteLookup_[nodeIndex];

    if (primitiveIndex >= nodePalettes.size())
    {
        return nullptr;
    }

    return nodePalettes[primitiveIndex];
}

void SkinningPaletteSet::clear() noexcept
{
    paletteLookup_.clear();
    paletteGroups_.clear();

    paletteCount_ = 0;
    jointMatrixCount_ = 0;
    lastUploadCount_ = 0;
    lastPoseVersion_ = 0;
}

std::size_t SkinningPaletteSet::paletteCount()
    const noexcept
{
    return paletteCount_;
}

std::size_t SkinningPaletteSet::jointMatrixCount()
    const noexcept
{
    return jointMatrixCount_;
}

std::size_t SkinningPaletteSet::lastUploadCount()
    const noexcept
{
    return lastUploadCount_;
}

} // namespace stylized::render
