#pragma once

#include <asset/AssetHandle.hpp>
#include <asset/TextureAsset.hpp>

#include <filesystem>

namespace stylized::asset
{

class AssetRegistry;

namespace importers
{

class TextureImporter
{
public:
    explicit TextureImporter(
        AssetRegistry& registry) noexcept;

    [[nodiscard]]
    AssetHandle<TextureAsset> import(
        const std::filesystem::path& path,
        ColorSpace colorSpace);

private:
    AssetRegistry& registry_;
};

} // namespace importers

} // namespace stylized::asset
