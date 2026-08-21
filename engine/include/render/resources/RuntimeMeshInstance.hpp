#pragma once

#include <animation/MorphState.hpp>
#include <core/NonCopyable.hpp>
#include <graphics/resources/Buffer.hpp>
#include <graphics/resources/VertexArray.hpp>
#include <math/Bounds.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace stylized::asset
{

struct MeshAsset;
struct MeshPrimitiveAsset;

} // namespace stylized::asset

namespace stylized::graphics
{

class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{

class RuntimeMesh;

class RuntimeMeshInstance final
    : public core::NonCopyable
{
public:
    RuntimeMeshInstance() = default;

    RuntimeMeshInstance(
        RuntimeMeshInstance&& other) noexcept = default;

    RuntimeMeshInstance& operator=(
        RuntimeMeshInstance&& other) noexcept = default;

    ~RuntimeMeshInstance() = default;

    [[nodiscard]] bool initialize(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::MeshAsset& meshAsset,
        const RuntimeMesh& runtimeMesh);

    [[nodiscard]] bool update();

    void clear() noexcept;

    [[nodiscard]] animation::MorphState*
        morphState(
            std::size_t primitiveIndex) noexcept;

    [[nodiscard]] const animation::MorphState*
        morphState(
            std::size_t primitiveIndex) const noexcept;

    [[nodiscard]] const graphics::VertexArray*
        vertexArray(
            std::size_t primitiveIndex) const noexcept;

    [[nodiscard]] const math::Bounds*
        localBounds(
            std::size_t primitiveIndex) const noexcept;

    [[nodiscard]] float maximumPositionDelta(
        std::size_t primitiveIndex) const noexcept;

    [[nodiscard]] bool isValid() const noexcept;

    [[nodiscard]] std::size_t primitiveCount()
        const noexcept;

    [[nodiscard]] std::size_t morphPrimitiveCount()
        const noexcept;

    [[nodiscard]] std::size_t lastUploadCount()
        const noexcept;

    [[nodiscard]] std::size_t totalUploadCount()
        const noexcept;

private:
    struct PrimitiveInstance final
    {
        PrimitiveInstance() = default;

        PrimitiveInstance(
            PrimitiveInstance&& other) noexcept = default;

        PrimitiveInstance& operator=(
            PrimitiveInstance&& other) noexcept = default;

        animation::MorphState morphState;

        graphics::Buffer vertexBuffer;
        graphics::VertexArray vertexArray;

        math::Bounds localBounds;

        float maximumPositionDelta = 0.0F;

        std::uint64_t appliedMorphVersion = 0;
    };

    [[nodiscard]] bool initializePrimitive(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::MeshPrimitiveAsset& source,
        std::size_t primitiveIndex,
        PrimitiveInstance& destination);

    [[nodiscard]] bool uploadPrimitive(
        const asset::MeshPrimitiveAsset& source,
        PrimitiveInstance& destination);

    const asset::MeshAsset* meshAsset_ = nullptr;
    const RuntimeMesh* runtimeMesh_ = nullptr;

    std::vector<PrimitiveInstance> primitives_;

    std::size_t morphPrimitiveCount_ = 0;
    std::size_t lastUploadCount_ = 0;
    std::size_t totalUploadCount_ = 0;
};

} // namespace stylized::render
