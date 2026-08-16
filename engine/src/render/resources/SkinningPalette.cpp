#include <render/resources/SkinningPalette.hpp>

#include <animation/ScenePose.hpp>
#include <asset/MeshAsset.hpp>

#include <cmath>
#include <cstddef>

#include <glm/matrix.hpp>

namespace stylized::render
{

namespace
{

constexpr float minimumAbsoluteDeterminant =
    1.0e-8F;

} // namespace

bool SkinningPalette::update(
    const asset::SkinAsset& skin,
    const std::uint32_t meshNodeIndex,
    const animation::ScenePose& pose)
{
    clear();

    if (!skin.isValid() ||
        !pose.isInitialized() ||
        pose.worldMatricesDirty())
    {
        return false;
    }

    const glm::mat4* meshWorld =
        pose.worldMatrix(meshNodeIndex);

    if (meshWorld == nullptr)
    {
        return false;
    }

    const float determinant =
        glm::determinant(*meshWorld);

    if (!std::isfinite(determinant) ||
        std::abs(determinant) <=
            minimumAbsoluteDeterminant)
    {
        return false;
    }

    const glm::mat4 inverseMeshWorld =
        glm::inverse(*meshWorld);

    matrices_.reserve(
        skin.jointNodeIndices.size());

    for (std::size_t jointIndex = 0;
         jointIndex <
             skin.jointNodeIndices.size();
         ++jointIndex)
    {
        const std::uint32_t jointNodeIndex =
            skin.jointNodeIndices[jointIndex];

        const glm::mat4* jointWorld =
            pose.worldMatrix(jointNodeIndex);

        if (jointWorld == nullptr)
        {
            clear();
            return false;
        }

        matrices_.push_back(
            inverseMeshWorld *
            *jointWorld *
            skin.inverseBindMatrices[jointIndex]);
    }

    return true;
}

void SkinningPalette::clear() noexcept
{
    matrices_.clear();
}

bool SkinningPalette::empty() const noexcept
{
    return matrices_.empty();
}

std::size_t SkinningPalette::size() const noexcept
{
    return matrices_.size();
}

const std::vector<glm::mat4>&
SkinningPalette::matrices() const noexcept
{
    return matrices_;
}

} // namespace stylized::render
