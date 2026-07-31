#include <render/RuntimeMesh.hpp>

#include <asset/MeshAsset.hpp>
#include <graphics/GraphicsDevice.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <limits>
#include <span>
#include <string>

namespace stylized::render
{

namespace
{

bool fitsUint32(const std::size_t value) noexcept
{
    return value <= static_cast<std::size_t> (std::numeric_limits<std::uint32_t>::max());
}

} // namespace

bool RuntimeMesh::uploadPrimitive(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::MeshPrimitiveAsset& source,
    const std::string& meshName,
    const std::size_t primitiveIndex,
    RuntimeMeshPrimitive& destination
)
{
    if (!source.isValid()) return false;

    if (!fitsUint32(source.indices.size())) return false;

    const std::string primitiveName = meshName + " Primitive " + std::to_string(primitiveIndex);

    graphics::BufferDesc vertexBufferDesc;
    vertexBufferDesc.usage = graphics::BufferUsage::Static;
    vertexBufferDesc.debugName = primitiveName + " Vertex Buffer";
    destination.vertexBuffer_ = graphicsDevice.createBuffer(vertexBufferDesc,
        std::span<const asset::StaticMeshVertex>{source.vertices});
    if (!destination.vertexBuffer_.isValid())
    {
        std::cerr
            << "Failed to upload vertex buffer for "
            << primitiveName
            << ".\n";

        return false;
    }

    graphics::BufferDesc indexBufferDesc;
    indexBufferDesc.usage = graphics::BufferUsage::Static;
    indexBufferDesc.debugName = primitiveName + " Index Buffer";
    destination.indexBuffer_ = graphicsDevice.createBuffer(indexBufferDesc,
        std::span<const std::uint32_t>{source.indices});
    if (!destination.indexBuffer_.isValid())
    {
        std::cerr
            << "Failed to upload index buffer for "
            << primitiveName
            << ".\n";

        return false;
    }

    constexpr std::uint32_t vertexBinding = 0;

    const std::array<graphics::VertexAttributeDesc, 4> attributes{
        graphics::VertexAttributeDesc{
            .location = 0,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float3,
            .offset = offsetof(
                asset::StaticMeshVertex,
                position
            )
        },
        graphics::VertexAttributeDesc{
            .location = 1,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float3,
            .offset = offsetof(
                asset::StaticMeshVertex,
                normal
            )
        },
        graphics::VertexAttributeDesc{
                .location = 2,
                .binding = vertexBinding,
                .format =
                    graphics::VertexAttributeFormat::Float4,
                .offset =
                    offsetof(
                        asset::StaticMeshVertex,
                        tangent)
        },
        graphics::VertexAttributeDesc{
            .location = 3,
            .binding = vertexBinding,
            .format =
                graphics::VertexAttributeFormat::Float2,
            .offset =
                offsetof(
                    asset::StaticMeshVertex,
                    texCoord0)
        }
    };

    graphics::VertexArrayDesc vertexArrayDesc;
    vertexArrayDesc.vertexBuffer = &destination.vertexBuffer_;
    vertexArrayDesc.indexBuffer = &destination.indexBuffer_;
    vertexArrayDesc.vertexBinding.binding = vertexBinding;
    vertexArrayDesc.vertexBinding.stride = sizeof(asset::StaticMeshVertex);
    vertexArrayDesc.attributes = std::span<const graphics::VertexAttributeDesc>{attributes};
    vertexArrayDesc.vertexBufferOffset = 0;
    vertexArrayDesc.debugName = primitiveName + " Vertex Array";

    destination.vertexArray_ = graphicsDevice.createVertexArray(vertexArrayDesc);
    if (!destination.vertexArray_.isValid())
    {
        std::cerr
            << "Failed to create vertex array for "
            << primitiveName
            << ".\n";

        return false;
    }

    destination.indexCount_ = static_cast<std::uint32_t>(source.indices.size());
    destination.indexType_ = graphics::IndexType::Uint32;
    destination.material_ = source.material;
    destination.localBounds_ = source.localBounds;

    return true;
}

bool RuntimeMeshPrimitive::isValid() const noexcept
{
    return
        vertexBuffer_.isValid() &&
        indexBuffer_.isValid() &&
        vertexArray_.isValid() &&
        vertexArray_.hasIndexBuffer() &&
        indexCount_ > 0 &&
        localBounds_.isValid();
}

const graphics::VertexArray& RuntimeMeshPrimitive::vertexArray() const noexcept
{
    return vertexArray_;
}

std::uint32_t RuntimeMeshPrimitive::indexCount() const noexcept
{
    return indexCount_;
}

graphics::IndexType RuntimeMeshPrimitive::indexType() const noexcept
{
    return indexType_;
}

asset::AssetHandle<asset::MaterialAsset> RuntimeMeshPrimitive::material() const noexcept
{
    return material_;
}

const math::Bounds& RuntimeMeshPrimitive::localBounds() const noexcept
{
    return localBounds_;
}

RuntimeMesh RuntimeMesh::create(graphics::GraphicsDevice& graphicsDevice,
     const asset::MeshAsset& meshAsset)
{
    RuntimeMesh runtimeMesh;
    if (!meshAsset.isValid())
    {
        std::cerr << "Cannot upload an invalid MeshAsset.\n";
        return runtimeMesh;
    }

    runtimeMesh.name_ = meshAsset.name;
    runtimeMesh.localBounds_ = meshAsset.localBounds;
    runtimeMesh.primitives_.reserve(meshAsset.primitives.size());

    for (std::size_t primitiveIndex = 0; primitiveIndex < meshAsset.primitives.size(); ++primitiveIndex)
    {
        RuntimeMeshPrimitive primitive;

        if (!uploadPrimitive(
            graphicsDevice,
            meshAsset.primitives[primitiveIndex],
            meshAsset.name,
            primitiveIndex,
            primitive
        ))
        {
            return {};
        }

        runtimeMesh.primitives_.push_back(std::move(primitive));
    }

    return runtimeMesh;
}

bool RuntimeMesh::isValid() const noexcept
{
    if (primitives_.empty() || !localBounds_.isValid()) return false;

    for (const RuntimeMeshPrimitive& primitive : primitives_)
    {
        if (!primitive.isValid()) return false;
    }

    return true;
}

const std::string& RuntimeMesh::name() const noexcept
{
    return name_;
}

const std::vector<RuntimeMeshPrimitive>& 
RuntimeMesh::primitives() const noexcept
{
    return primitives_;
}

const math::Bounds& RuntimeMesh::localBounds() const noexcept
{
    return localBounds_;
}

} // namespace stylized::render
