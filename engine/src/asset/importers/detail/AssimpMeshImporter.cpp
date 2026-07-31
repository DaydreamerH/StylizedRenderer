#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <cstddef>
#include <cstdint>

#include <assimp/mesh.h>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace stylized::asset::importers::detail
{

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
