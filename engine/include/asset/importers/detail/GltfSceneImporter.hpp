#pragma once

#include <asset/SceneAsset.hpp>

#include <cstddef>

namespace fastgltf
{

class Asset;

} // namespace fastgltf


namespace stylized::asset::importers::detail
{

[[nodiscard]]
bool buildSceneAsset(
    const fastgltf::Asset& gltfAsset,
    std::size_t sceneIndex,
    SceneAsset& sceneAsset);

} // namespace stylized::asset::importers::detail
