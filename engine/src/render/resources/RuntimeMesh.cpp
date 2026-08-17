#include <render/resources/RuntimeMesh.hpp>

#include <asset/MeshAsset.hpp>
#include <graphics/device/GraphicsDevice.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace stylized::render
{

namespace
{

bool fitsUint32(const std::size_t value) noexcept
{
    return value <= static_cast<std::size_t> (std::numeric_limits<std::uint32_t>::max());
}

struct SkinnedMeshVertex
{
    glm::vec3 position{0.0F};
    glm::vec3 normal{0.0F, 1.0F, 0.0F};
    glm::vec4 tangent{1.0F, 0.0F, 0.0F, 1.0F};
    glm::vec2 texCoord0{0.0F};

    glm::uvec4 joints{0U};
    glm::vec4 weights{0.0F};
};

static_assert(
    std::is_standard_layout_v<
        SkinnedMeshVertex>);

static_assert(
    std::is_trivially_copyable_v<
        SkinnedMeshVertex>);

[[nodiscard]]
std::vector<SkinnedMeshVertex>
buildSkinnedVertices(
    const asset::MeshPrimitiveAsset& source)
{
    std::vector<SkinnedMeshVertex> vertices;

    vertices.reserve(source.vertices.size());

    for (std::size_t vertexIndex = 0;
         vertexIndex < source.vertices.size();
         ++vertexIndex)
    {
        const asset::StaticMeshVertex& geometry =
            source.vertices[vertexIndex];

        const asset::VertexSkinData& skin =
            source.skinVertices[vertexIndex];

        vertices.push_back(
            SkinnedMeshVertex{
                .position = geometry.position,
                .normal = geometry.normal,
                .tangent = geometry.tangent,
                .texCoord0 = geometry.texCoord0,
                .joints = skin.joints,
                .weights = skin.weights
            });
    }

    return vertices;
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

    if (source.hasSkin())
    {
        const std::vector<SkinnedMeshVertex>
            skinnedVertices =
                buildSkinnedVertices(source);

        destination.vertexBuffer_ =
            graphicsDevice.createBuffer(
                vertexBufferDesc,
                std::span<
                    const SkinnedMeshVertex>{
                        skinnedVertices});
    }
    else
    {
        destination.vertexBuffer_ =
            graphicsDevice.createBuffer(
                vertexBufferDesc,
                std::span<
                    const asset::StaticMeshVertex>{
                        source.vertices});
    }

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

    const std::array<graphics::VertexAttributeDesc, 4> staticAttributes{
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

    const std::array<graphics::VertexAttributeDesc, 6>
        skinnedAttributes{
            graphics::VertexAttributeDesc{
                .location = 0,
                .binding = vertexBinding,
                .format =
                    graphics::VertexAttributeFormat::Float3,
                .offset =
                    offsetof(
                        SkinnedMeshVertex,
                        position)
            },
            graphics::VertexAttributeDesc{
                .location = 1,
                .binding = vertexBinding,
                .format =
                    graphics::VertexAttributeFormat::Float3,
                .offset =
                    offsetof(
                        SkinnedMeshVertex,
                        normal)
            },
            graphics::VertexAttributeDesc{
                .location = 2,
                .binding = vertexBinding,
                .format =
                    graphics::VertexAttributeFormat::Float4,
                .offset =
                    offsetof(
                        SkinnedMeshVertex,
                        tangent)
            },
            graphics::VertexAttributeDesc{
                .location = 3,
                .binding = vertexBinding,
                .format =
                    graphics::VertexAttributeFormat::Float2,
                .offset =
                    offsetof(
                        SkinnedMeshVertex,
                        texCoord0)
            },
            graphics::VertexAttributeDesc{
                .location = 4,
                .binding = vertexBinding,
                .format =
                    graphics::VertexAttributeFormat::Uint4,
                .offset =
                    offsetof(
                        SkinnedMeshVertex,
                        joints)
            },
            graphics::VertexAttributeDesc{
                .location = 5,
                .binding = vertexBinding,
                .format =
                    graphics::VertexAttributeFormat::Float4,
                .offset =
                    offsetof(
                        SkinnedMeshVertex,
                        weights)
            }
        };

    graphics::VertexArrayDesc vertexArrayDesc;
    vertexArrayDesc.vertexBuffer = &destination.vertexBuffer_;
    vertexArrayDesc.indexBuffer = &destination.indexBuffer_;
    vertexArrayDesc.vertexBinding.binding = vertexBinding;

    if (source.hasSkin())
    {
        vertexArrayDesc.vertexBinding.stride =
            sizeof(SkinnedMeshVertex);

        vertexArrayDesc.attributes =
            std::span<
                const graphics::VertexAttributeDesc>{
                    skinnedAttributes};
    }
    else
    {
        vertexArrayDesc.vertexBinding.stride =
            sizeof(asset::StaticMeshVertex);

        vertexArrayDesc.attributes =
            std::span<
                const graphics::VertexAttributeDesc>{
                    staticAttributes};
    }

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
