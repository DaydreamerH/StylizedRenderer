#pragma once

#include <asset/AssetHandle.hpp>
#include <math/Bounds.hpp>

#include <cstdint>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace stylized::asset
{

struct MaterialAsset;

} // namespace stylized::asset

namespace stylized::render
{

class RuntimeMeshPrimitive;

struct RenderView
{
    glm::mat4 view{1.F};
    glm::mat4 projection{1.F};
    glm::mat4 viewProjection{1.F};
    glm::vec3 cameraPosition{0.F};
};

struct RenderItem
{
    const RuntimeMeshPrimitive* primitive = nullptr;

    asset::AssetHandle<asset::MaterialAsset> material;

    glm::mat4 world{1.0F};
    glm::mat3 normalMatrix{1.0F};

    math::Bounds worldBounds;

    std::uint32_t objectId = 0;
};

struct RenderWorld
{
    RenderView mainView;
    std::vector<RenderItem> items;

    void clear() noexcept
    {
        items.clear();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return items.empty();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return items.size();
    }

};

} // namespace stylized::render
