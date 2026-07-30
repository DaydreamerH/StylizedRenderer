#include <asset/MeshAsset.hpp>

namespace stylized::asset
{

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