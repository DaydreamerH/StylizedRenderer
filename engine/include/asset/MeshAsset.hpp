#pragma once

#include <asset/AssetHandle.hpp>
#include <math/Bounds.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <glm/ext/vector_uint4.hpp>
#include <glm/mat4x4.hpp>
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

struct VertexSkinData
{
    glm::uvec4 joints{0U};
    glm::vec4 weights{0.0F};
};

struct SkinAsset
{
    std::vector<std::uint32_t> jointNodeIndices;
    std::vector<glm::mat4> inverseBindMatrices;

    std::vector<math::Bounds> jointLocalBounds;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
};

struct MeshPrimitiveAsset
{
    std::vector<StaticMeshVertex> vertices;
    std::vector<std::uint32_t> indices;

    std::vector<VertexSkinData> skinVertices;
    SkinAsset skin;

    AssetHandle<MaterialAsset> material;

    math::Bounds localBounds;

    void rebuildBounds() noexcept;

    [[nodiscard]] bool hasSkin() const noexcept
    {
        return !skinVertices.empty();
    }

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
