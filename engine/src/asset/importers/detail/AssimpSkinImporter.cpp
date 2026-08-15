#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>

namespace stylized::asset::importers::detail
{

namespace
{

constexpr glm::length_t maximumInfluenceCount = 4;

[[nodiscard]] glm::mat4 convertMatrix(
    const aiMatrix4x4& matrix) noexcept
{
    return {
        matrix.a1,
        matrix.b1,
        matrix.c1,
        matrix.d1,

        matrix.a2,
        matrix.b2,
        matrix.c2,
        matrix.d2,

        matrix.a3,
        matrix.b3,
        matrix.c3,
        matrix.d3,

        matrix.a4,
        matrix.b4,
        matrix.c4,
        matrix.d4
    };
}

void addInfluence(
    VertexSkinData& vertex,
    const std::uint32_t jointIndex,
    const float weight,
    std::size_t& discardedInfluenceCount) noexcept
{
    for (glm::length_t component = 0;
         component < maximumInfluenceCount;
         ++component)
    {
        if (vertex.weights[component] > 0.0F &&
            vertex.joints[component] == jointIndex)
        {
            vertex.weights[component] += weight;
            return;
        }
    }

    glm::length_t weakestComponent = 0;

    for (glm::length_t component = 1;
         component < maximumInfluenceCount;
         ++component)
    {
        if (vertex.weights[component] <
            vertex.weights[weakestComponent])
        {
            weakestComponent = component;
        }
    }

    if (weight <=
        vertex.weights[weakestComponent])
    {
        ++discardedInfluenceCount;
        return;
    }

    if (vertex.weights[weakestComponent] > 0.0F)
    {
        ++discardedInfluenceCount;
    }

    vertex.joints[weakestComponent] =
        jointIndex;

    vertex.weights[weakestComponent] =
        weight;
}

[[nodiscard]] bool normalizeWeights(
    MeshPrimitiveAsset& primitive) noexcept
{
    for (VertexSkinData& vertex :
         primitive.skinVertices)
    {
        float totalWeight = 0.0F;

        for (glm::length_t component = 0;
             component < maximumInfluenceCount;
             ++component)
        {
            totalWeight +=
                vertex.weights[component];
        }

        if (!std::isfinite(totalWeight) ||
            totalWeight <= 0.0F)
        {
            return false;
        }

        vertex.weights /= totalWeight;
    }

    return true;
}

[[nodiscard]] bool stagePrimitiveSkin(
    const aiMesh& sourceMesh,
    const StagedScene& scene,
    MeshPrimitiveAsset& primitive,
    std::size_t& discardedInfluenceCount)
{
    if (sourceMesh.mNumBones == 0)
    {
        return true;
    }

    if (sourceMesh.mBones == nullptr ||
        primitive.vertices.size() !=
            sourceMesh.mNumVertices)
    {
        return false;
    }

    primitive.skin = {};
    primitive.skinVertices.assign(
        primitive.vertices.size(),
        VertexSkinData{});

    primitive.skin.jointNodeIndices.reserve(
        sourceMesh.mNumBones);

    primitive.skin.inverseBindMatrices.reserve(
        sourceMesh.mNumBones);

    for (unsigned int boneIndex = 0;
         boneIndex < sourceMesh.mNumBones;
         ++boneIndex)
    {
        const aiBone* sourceBone =
            sourceMesh.mBones[boneIndex];

        if (sourceBone == nullptr ||
            sourceBone->mNumWeights == 0 ||
            sourceBone->mWeights == nullptr)
        {
            return false;
        }

        std::uint32_t jointNodeIndex =
            SceneNodeAsset::invalidNodeIndex;

        if (findSceneNodeIndex(
                scene,
                sourceBone->mName.C_Str(),
                jointNodeIndex) !=
            SceneNodeLookupResult::Found)
        {
            std::cerr
                << "Cannot uniquely match skin joint: "
                << sourceBone->mName.C_Str()
                << '\n';

            return false;
        }

        if (std::find(
                primitive.skin.jointNodeIndices.begin(),
                primitive.skin.jointNodeIndices.end(),
                jointNodeIndex) !=
            primitive.skin.jointNodeIndices.end())
        {
            return false;
        }

        if (primitive.skin.jointNodeIndices.size() >=
            std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        const auto paletteIndex =
            static_cast<std::uint32_t>(
                primitive.skin.jointNodeIndices.size());

        primitive.skin.jointNodeIndices.push_back(
            jointNodeIndex);

        primitive.skin.inverseBindMatrices.push_back(
            convertMatrix(
                sourceBone->mOffsetMatrix));

        for (unsigned int weightIndex = 0;
             weightIndex < sourceBone->mNumWeights;
             ++weightIndex)
        {
            const aiVertexWeight& sourceWeight =
                sourceBone->mWeights[weightIndex];

            if (sourceWeight.mVertexId >=
                primitive.skinVertices.size())
            {
                return false;
            }

            const float weight =
                sourceWeight.mWeight;

            if (!std::isfinite(weight) ||
                weight <= 0.0F)
            {
                ++discardedInfluenceCount;
                continue;
            }

            addInfluence(
                primitive.skinVertices[
                    sourceWeight.mVertexId],
                paletteIndex,
                weight,
                discardedInfluenceCount);
        }
    }

    if (!normalizeWeights(primitive))
    {
        return false;
    }

    return primitive.isValid();
}

} // namespace

bool stageSkins(
    const aiScene& importedScene,
    std::vector<StagedMesh>& meshes,
    const StagedScene& scene)
{
    std::size_t discardedInfluenceCount = 0;

    for (StagedMesh& mesh : meshes)
    {
        if (mesh.sourceMeshIndices.size() !=
            mesh.asset.primitives.size())
        {
            return false;
        }

        for (std::size_t primitiveIndex = 0;
             primitiveIndex <
                 mesh.asset.primitives.size();
             ++primitiveIndex)
        {
            const unsigned int sourceMeshIndex =
                mesh.sourceMeshIndices[primitiveIndex];

            if (sourceMeshIndex >=
                    importedScene.mNumMeshes ||
                importedScene.mMeshes == nullptr)
            {
                return false;
            }

            const aiMesh* sourceMesh =
                importedScene.mMeshes[
                    sourceMeshIndex];

            if (sourceMesh == nullptr ||
                !stagePrimitiveSkin(
                    *sourceMesh,
                    scene,
                    mesh.asset.primitives[
                        primitiveIndex],
                    discardedInfluenceCount))
            {
                return false;
            }
        }

        if (!mesh.asset.isValid())
        {
            return false;
        }
    }

    if (discardedInfluenceCount > 0)
    {
        std::clog
            << "Discarded "
            << discardedInfluenceCount
            << " invalid or excess skin influences.\n";
    }

    return true;
}

} // namespace stylized::asset::importers::detail
