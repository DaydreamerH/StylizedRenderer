#include <asset/importers/TextureImporter.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <utility>

namespace stylized::asset::importers
{

TextureImporter::TextureImporter(
    AssetRegistry& registry) noexcept
    : registry_(registry)
{
}

AssetHandle<TextureAsset> TextureImporter::import(
    const std::filesystem::path& path,
    const ColorSpace colorSpace,
    const bool generateMipmaps)
{
    if (path.empty())
    {
        std::cerr
            << "Cannot import an empty texture path.\n";

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
            << "Texture file does not exist: "
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
            << "Texture path is not a regular file: "
            << path
            << '\n';

        return {};
    }

    TextureAsset texture;

    if (!detail::decodeExternalTexture(
            path,
            texture))
    {
        std::cerr
            << "Failed to decode texture: "
            << path
            << '\n';

        return {};
    }

    texture.colorSpace = colorSpace;
    texture.generateMipmaps = generateMipmaps;

    try
    {
        const AssetHandle<TextureAsset> handle =
            registry_.emplace<TextureAsset>(
                std::move(texture));

        if (handle.isNull())
        {
            std::cerr
                << "Failed to register texture: "
                << path
                << '\n';
        }

        return handle;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Texture import failed: "
            << path
            << "\nReason: "
            << exception.what()
            << '\n';

        return {};
    }
}

} // namespace stylized::asset::importers
