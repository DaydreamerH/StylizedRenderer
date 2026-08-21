#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <assimp/mesh.h>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace stylized::asset::importers::detail
{

namespace
{

[[nodiscard]] glm::vec3 toGlm(
    const aiVector3D& value) noexcept
{
    return {
        value.x,
        value.y,
        value.z};
}

[[nodiscard]] bool buildMorphTargets(
    const aiMesh& sourceMesh,
    MeshPrimitiveAsset& primitiveAsset)
{
    if (sourceMesh.mNumAnimMeshes == 0)
    {
        return true;
    }

    if (sourceMesh.mAnimMeshes == nullptr)
    {
        return false;
    }

    primitiveAsset.morphTargets.reserve(
        sourceMesh.mNumAnimMeshes);

    for (unsigned int morphIndex = 0;
         morphIndex < sourceMesh.mNumAnimMeshes;
         ++morphIndex)
    {
        const aiAnimMesh* sourceMorph =
            sourceMesh.mAnimMeshes[morphIndex];

        if (sourceMorph == nullptr ||
            sourceMorph->mNumVertices !=
                sourceMesh.mNumVertices)
        {
            return false;
        }

        MorphTargetAsset morphTarget;

        morphTarget.name =
            sourceMorph->mName.length > 0
                ? sourceMorph->mName.C_Str()
                : "Morph_" +
                    std::to_string(morphIndex);

        morphTarget.positionDeltas.resize(
            sourceMesh.mNumVertices,
            glm::vec3{0.0F});

        if (sourceMorph->HasPositions())
        {
            for (unsigned int vertexIndex = 0;
                 vertexIndex < sourceMesh.mNumVertices;
                 ++vertexIndex)
            {
                morphTarget.positionDeltas[vertexIndex] =
                    toGlm(
                        sourceMorph->mVertices[vertexIndex]) -
                    toGlm(
                        sourceMesh.mVertices[vertexIndex]);
            }
        }

        if (sourceMorph->HasNormals())
        {
            if (!sourceMesh.HasNormals())
            {
                return false;
            }

            morphTarget.normalDeltas.resize(
                sourceMesh.mNumVertices);

            for (unsigned int vertexIndex = 0;
                 vertexIndex < sourceMesh.mNumVertices;
                 ++vertexIndex)
            {
                morphTarget.normalDeltas[vertexIndex] =
                    toGlm(
                        sourceMorph->mNormals[vertexIndex]) -
                    toGlm(
                        sourceMesh.mNormals[vertexIndex]);
            }
        }

        if (sourceMorph->HasTangentsAndBitangents())
        {
            if (!sourceMesh.HasTangentsAndBitangents())
            {
                return false;
            }

            morphTarget.tangentDeltas.resize(
                sourceMesh.mNumVertices);

            for (unsigned int vertexIndex = 0;
                 vertexIndex < sourceMesh.mNumVertices;
                 ++vertexIndex)
            {
                morphTarget.tangentDeltas[vertexIndex] =
                    toGlm(
                        sourceMorph->mTangents[vertexIndex]) -
                    toGlm(
                        sourceMesh.mTangents[vertexIndex]);
            }
        }

        if (!morphTarget.isValid(
                primitiveAsset.vertices.size()))
        {
            return false;
        }

        primitiveAsset.morphTargets.push_back(
            std::move(morphTarget));
    }

    return true;
}

} // namespace

bool buildMeshPrimitive(
    const aiMesh& sourceMesh,
    MeshPrimitiveAsset& primitiveAsset)
{
    if (!sourceMesh.HasPositions() ||
        sourceMesh.mNumVertices == 0 ||
        sourceMesh.mNumFaces == 0 ||
        (sourceMesh.mPrimitiveTypes &
         aiPrimitiveType_TRIANGLE) == 0)
    {
        return false;
    }

    primitiveAsset = {};
    primitiveAsset.vertices.resize(
        sourceMesh.mNumVertices);

    const bool hasNormals = sourceMesh.HasNormals();
    const bool hasTangents =
        sourceMesh.HasTangentsAndBitangents();
    const bool hasTexCoords =
        sourceMesh.HasTextureCoords(0);

    for (unsigned int vertexIndex = 0;
         vertexIndex < sourceMesh.mNumVertices;
         ++vertexIndex)
    {
        StaticMeshVertex& destination =
            primitiveAsset.vertices[vertexIndex];

        const aiVector3D& position =
            sourceMesh.mVertices[vertexIndex];

        destination.position = {
            position.x,
            position.y,
            position.z};

        if (hasNormals)
        {
            const aiVector3D& normal =
                sourceMesh.mNormals[vertexIndex];

            destination.normal = {
                normal.x,
                normal.y,
                normal.z};
        }

        if (hasTexCoords)
        {
            const aiVector3D& texCoord =
                sourceMesh.mTextureCoords[0][vertexIndex];

            destination.texCoord0 = {
                texCoord.x,
                texCoord.y};
        }

        if (hasTangents)
        {
            const aiVector3D& sourceTangent =
                sourceMesh.mTangents[vertexIndex];
            const aiVector3D& sourceBitangent =
                sourceMesh.mBitangents[vertexIndex];

            const glm::vec3 tangent{
                sourceTangent.x,
                sourceTangent.y,
                sourceTangent.z};
            const glm::vec3 bitangent{
                sourceBitangent.x,
                sourceBitangent.y,
                sourceBitangent.z};

            const float handedness =
                glm::dot(
                    glm::cross(destination.normal, tangent),
                    bitangent) < 0.0F
                    ? -1.0F
                    : 1.0F;

            destination.tangent = {
                tangent.x,
                tangent.y,
                tangent.z,
                handedness};
        }
    }

    if (!buildMorphTargets(
            sourceMesh,
            primitiveAsset))
    {
        return false;
    }

    primitiveAsset.indices.reserve(
        static_cast<std::size_t>(
            sourceMesh.mNumFaces) *
        3);

    for (unsigned int faceIndex = 0;
         faceIndex < sourceMesh.mNumFaces;
         ++faceIndex)
    {
        const aiFace& face =
            sourceMesh.mFaces[faceIndex];

        if (face.mNumIndices != 3 ||
            face.mIndices == nullptr)
        {
            return false;
        }

        for (unsigned int index = 0;
             index < face.mNumIndices;
             ++index)
        {
            primitiveAsset.indices.push_back(
                static_cast<std::uint32_t>(
                    face.mIndices[index]));
        }
    }

    primitiveAsset.rebuildBounds();
    return primitiveAsset.isValid();
}

} // namespace stylized::asset::importers::detail
