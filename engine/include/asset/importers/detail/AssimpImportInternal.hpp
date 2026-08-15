#pragma once

#include <asset/MaterialAsset.hpp>
#include <asset/MeshAsset.hpp>
#include <asset/SceneAsset.hpp>
#include <asset/TextureAsset.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct aiMaterial;
struct aiMesh;
struct aiNode;
struct aiScene;
struct aiTexture;

namespace stylized::asset::importers::detail
{

struct StagedMaterial
{
    MaterialAsset asset;

    std::optional<std::size_t>
        baseColorTextureIndex;

    std::optional<std::size_t>
        normalTextureIndex;
};

struct StagedMesh
{
    MeshAsset asset;
    std::vector<unsigned int> materialIndices;
};

enum class SceneNodeLookupResult : std::uint8_t
{
    Found,
    Missing,
    Ambiguous
};

struct StagedScene
{
    SceneAsset asset;
    std::vector<std::optional<std::size_t>> meshIndices;

    std::unordered_map<
        std::string,
        std::vector<std::uint32_t>>
        nodeIndicesByName;
};

[[nodiscard]] SceneNodeLookupResult findSceneNodeIndex(
    const StagedScene& scene,
    const std::string& nodeName,
    std::uint32_t& nodeIndex) noexcept;

[[nodiscard]] bool decodeEmbeddedTexture(
    const aiTexture& sourceTexture,
    const std::filesystem::path& modelPath,
    TextureAsset& textureAsset);

[[nodiscard]] bool decodeExternalTexture(
    const std::filesystem::path& texturePath,
    TextureAsset& textureAsset);

[[nodiscard]] bool stageMaterials(
    const aiScene& importedScene,
    const std::filesystem::path& modelPath,
    std::vector<TextureAsset>& textures,
    std::vector<StagedMaterial>& materials);

[[nodiscard]] bool buildMeshPrimitive(
    const aiMesh& sourceMesh,
    MeshPrimitiveAsset& primitiveAsset);

[[nodiscard]] bool stageAnimations(
    const aiScene& importedScene,
    StagedScene& scene);

[[nodiscard]] bool stageScene(
    const aiScene& importedScene,
    std::vector<StagedMesh>& meshes,
    StagedScene& scene);

} // namespace stylized::asset::importers::detail
