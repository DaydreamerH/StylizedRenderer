#include <asset/importers/ModelImporter.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/MaterialAsset.hpp>
#include <asset/MeshAsset.hpp>
#include <asset/SceneAsset.hpp>
#include <asset/TextureAsset.hpp>
#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace stylized::asset::importers
{

namespace
{

[[nodiscard]] std::string pathToUtf8(
    const std::filesystem::path& path)
{
    const std::u8string utf8Path = path.u8string();

    return {
        reinterpret_cast<const char*>(utf8Path.data()),
        utf8Path.size()};
}

} // namespace

ModelImporter::ModelImporter(
    AssetRegistry& registry) noexcept
    : registry_(registry)
{
}

AssetHandle<SceneAsset> ModelImporter::import(
    const std::filesystem::path& path)
{
    if (path.empty())
    {
        std::cerr
            << "Cannot import an empty model path.\n";
        return {};
    }

    std::error_code filesystemError;

    const bool fileExists = std::filesystem::exists(
        path,
        filesystemError);

    if (filesystemError || !fileExists)
    {
        std::cerr
            << "Model file does not exist: "
            << path
            << '\n';
        return {};
    }

    const bool regularFile =
        std::filesystem::is_regular_file(
            path,
            filesystemError);

    if (filesystemError || !regularFile)
    {
        std::cerr
            << "Model path is not a regular file: "
            << path
            << '\n';
        return {};
    }

    constexpr unsigned int importFlags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_ImproveCacheLocality |
        aiProcess_SortByPType |
        aiProcess_ValidateDataStructure;

    Assimp::Importer importer;

    const std::string modelPath = pathToUtf8(path);

    const aiScene* importedScene = importer.ReadFile(
        modelPath,
        importFlags);

    if (importedScene == nullptr)
    {
        std::cerr
            << "Assimp failed to import model: "
            << path
            << "\nReason: "
            << importer.GetErrorString()
            << '\n';
        return {};
    }

    if ((importedScene->mFlags &
         AI_SCENE_FLAGS_INCOMPLETE) != 0 ||
        importedScene->mRootNode == nullptr)
    {
        std::cerr
            << "Assimp returned an incomplete scene: "
            << path
            << '\n';
        return {};
    }

    std::vector<TextureAsset> stagedTextures;
    std::vector<detail::StagedMaterial> stagedMaterials;
    std::vector<detail::StagedMesh> stagedMeshes;
    detail::StagedScene stagedScene;

    if (!detail::stageMaterials(
            *importedScene,
            path,
            stagedTextures,
            stagedMaterials))
    {
        std::cerr
            << "Failed to convert model materials: "
            << path
            << '\n';
        return {};
    }

    if (!detail::stageScene(
            *importedScene,
            stagedMeshes,
            stagedScene))
    {
        std::cerr
            << "Failed to convert model scene or meshes: "
            << path
            << '\n';
        return {};
    }

    std::vector<AssetHandle<TextureAsset>>
        textureHandles;
    std::vector<AssetHandle<MaterialAsset>>
        materialHandles;
    std::vector<AssetHandle<MeshAsset>> meshHandles;

    textureHandles.reserve(stagedTextures.size());
    materialHandles.reserve(stagedMaterials.size());
    meshHandles.reserve(stagedMeshes.size());

    const auto rollback = [&]()
    {
        for (auto iterator = meshHandles.rbegin();
             iterator != meshHandles.rend();
             ++iterator)
        {
            registry_.remove(*iterator);
        }

        for (auto iterator = materialHandles.rbegin();
             iterator != materialHandles.rend();
             ++iterator)
        {
            registry_.remove(*iterator);
        }

        for (auto iterator = textureHandles.rbegin();
             iterator != textureHandles.rend();
             ++iterator)
        {
            registry_.remove(*iterator);
        }
    };

    try
    {
        for (TextureAsset& texture : stagedTextures)
        {
            const AssetHandle<TextureAsset> handle =
                registry_.emplace<TextureAsset>(
                    std::move(texture));

            if (handle.isNull())
            {
                rollback();
                return {};
            }

            textureHandles.push_back(handle);
        }

        for (detail::StagedMaterial& material :
             stagedMaterials)
        {
            if (material.textureIndex.has_value())
            {
                if (*material.textureIndex >=
                    textureHandles.size())
                {
                    rollback();
                    return {};
                }

                material.asset.baseColorTexture =
                    textureHandles[*material.textureIndex];
            }

            const AssetHandle<MaterialAsset> handle =
                registry_.emplace<MaterialAsset>(
                    std::move(material.asset));

            if (handle.isNull())
            {
                rollback();
                return {};
            }

            materialHandles.push_back(handle);
        }

        for (detail::StagedMesh& mesh : stagedMeshes)
        {
            if (mesh.materialIndices.size() !=
                mesh.asset.primitives.size())
            {
                rollback();
                return {};
            }

            for (std::size_t primitiveIndex = 0;
                 primitiveIndex <
                     mesh.asset.primitives.size();
                 ++primitiveIndex)
            {
                const unsigned int materialIndex =
                    mesh.materialIndices[primitiveIndex];

                if (materialIndex >= materialHandles.size())
                {
                    rollback();
                    return {};
                }

                mesh.asset.primitives[primitiveIndex].material =
                    materialHandles[materialIndex];
            }

            const AssetHandle<MeshAsset> handle =
                registry_.emplace<MeshAsset>(
                    std::move(mesh.asset));

            if (handle.isNull())
            {
                rollback();
                return {};
            }

            meshHandles.push_back(handle);
        }

        for (std::size_t nodeIndex = 0;
             nodeIndex < stagedScene.asset.nodes.size();
             ++nodeIndex)
        {
            const auto& meshIndex =
                stagedScene.meshIndices[nodeIndex];

            if (!meshIndex.has_value())
            {
                continue;
            }

            if (*meshIndex >= meshHandles.size())
            {
                rollback();
                return {};
            }

            stagedScene.asset.nodes[nodeIndex].mesh =
                meshHandles[*meshIndex];
        }

        const AssetHandle<SceneAsset> sceneHandle =
            registry_.emplace<SceneAsset>(
                std::move(stagedScene.asset));

        if (sceneHandle.isNull())
        {
            rollback();
        }

        return sceneHandle;
    }
    catch (const std::exception& exception)
    {
        rollback();

        std::cerr
            << "Model import failed while registering assets: "
            << path
            << "\nReason: "
            << exception.what()
            << '\n';

        return {};
    }
}

} // namespace stylized::asset::importers
