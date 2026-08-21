#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include <assimp/matrix4x4.h>
#include <assimp/quaternion.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>

#include <glm/gtc/quaternion.hpp>

namespace stylized::asset::importers::detail
{

namespace
{

[[nodiscard]] bool isFinite(
    const aiVector3D& value) noexcept
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

[[nodiscard]] bool isFinite(
    const aiQuaternion& value) noexcept
{
    return
        std::isfinite(value.w) &&
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

[[nodiscard]] bool findOrBuildMesh(
    const aiScene& importedScene,
    const aiNode& sourceNode,
    std::map<std::vector<unsigned int>, std::size_t>&
        meshLookup,
    std::vector<StagedMesh>& meshes,
    std::optional<std::size_t>& meshIndex)
{
    if (sourceNode.mNumMeshes == 0)
    {
        meshIndex.reset();
        return true;
    }

    if (sourceNode.mMeshes == nullptr)
    {
        return false;
    }

    const std::vector<unsigned int> meshKey{
        sourceNode.mMeshes,
        sourceNode.mMeshes + sourceNode.mNumMeshes};

    const auto existing = meshLookup.find(meshKey);

    if (existing != meshLookup.end())
    {
        meshIndex = existing->second;
        return true;
    }

    StagedMesh staged;
    staged.asset.name = sourceNode.mName.C_Str();
    staged.asset.primitives.reserve(
        sourceNode.mNumMeshes);
    staged.materialIndices.reserve(
        sourceNode.mNumMeshes);
    staged.sourceMeshIndices.reserve(
        sourceNode.mNumMeshes);

    for (const unsigned int sourceMeshIndex : meshKey)
    {
        if (sourceMeshIndex >= importedScene.mNumMeshes)
        {
            return false;
        }

        const aiMesh* sourceMesh =
            importedScene.mMeshes[sourceMeshIndex];

        if (sourceMesh == nullptr ||
            sourceMesh->mMaterialIndex >=
                importedScene.mNumMaterials)
        {
            return false;
        }

        MeshPrimitiveAsset primitive;

        if (!buildMeshPrimitive(
                *sourceMesh,
                primitive))
        {
            return false;
        }

        staged.asset.primitives.push_back(
            std::move(primitive));
        staged.materialIndices.push_back(
            sourceMesh->mMaterialIndex);
        staged.sourceMeshIndices.push_back(
            sourceMeshIndex);
    }

    if (staged.materialIndices.size() !=
            staged.asset.primitives.size() ||
        staged.sourceMeshIndices.size() !=
            staged.asset.primitives.size())
    {
        return false;
    }

    staged.asset.rebuildBounds();

    if (!staged.asset.isValid())
    {
        return false;
    }

    const std::size_t newMeshIndex = meshes.size();
    meshes.push_back(std::move(staged));
    meshLookup.emplace(meshKey, newMeshIndex);
    meshIndex = newMeshIndex;

    return true;
}

[[nodiscard]] bool appendNode(
    const aiScene& importedScene,
    const aiNode& sourceNode,
    const std::uint32_t parentIndex,
    std::map<std::vector<unsigned int>, std::size_t>&
        meshLookup,
    std::unordered_set<const aiNode*>& visitedNodes,
    std::vector<StagedMesh>& meshes,
    StagedScene& scene)
{
    if (!visitedNodes.emplace(&sourceNode).second ||
        scene.asset.nodes.size() >=
            SceneNodeAsset::invalidNodeIndex)
    {
        return false;
    }

    aiVector3D scaling;
    aiQuaternion rotation;
    aiVector3D translation;

    sourceNode.mTransformation.Decompose(
        scaling,
        rotation,
        translation);

    if (!isFinite(translation) ||
        !isFinite(rotation) ||
        !isFinite(scaling))
    {
        return false;
    }

    SceneNodeAsset node;
    node.name = sourceNode.mName.C_Str();
    node.parentIndex = parentIndex;
    node.localTransform.setTranslation({
        translation.x,
        translation.y,
        translation.z});

    if (!node.localTransform.setRotation(glm::quat{
            rotation.w,
            rotation.x,
            rotation.y,
            rotation.z}))
    {
        return false;
    }

    node.localTransform.setScale({
        scaling.x,
        scaling.y,
        scaling.z});

    std::optional<std::size_t> stagedMeshIndex;

    if (!findOrBuildMesh(
            importedScene,
            sourceNode,
            meshLookup,
            meshes,
            stagedMeshIndex))
    {
        return false;
    }

    const auto nodeIndex =
        static_cast<std::uint32_t>(
            scene.asset.nodes.size());

    scene.asset.nodes.push_back(std::move(node));
    scene.meshIndices.push_back(stagedMeshIndex);

    scene.nodeIndicesByName[
        scene.asset.nodes.back().name
    ].push_back(nodeIndex);

    for (unsigned int childIndex = 0;
         childIndex < sourceNode.mNumChildren;
         ++childIndex)
    {
        const aiNode* child =
            sourceNode.mChildren[childIndex];

        if (child == nullptr ||
            !appendNode(
                importedScene,
                *child,
                nodeIndex,
                meshLookup,
                visitedNodes,
                meshes,
                scene))
        {
            return false;
        }
    }

    return true;
}

} // namespace

SceneNodeLookupResult findSceneNodeIndex(
    const StagedScene& scene,
    const std::string& nodeName,
    std::uint32_t& nodeIndex) noexcept
{
    nodeIndex =
        SceneNodeAsset::invalidNodeIndex;

    if (nodeName.empty())
    {
        return SceneNodeLookupResult::Missing;
    }

    const auto iterator =
        scene.nodeIndicesByName.find(nodeName);

    if (iterator ==
        scene.nodeIndicesByName.end())
    {
        return SceneNodeLookupResult::Missing;
    }

    const std::vector<std::uint32_t>& matches =
        iterator->second;

    if (matches.size() != 1)
    {
        return SceneNodeLookupResult::Ambiguous;
    }

    nodeIndex = matches.front();

    return SceneNodeLookupResult::Found;
}

bool stageScene(
    const aiScene& importedScene,
    std::vector<StagedMesh>& meshes,
    StagedScene& scene)
{
    if (importedScene.mRootNode == nullptr)
    {
        return false;
    }

    meshes.clear();
    scene = {};

    scene.asset.name =
        importedScene.mName.length > 0
            ? importedScene.mName.C_Str()
            : importedScene.mRootNode->mName.C_Str();

    std::map<std::vector<unsigned int>, std::size_t>
        meshLookup;
    std::unordered_set<const aiNode*> visitedNodes;

    if (!appendNode(
            importedScene,
            *importedScene.mRootNode,
            SceneNodeAsset::invalidNodeIndex,
            meshLookup,
            visitedNodes,
            meshes,
            scene))
    {
        return false;
    }

    return
        scene.meshIndices.size() ==
            scene.asset.nodes.size() &&
        scene.asset.isValid();
}

} // namespace stylized::asset::importers::detail
