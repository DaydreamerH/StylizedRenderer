#include <asset/importers/ModelImporter.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/SceneAsset.hpp>

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace stylized::asset::importers
{

namespace
{

[[nodiscard]]
std::string pathToUtf8(const std::filesystem::path& path)
{
    const std::u8string utf8Path = path.u8string();

    return {
        reinterpret_cast<const char*>(
            utf8Path.data()),
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
        std::cerr << "Cannot import an empty model path.\n";

        return {};
    }

    std::error_code filesystemError;

    const bool fileExists = std::filesystem::exists(
        path,
        filesystemError
    );

    if (filesystemError || !fileExists)
    {
        std::cerr
            << "Model file does not exist: "
            << path
            << '\n';

        return {};
    }

    const bool regularFile = std::filesystem::is_regular_file(
        path,
        filesystemError
    );

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
        importFlags
    );

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

    if ((importedScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0)
    {
        std::cerr
            << "Assimp returned an incomplete scene: "
            << path
            << '\n';

        return {};
    }

    if (importedScene->mRootNode == nullptr)
    {
        std::cerr
            << "Imported model does not contain "
               "a root node: "
            << path
            << '\n';

        return {};
    }

    std::size_t primitiveCount = 0;

    for (unsigned int meshIndex = 0;
         meshIndex < importedScene->mNumMeshes;
         ++meshIndex)
    {
        const aiMesh* mesh = importedScene->mMeshes[meshIndex];

        if (mesh != nullptr)
        {
            primitiveCount += mesh->mNumFaces;
        }
    }

    std::cout
        << "Model parsed successfully: "
        << path
        << '\n'
        << "Meshes:     "
        << importedScene->mNumMeshes
        << '\n'
        << "Materials:  "
        << importedScene->mNumMaterials
        << '\n'
        << "Textures:   "
        << importedScene->mNumTextures
        << '\n'
        << "Animations: "
        << importedScene->mNumAnimations
        << '\n'
        << "Faces:      "
        << primitiveCount
        << '\n';

    (void)registry_;

    return {};
}

} // namespace stylized::asset::importers