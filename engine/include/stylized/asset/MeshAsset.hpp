#pragma once

#include <stylized/asset/AssetHandle.hpp>
#include <stylized/math/Bounds.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace stylized::asset
{
struct MaterialAsset;

struct StaticMeshVertex
{
    glm::vec3 position{0.F};
    glm::vec3 normal{0.F, 1.F, 0.F};
    glm::vec4 tangent{1.F, 0.F, 0.F, 1.F};
    glm::vec2 texCoord0{0.F};
};

struct MeshPrimitiveAsset
{
    std::vector<StaticMeshVertex> vertices;
    std::vector<std::uint32_t> indices;

    AssetHandle<MaterialAsset> material;

    math::Bounds localBounds;

    void rebuildBounds() noexcept;

    [[nodiscard]] bool isValid() const noexcept;
};

struct MeshAsset
{
    std::string name;
    std::vector<MeshPrimitiveAsset> primitives;

    math::Bounds localBounds;

    void rebuildBounds() noexcept;

    [[nodiscard]] bool isValid() const noexcept;
};

} // namespace stylized::asset
