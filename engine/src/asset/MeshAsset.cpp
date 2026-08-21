#include <asset/MeshAsset.hpp>

#include <cmath>

namespace stylized::asset
{

namespace
{

constexpr float skinWeightError = 1.0e-4F;

[[nodiscard]] bool isFinite(
    const glm::mat4& matrix) noexcept
{
    for (glm::length_t column = 0;
         column < 4;
         ++column)
    {
        for (glm::length_t row = 0;
             row < 4;
             ++row)
        {
            if (!std::isfinite(
                    matrix[column][row]))
            {
                return false;
            }
        }
    }

    return true;
}

[[nodiscard]] bool isSkinVertexValid(
    const VertexSkinData& vertex,
    const std::size_t jointCount) noexcept
{
    float totalWeight = 0.0F;

    for (glm::length_t component = 0;
         component < 4;
         ++component)
    {
        const float weight =
            vertex.weights[component];

        if (!std::isfinite(weight) ||
            weight < 0.0F)
        {
            return false;
        }

        if (vertex.joints[component] >=
            jointCount)
        {
            return false;
        }

        totalWeight += weight;
    }

    return
        std::abs(totalWeight - 1.0F) <=
            skinWeightError;
}

} // namespace

bool SkinAsset::empty() const noexcept
{
    return
        jointNodeIndices.empty() &&
        inverseBindMatrices.empty() &&
        jointLocalBounds.empty();
}

bool SkinAsset::isValid() const noexcept
{
    if (jointNodeIndices.empty() ||
        jointNodeIndices.size() !=
            inverseBindMatrices.size() ||
        jointNodeIndices.size() !=
            jointLocalBounds.size())
    {
        return false;
    }

    for (const glm::mat4& inverseBindMatrix :
        inverseBindMatrices)
    {
        if (!isFinite(inverseBindMatrix))
        {
            return false;
        }
    }

    return true;
}

void MeshPrimitiveAsset::rebuildBounds() noexcept
{
    localBounds.reset();

    for (const StaticMeshVertex& vertex : vertices)
    {
        localBounds.expand(vertex.position);
    }
}

bool MeshPrimitiveAsset::isValid() const noexcept
{
    if (vertices.empty())
    {
        return false;
    }

    if (indices.empty())
    {
        return false;
    }

    // 当前阶段只支持 Triangle List
    if (indices.size() % 3 != 0)
    {
        return false;
    }

    for (const std::uint32_t index : indices)
    {
        if (index >= vertices.size())
        {
            return false;
        }
    }

    if (skinVertices.empty())
    {
        return
            skin.empty() &&
            localBounds.isValid();
    }

    if (skinVertices.size() != vertices.size() ||
        !skin.isValid())
    {
        return false;
    }

    for (const VertexSkinData& skinVertex :
        skinVertices)
    {
        if (!isSkinVertexValid(
                skinVertex,
                skin.jointNodeIndices.size()))
        {
            return false;
        }
    }

    return localBounds.isValid();
}

void MeshAsset::rebuildBounds() noexcept
{
    localBounds.reset();

    for (MeshPrimitiveAsset& primitive : primitives)
    {
        primitive.rebuildBounds();
        localBounds.expand(primitive.localBounds);
    }
}

bool MeshAsset::isValid() const noexcept
{
    if (primitives.empty())
    {
        return false;
    }

    if (!localBounds.isValid())
    {
        return false;
    }

    for (const MeshPrimitiveAsset& primitive : primitives)
    {
        if (!primitive.isValid())
        {
            return false;
        }
    }

    return true;
}

} // namespace stylized::asset
