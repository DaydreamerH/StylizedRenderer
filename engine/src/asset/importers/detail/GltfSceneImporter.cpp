#include <asset/importers/detail/GltfSceneImporter.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include <fastgltf/core.hpp>

#include <glm/gtc/quaternion.hpp>

namespace stylized::asset::importers::detail
{

namespace
{

[[nodiscard]]
bool appendNode(
    const fastgltf::Asset& gltfAsset,
    const std::size_t gltfNodeIndex,
    const std::uint32_t parentIndex,
    SceneAsset& sceneAsset,
    std::vector<std::uint8_t>& nodeStates)
{
    if (gltfNodeIndex >= gltfAsset.nodes.size())
    {
        return false;
    }

    // A non-zero state means that this node has already
    // appeared in the selected scene hierarchy.
    if (nodeStates[gltfNodeIndex] != 0)
    {
        return false;
    }

    if (sceneAsset.nodes.size() >=
        SceneNodeAsset::invalidNodeIndex)
    {
        return false;
    }

    // 1 means that the node is on the current recursion path.
    nodeStates[gltfNodeIndex] = 1;

    const fastgltf::Node& sourceNode =
        gltfAsset.nodes[gltfNodeIndex];

    const fastgltf::TRS* sourceTransform =
        std::get_if<fastgltf::TRS>(
            &sourceNode.transform);

    if (sourceTransform == nullptr)
    {
        return false;
    }

    SceneNodeAsset node;

    node.name.assign(
        sourceNode.name.begin(),
        sourceNode.name.end());

    node.parentIndex = parentIndex;

    node.localTransform.setTranslation({
        sourceTransform->translation.x(),
        sourceTransform->translation.y(),
        sourceTransform->translation.z()});

    const glm::quat rotation{
        sourceTransform->rotation.w(),
        sourceTransform->rotation.x(),
        sourceTransform->rotation.y(),
        sourceTransform->rotation.z()};

    if (!node.localTransform.setRotation(rotation))
    {
        return false;
    }

    node.localTransform.setScale({
        sourceTransform->scale.x(),
        sourceTransform->scale.y(),
        sourceTransform->scale.z()});

    const auto nodeIndex =
        static_cast<std::uint32_t>(
            sceneAsset.nodes.size());

    sceneAsset.nodes.push_back(std::move(node));

    for (const std::size_t childIndex :
         sourceNode.children)
    {
        if (!appendNode(
                gltfAsset,
                childIndex,
                nodeIndex,
                sceneAsset,
                nodeStates))
        {
            return false;
        }
    }

    // 2 means that this node and its children are complete.
    nodeStates[gltfNodeIndex] = 2;

    return true;
}

} // namespace

bool buildSceneAsset(
    const fastgltf::Asset& gltfAsset,
    const std::size_t sceneIndex,
    SceneAsset& sceneAsset)
{
    if (sceneIndex >= gltfAsset.scenes.size())
    {
        return false;
    }

    const fastgltf::Scene& sourceScene =
        gltfAsset.scenes[sceneIndex];

    sceneAsset.name.assign(
        sourceScene.name.begin(),
        sourceScene.name.end());

    sceneAsset.nodes.clear();

    std::vector<std::uint8_t> nodeStates(
        gltfAsset.nodes.size(),
        0);

    for (const std::size_t rootNodeIndex :
         sourceScene.nodeIndices)
    {
        if (!appendNode(
                gltfAsset,
                rootNodeIndex,
                SceneNodeAsset::invalidNodeIndex,
                sceneAsset,
                nodeStates))
        {
            return false;
        }
    }

    return sceneAsset.isValid();
}

} // namespace stylized::asset::importers::detail
