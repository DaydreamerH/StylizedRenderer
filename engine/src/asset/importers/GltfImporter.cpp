#include <asset/importers/GltfImporter.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/SceneAsset.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

#include <fastgltf/core.hpp>

namespace stylized::asset::importers
{

namespace
{
    
[[nodiscard]]
std::string lowercaseExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();

    std::transform(extension.begin(), extension.end(), extension.begin(), 
        [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
    );

    return extension;
}

} // namespace

GltfImporter::GltfImporter(AssetRegistry& registry) noexcept
    : registry_(registry)    
{
}

AssetHandle<SceneAsset> GltfImporter::import(const std::filesystem::path& path)
{
    if (path.empty())
    {
        std::cerr << "Cannot import an empty glTF path.\n";
        return {};
    }

    std::error_code filesystemError;

    const bool fileExists =
        std::filesystem::exists(
            path,
            filesystemError);

    if (filesystemError || !fileExists)
    {
        std::cerr
            << "glTF file does not exist: "
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
            << "glTF path is not a regular file: "
            << path
            << '\n';

        return {};
    }

    const std::string extension = lowercaseExtension(path);

    if (extension != ".gltf" && extension != ".glb")
    {
        std::cerr
            << "Unsupported glTF extension: "
            << path
            << '\n';

        return {};
    }

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);

    if (gltfFile.error() !=
        fastgltf::Error::None)
    {
        std::cerr
            << "Failed to open glTF file: "
            << path
            << "\nReason: "
            << fastgltf::getErrorMessage(
                   gltfFile.error())
            << '\n';

        return {};
    }

    fastgltf::Parser parser;

    constexpr fastgltf::Options options =
        fastgltf::Options::AllowDouble |
        fastgltf::Options::LoadExternalBuffers |
        fastgltf::Options::LoadExternalImages |
        fastgltf::Options::GenerateMeshIndices |
        fastgltf::Options::DecomposeNodeMatrices;

    auto loadedAsset =
        parser.loadGltf(
            gltfFile.get(),
            path.parent_path(),
            options);

    if (loadedAsset.error() != fastgltf::Error::None)
    {
        std::cerr
            << "Failed to parse glTF file: "
            << path
            << "\nReason: "
            << fastgltf::getErrorMessage(
                   loadedAsset.error())
            << '\n';

        return {};
    }

    fastgltf::Asset gltfAsset = std::move(loadedAsset.get());

    const fastgltf::Error validationError = fastgltf::validate(gltfAsset);

    if (validationError !=
        fastgltf::Error::None)
    {
        std::cerr
            << "glTF validation failed: "
            << path
            << "\nReason: "
            << fastgltf::getErrorMessage(
                   validationError)
            << '\n';

        return {};
    }

    std::cout
        << "Parsed glTF file successfully: "
        << path
        << '\n'
        << "Scenes:    "
        << gltfAsset.scenes.size()
        << '\n'
        << "Nodes:     "
        << gltfAsset.nodes.size()
        << '\n'
        << "Meshes:    "
        << gltfAsset.meshes.size()
        << '\n'
        << "Materials: "
        << gltfAsset.materials.size()
        << '\n'
        << "Images:    "
        << gltfAsset.images.size()
        << '\n';

    (void)registry_;

    return{};
}

} // namespace stylized::asset::importers
