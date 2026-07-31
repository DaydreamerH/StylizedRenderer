#pragma once

#include <asset/AssetHandle.hpp>

#include <filesystem>

namespace stylized::asset
{
class AssetRegistry;
struct SceneAsset;

namespace importers
{

class GltfImporter
{
public:
    explicit GltfImporter(AssetRegistry& registry) noexcept;

    [[nodiscard]] AssetHandle<SceneAsset> import(const std::filesystem::path& path);

private:
    AssetRegistry& registry_;
};

} // namespace importers


} // namespace stylized::asset
