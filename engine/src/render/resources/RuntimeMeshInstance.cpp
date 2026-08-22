#include <render/resources/RuntimeMeshInstance.hpp>

#include <asset/MeshAsset.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <render/resources/RuntimeMesh.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <glm/geometric.hpp>

namespace stylized::render
{

namespace
{

[[nodiscard]] glm::vec3 normalizeOrFallback(
    const glm::vec3& value,
    const glm::vec3& fallback) noexcept
{
    const float lengthSquared =
        glm::dot(value, value);

    if (!std::isfinite(lengthSquared) ||
        lengthSquared <= 1.0e-12F)
    {
        return fallback;
    }

    return value / std::sqrt(lengthSquared);
}

[[nodiscard]] bool buildMorphedVertices(
    const asset::MeshPrimitiveAsset& source,
    const animation::MorphState& morphState,
    const std::vector<std::vector<std::uint32_t>>&
        morphVertexIndices,
    std::vector<asset::StaticMeshVertex>& vertices,
    math::Bounds& bounds,
    float& maximumPositionDelta)
{
    if (!source.hasMorphTargets() ||
        source.vertices.empty() ||
        morphState.targetCount() !=
            source.morphTargets.size() ||
        morphVertexIndices.size() !=
            source.morphTargets.size())
    {
        return false;
    }

    vertices = source.vertices;

    bool normalsChanged = false;
    bool tangentsChanged = false;

    for (std::size_t targetIndex = 0;
         targetIndex < source.morphTargets.size();
         ++targetIndex)
    {
        const float weight =
            morphState.weight(targetIndex);

        if (weight == 0.0F)
        {
            continue;
        }

        const asset::MorphTargetAsset& target =
            source.morphTargets[targetIndex];

        const std::vector<std::uint32_t>&
            affectedVertices =
                morphVertexIndices[targetIndex];

        for (const std::uint32_t vertexIndex :
             affectedVertices)
        {
            asset::StaticMeshVertex& vertex =
                vertices[vertexIndex];

            vertex.position +=
                target.positionDeltas[vertexIndex] *
                weight;

            if (!target.normalDeltas.empty())
            {
                normalsChanged = true;

                vertex.normal +=
                    target.normalDeltas[vertexIndex] *
                    weight;
            }

            if (!target.tangentDeltas.empty())
            {
                tangentsChanged = true;

                glm::vec3 tangent{
                    vertex.tangent};

                tangent +=
                    target.tangentDeltas[vertexIndex] *
                    weight;

                vertex.tangent = glm::vec4{
                    tangent,
                    vertex.tangent.w};
            }
        }
    }

    bounds.reset();
    float maximumPositionDeltaSquared = 0.0F;

    for (std::size_t vertexIndex = 0;
         vertexIndex < vertices.size();
         ++vertexIndex)
    {
        asset::StaticMeshVertex& vertex =
            vertices[vertexIndex];

        const asset::StaticMeshVertex& baseVertex =
            source.vertices[vertexIndex];

        if (normalsChanged)
        {
            vertex.normal = normalizeOrFallback(
                vertex.normal,
                baseVertex.normal);
        }

        if (tangentsChanged)
        {
            const glm::vec3 tangent =
                normalizeOrFallback(
                glm::vec3{vertex.tangent},
                glm::vec3{baseVertex.tangent});

            vertex.tangent = glm::vec4{
                tangent,
                baseVertex.tangent.w};
        }

        bounds.expand(vertex.position);

        const glm::vec3 positionDelta =
            vertex.position - baseVertex.position;

        maximumPositionDeltaSquared = std::max(
            maximumPositionDeltaSquared,
            glm::dot(positionDelta, positionDelta));
    }

    maximumPositionDelta =
        std::sqrt(maximumPositionDeltaSquared);

    return bounds.isValid();
}

[[nodiscard]] bool buildMorphVertexIndices(
    const asset::MeshPrimitiveAsset& source,
    std::vector<std::vector<std::uint32_t>>& indices)
{
    indices.clear();
    indices.resize(source.morphTargets.size());

    for (std::size_t targetIndex = 0;
         targetIndex < source.morphTargets.size();
         ++targetIndex)
    {
        const asset::MorphTargetAsset& target =
            source.morphTargets[targetIndex];

        std::vector<std::uint32_t>& targetIndices =
            indices[targetIndex];

        for (std::size_t vertexIndex = 0;
             vertexIndex < source.vertices.size();
             ++vertexIndex)
        {
            const bool positionChanged =
                target.positionDeltas[vertexIndex] !=
                glm::vec3{0.0F};

            const bool normalChanged =
                !target.normalDeltas.empty() &&
                target.normalDeltas[vertexIndex] !=
                    glm::vec3{0.0F};

            const bool tangentChanged =
                !target.tangentDeltas.empty() &&
                target.tangentDeltas[vertexIndex] !=
                    glm::vec3{0.0F};

            if (positionChanged ||
                normalChanged ||
                tangentChanged)
            {
                targetIndices.push_back(
                    static_cast<std::uint32_t>(
                        vertexIndex));
            }
        }
    }

    return true;
}

template<typename Vertex>
void buildSkinnedVertices(
    const asset::MeshPrimitiveAsset& source,
    const std::vector<asset::StaticMeshVertex>&
        geometry,
    std::vector<Vertex>& vertices)
{
    const bool initializeFixedData =
        vertices.size() != geometry.size();

    if (initializeFixedData)
    {
        vertices.resize(geometry.size());
    }

    for (std::size_t vertexIndex = 0;
         vertexIndex < geometry.size();
         ++vertexIndex)
    {
        const asset::VertexSkinData& skin =
            source.skinVertices[vertexIndex];

        Vertex& vertex = vertices[vertexIndex];

        vertex.position = geometry[vertexIndex].position;
        vertex.normal = geometry[vertexIndex].normal;
        vertex.tangent = geometry[vertexIndex].tangent;

        if (initializeFixedData)
        {
            vertex.texCoord0 =
                geometry[vertexIndex].texCoord0;
            vertex.joints = skin.joints;
            vertex.weights = skin.weights;
        }
    }
}

constexpr std::uint32_t vertexBinding = 0;

const std::array<graphics::VertexAttributeDesc, 4>
    staticAttributes{
        graphics::VertexAttributeDesc{
            .location = 0,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float3,
            .offset = offsetof(
                asset::StaticMeshVertex,
                position)
        },
        graphics::VertexAttributeDesc{
            .location = 1,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float3,
            .offset = offsetof(
                asset::StaticMeshVertex,
                normal)
        },
        graphics::VertexAttributeDesc{
            .location = 2,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float4,
            .offset = offsetof(
                asset::StaticMeshVertex,
                tangent)
        },
        graphics::VertexAttributeDesc{
            .location = 3,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float2,
            .offset = offsetof(
                asset::StaticMeshVertex,
                texCoord0)
        }
    };

const std::array<graphics::VertexAttributeDesc, 6>
    skinnedAttributes{
        graphics::VertexAttributeDesc{
            .location = 0,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float3,
            .offset = offsetof(
            detail::MorphedSkinnedVertex,
                position)
        },
        graphics::VertexAttributeDesc{
            .location = 1,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float3,
            .offset = offsetof(
            detail::MorphedSkinnedVertex,
                normal)
        },
        graphics::VertexAttributeDesc{
            .location = 2,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float4,
            .offset = offsetof(
            detail::MorphedSkinnedVertex,
                tangent)
        },
        graphics::VertexAttributeDesc{
            .location = 3,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float2,
            .offset = offsetof(
            detail::MorphedSkinnedVertex,
                texCoord0)
        },
        graphics::VertexAttributeDesc{
            .location = 4,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Uint4,
            .offset = offsetof(
            detail::MorphedSkinnedVertex,
                joints)
        },
        graphics::VertexAttributeDesc{
            .location = 5,
            .binding = vertexBinding,
            .format = graphics::VertexAttributeFormat::Float4,
            .offset = offsetof(
            detail::MorphedSkinnedVertex,
                weights)
        }
    };

} // namespace

bool RuntimeMeshInstance::initialize(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::MeshAsset& meshAsset,
    const RuntimeMesh& runtimeMesh)
{
    clear();

    if (!meshAsset.isValid() ||
        !runtimeMesh.isValid() ||
        meshAsset.primitives.size() !=
            runtimeMesh.primitives().size())
    {
        return false;
    }

    meshAsset_ = &meshAsset;
    runtimeMesh_ = &runtimeMesh;

    primitives_.resize(meshAsset.primitives.size());

    for (std::size_t primitiveIndex = 0;
         primitiveIndex < meshAsset.primitives.size();
         ++primitiveIndex)
    {
        const asset::MeshPrimitiveAsset& source =
            meshAsset.primitives[primitiveIndex];

        if (!source.hasMorphTargets())
        {
            continue;
        }

        if (!initializePrimitive(
                graphicsDevice,
                source,
                primitiveIndex,
                primitives_[primitiveIndex]))
        {
            clear();
            return false;
        }

        ++morphPrimitiveCount_;
        ++lastUploadCount_;
        ++totalUploadCount_;
    }

    return morphPrimitiveCount_ > 0;
}

bool RuntimeMeshInstance::update()
{
    lastUploadCount_ = 0;

    if (!isValid())
    {
        return false;
    }

    for (std::size_t primitiveIndex = 0;
         primitiveIndex < primitives_.size();
         ++primitiveIndex)
    {
        PrimitiveInstance& destination =
            primitives_[primitiveIndex];

        if (!destination.vertexBuffer.isValid())
        {
            continue;
        }

        if (destination.appliedMorphVersion ==
            destination.morphState.version())
        {
            continue;
        }

        if (!uploadPrimitive(
                meshAsset_->primitives[primitiveIndex],
                destination))
        {
            return false;
        }

        destination.appliedMorphVersion =
            destination.morphState.version();

        ++lastUploadCount_;
        ++totalUploadCount_;
    }

    return true;
}

void RuntimeMeshInstance::clear() noexcept
{
    primitives_.clear();

    meshAsset_ = nullptr;
    runtimeMesh_ = nullptr;

    morphPrimitiveCount_ = 0;
    lastUploadCount_ = 0;
    totalUploadCount_ = 0;
}

animation::MorphState*
RuntimeMeshInstance::morphState(
    const std::size_t primitiveIndex) noexcept
{
    if (primitiveIndex >= primitives_.size() ||
        !primitives_[primitiveIndex].vertexBuffer.isValid())
    {
        return nullptr;
    }

    return &primitives_[primitiveIndex].morphState;
}

const animation::MorphState*
RuntimeMeshInstance::morphState(
    const std::size_t primitiveIndex) const noexcept
{
    if (primitiveIndex >= primitives_.size() ||
        !primitives_[primitiveIndex].vertexBuffer.isValid())
    {
        return nullptr;
    }

    return &primitives_[primitiveIndex].morphState;
}

const graphics::VertexArray*
RuntimeMeshInstance::vertexArray(
    const std::size_t primitiveIndex) const noexcept
{
    if (runtimeMesh_ == nullptr ||
        primitiveIndex >= primitives_.size())
    {
        return nullptr;
    }

    const PrimitiveInstance& instance =
        primitives_[primitiveIndex];

    if (instance.vertexArray.isValid())
    {
        return &instance.vertexArray;
    }

    return &runtimeMesh_->primitives()[
        primitiveIndex].vertexArray();
}

const math::Bounds*
RuntimeMeshInstance::localBounds(
    const std::size_t primitiveIndex) const noexcept
{
    if (runtimeMesh_ == nullptr ||
        primitiveIndex >= primitives_.size())
    {
        return nullptr;
    }

    const PrimitiveInstance& instance =
        primitives_[primitiveIndex];

    if (instance.vertexArray.isValid())
    {
        return &instance.localBounds;
    }

    return &runtimeMesh_->primitives()[
        primitiveIndex].localBounds();
}

float RuntimeMeshInstance::maximumPositionDelta(
    const std::size_t primitiveIndex) const noexcept
{
    if (primitiveIndex >= primitives_.size())
    {
        return 0.0F;
    }

    return primitives_[primitiveIndex]
        .maximumPositionDelta;
}

bool RuntimeMeshInstance::isValid() const noexcept
{
    if (meshAsset_ == nullptr ||
        runtimeMesh_ == nullptr ||
        !runtimeMesh_->isValid() ||
        primitives_.size() !=
            meshAsset_->primitives.size() ||
        primitives_.size() !=
            runtimeMesh_->primitives().size() ||
        morphPrimitiveCount_ == 0)
    {
        return false;
    }

    std::size_t validMorphPrimitiveCount = 0;

    for (std::size_t primitiveIndex = 0;
         primitiveIndex < primitives_.size();
         ++primitiveIndex)
    {
        const asset::MeshPrimitiveAsset& source =
            meshAsset_->primitives[primitiveIndex];

        const PrimitiveInstance& instance =
            primitives_[primitiveIndex];

        if (!source.hasMorphTargets())
        {
            if (instance.vertexBuffer.isValid() ||
                instance.vertexArray.isValid())
            {
                return false;
            }

            continue;
        }

        if (!instance.vertexBuffer.isValid() ||
            !instance.vertexArray.isValid() ||
            !instance.vertexArray.hasIndexBuffer() ||
            !instance.localBounds.isValid() ||
            instance.morphState.targetCount() !=
                source.morphTargets.size())
        {
            return false;
        }

        ++validMorphPrimitiveCount;
    }

    return validMorphPrimitiveCount ==
        morphPrimitiveCount_;
}

std::size_t RuntimeMeshInstance::primitiveCount()
    const noexcept
{
    return primitives_.size();
}

std::size_t RuntimeMeshInstance::morphPrimitiveCount()
    const noexcept
{
    return morphPrimitiveCount_;
}

std::size_t RuntimeMeshInstance::lastUploadCount()
    const noexcept
{
    return lastUploadCount_;
}

std::size_t RuntimeMeshInstance::totalUploadCount()
    const noexcept
{
    return totalUploadCount_;
}

bool RuntimeMeshInstance::initializePrimitive(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::MeshPrimitiveAsset& source,
    const std::size_t primitiveIndex,
    PrimitiveInstance& destination)
{
    if (runtimeMesh_ == nullptr ||
        !source.hasMorphTargets() ||
        !destination.morphState.initialize(
            source.morphTargets.size()) ||
        !buildMorphVertexIndices(
            source,
            destination.morphVertexIndices))
    {
        return false;
    }

    if (!buildMorphedVertices(
            source,
            destination.morphState,
            destination.morphVertexIndices,
            destination.morphedVertices,
            destination.localBounds,
            destination.maximumPositionDelta))
    {
        return false;
    }

    const std::string primitiveName =
        meshAsset_->name +
        " Morph Primitive " +
        std::to_string(primitiveIndex);

    graphics::BufferDesc vertexBufferDesc;
    vertexBufferDesc.usage =
        graphics::BufferUsage::Dynamic;
    vertexBufferDesc.debugName =
        primitiveName + " Vertex Buffer";

    if (source.hasSkin())
    {
        buildSkinnedVertices(
            source,
            destination.morphedVertices,
            destination.skinnedVertices);

        destination.vertexBuffer =
            graphicsDevice.createBuffer(
                vertexBufferDesc,
                std::span<
                    const detail::MorphedSkinnedVertex>{
                        destination.skinnedVertices});
    }
    else
    {
        destination.vertexBuffer =
            graphicsDevice.createBuffer(
                vertexBufferDesc,
                std::span<
                    const asset::StaticMeshVertex>{
                        destination.morphedVertices});
    }

    if (!destination.vertexBuffer.isValid())
    {
        return false;
    }

    const RuntimeMeshPrimitive& sharedPrimitive =
        runtimeMesh_->primitives()[primitiveIndex];

    graphics::VertexArrayDesc vertexArrayDesc;
    vertexArrayDesc.vertexBuffer =
        &destination.vertexBuffer;
    vertexArrayDesc.indexBuffer =
        &sharedPrimitive.indexBuffer_;
    vertexArrayDesc.vertexBinding.binding =
        vertexBinding;

    if (source.hasSkin())
    {
        vertexArrayDesc.vertexBinding.stride =
            sizeof(detail::MorphedSkinnedVertex);
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

    vertexArrayDesc.debugName =
        primitiveName + " Vertex Array";

    destination.vertexArray =
        graphicsDevice.createVertexArray(
            vertexArrayDesc);

    if (!destination.vertexArray.isValid())
    {
        return false;
    }

    destination.appliedMorphVersion =
        destination.morphState.version();

    return true;
}

bool RuntimeMeshInstance::uploadPrimitive(
    const asset::MeshPrimitiveAsset& source,
    PrimitiveInstance& destination)
{
    math::Bounds morphedBounds;
    float maximumPositionDelta = 0.0F;

    if (!buildMorphedVertices(
            source,
            destination.morphState,
            destination.morphVertexIndices,
            destination.morphedVertices,
            morphedBounds,
            maximumPositionDelta))
    {
        return false;
    }

    bool uploaded = false;

    if (source.hasSkin())
    {
        buildSkinnedVertices(
            source,
            destination.morphedVertices,
            destination.skinnedVertices);

        uploaded = destination.vertexBuffer.update(
            0,
            std::span<
                    const detail::MorphedSkinnedVertex>{
                        destination.skinnedVertices});
    }
    else
    {
        uploaded = destination.vertexBuffer.update(
            0,
            std::span<
                const asset::StaticMeshVertex>{
                    destination.morphedVertices});
    }

    if (!uploaded)
    {
        return false;
    }

    destination.localBounds = morphedBounds;
    destination.maximumPositionDelta =
        maximumPositionDelta;

    return true;
}

} // namespace stylized::render
