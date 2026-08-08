#pragma once

#include <asset/AssetHandle.hpp>
#include <core/NonCopyable.hpp>
#include <graphics/resources/Buffer.hpp>
#include <graphics/device/GraphicsTypes.hpp>
#include <graphics/resources/VertexArray.hpp>
#include <math/Bounds.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace stylized::asset
{

struct MaterialAsset;
struct MeshAsset;
struct MeshPrimitiveAsset;

} // namespace stylized::asset

namespace stylized::graphics
{

class GraphicsDevice;

} // namespace stylized::graphics


namespace stylized::render
{

class RuntimeMeshPrimitive final : public core::NonCopyable
{
public:
    RuntimeMeshPrimitive() = default;
    RuntimeMeshPrimitive(RuntimeMeshPrimitive&& other) noexcept = default;
    RuntimeMeshPrimitive& operator=(RuntimeMeshPrimitive&& other) noexcept = default;

    ~RuntimeMeshPrimitive() = default;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] const graphics::VertexArray& vertexArray() const noexcept;
    [[nodiscard]] std::uint32_t indexCount() const noexcept;
    [[nodiscard]] graphics::IndexType indexType() const noexcept;
    [[nodiscard]] asset::AssetHandle<asset::MaterialAsset> material() const noexcept;
    [[nodiscard]] const math::Bounds& localBounds() const noexcept;

private:
    friend class RuntimeMesh;

    graphics::Buffer vertexBuffer_;
    graphics::Buffer indexBuffer_;
    graphics::VertexArray vertexArray_;

    std::uint32_t indexCount_ = 0;

    graphics::IndexType indexType_ = graphics::IndexType::Uint32;

    asset::AssetHandle<asset::MaterialAsset> material_;

    math::Bounds localBounds_;
};

class RuntimeMesh final : public core::NonCopyable
{
public:
    RuntimeMesh() = default;
    RuntimeMesh(RuntimeMesh&& other) noexcept = default;
    RuntimeMesh& operator=(RuntimeMesh&& other) noexcept = default;
    ~RuntimeMesh() = default;

    [[nodiscard]] static RuntimeMesh create(graphics::GraphicsDevice& graphicsDevice, const asset::MeshAsset& meshAsset);
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] const std::vector<RuntimeMeshPrimitive>& primitives() const noexcept;
    [[nodiscard]] const math::Bounds& localBounds() const noexcept;

private:
    std::string name_;
    std::vector<RuntimeMeshPrimitive> primitives_;
    math::Bounds localBounds_;

    [[nodiscard]] static bool uploadPrimitive(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::MeshPrimitiveAsset& source,
        const std::string& meshName,
        std::size_t primitiveIndex,
        RuntimeMeshPrimitive& destination);
};

} // namespace stylized::render
