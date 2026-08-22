#include <render/resources/RuntimeMeshInstance.hpp>

#include <asset/MeshAsset.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <render/resources/RuntimeMesh.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace stylized::render
{

namespace
{

struct GpuMorphBaseVertex
{
    glm::vec4 position{0.0F};
    glm::vec4 normal{0.0F};
    glm::vec4 tangent{0.0F};
};

struct GpuMorphDelta
{
    glm::vec4 position{0.0F};
    glm::vec4 normal{0.0F};
    glm::vec4 tangent{0.0F};
    glm::uvec4 metadata{0U};
};

struct GpuMorphData
{
    std::vector<GpuMorphBaseVertex> baseVertices;
    std::vector<std::uint32_t> vertexOffsets;
    std::vector<GpuMorphDelta> deltas;

    math::Bounds conservativeBounds;
    float maximumPositionDelta = 0.0F;
};

static_assert(sizeof(GpuMorphBaseVertex) == 48);
static_assert(sizeof(GpuMorphDelta) == 64);
static_assert(std::is_trivially_copyable_v<GpuMorphBaseVertex>);
static_assert(std::is_trivially_copyable_v<GpuMorphDelta>);

[[nodiscard]] bool hasDelta(
    const glm::vec3& value) noexcept
{
    return glm::dot(value, value) > 0.0F;
}

[[nodiscard]] bool buildGpuMorphData(
    const asset::MeshPrimitiveAsset& source,
    GpuMorphData& data)
{
    if (!source.hasMorphTargets() ||
        source.vertices.empty() ||
        source.vertices.size() >
            std::numeric_limits<std::uint32_t>::max() ||
        source.morphTargets.size() >
            std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    data = {};
    data.baseVertices.reserve(source.vertices.size());
    data.vertexOffsets.reserve(source.vertices.size() + 1);

    for (std::size_t vertexIndex = 0;
         vertexIndex < source.vertices.size();
         ++vertexIndex)
    {
        const asset::StaticMeshVertex& vertex =
            source.vertices[vertexIndex];

        data.baseVertices.push_back(
            GpuMorphBaseVertex{
                .position = glm::vec4{
                    vertex.position,
                    0.0F},
                .normal = glm::vec4{
                    vertex.normal,
                    0.0F},
                .tangent = vertex.tangent
            });

        data.vertexOffsets.push_back(
            static_cast<std::uint32_t>(
                data.deltas.size()));

        glm::vec3 minimumDelta{0.0F};
        glm::vec3 maximumDelta{0.0F};

        for (std::size_t targetIndex = 0;
             targetIndex < source.morphTargets.size();
             ++targetIndex)
        {
            const asset::MorphTargetAsset& target =
                source.morphTargets[targetIndex];

            const glm::vec3 positionDelta =
                target.positionDeltas[vertexIndex];

            const glm::vec3 normalDelta =
                target.normalDeltas.empty()
                    ? glm::vec3{0.0F}
                    : target.normalDeltas[vertexIndex];

            const glm::vec3 tangentDelta =
                target.tangentDeltas.empty()
                    ? glm::vec3{0.0F}
                    : target.tangentDeltas[vertexIndex];

            minimumDelta += glm::min(
                positionDelta,
                glm::vec3{0.0F});

            maximumDelta += glm::max(
                positionDelta,
                glm::vec3{0.0F});

            if (hasDelta(positionDelta) ||
                hasDelta(normalDelta) ||
                hasDelta(tangentDelta))
            {
                if (data.deltas.size() >=
                    std::numeric_limits<
                        std::uint32_t>::max())
                {
                    return false;
                }

                data.deltas.push_back(
                    GpuMorphDelta{
                        .position = glm::vec4{
                            positionDelta,
                            0.0F},
                        .normal = glm::vec4{
                            normalDelta,
                            0.0F},
                        .tangent = glm::vec4{
                            tangentDelta,
                            0.0F},
                        .metadata = glm::uvec4{
                            static_cast<std::uint32_t>(
                                targetIndex),
                            0U,
                            0U,
                            0U}
                    });
            }
        }

        data.conservativeBounds.expand(
            vertex.position + minimumDelta);

        data.conservativeBounds.expand(
            vertex.position + maximumDelta);

        const glm::vec3 absoluteMaximum = glm::max(
            glm::abs(minimumDelta),
            glm::abs(maximumDelta));

        data.maximumPositionDelta = std::max(
            data.maximumPositionDelta,
            glm::length(absoluteMaximum));
    }

    data.vertexOffsets.push_back(
        static_cast<std::uint32_t>(
            data.deltas.size()));

    if (data.deltas.empty())
    {
        data.deltas.emplace_back();
    }

    return data.conservativeBounds.isValid();
}

[[nodiscard]] std::vector<detail::MorphedSkinnedVertex>
    buildInitialSkinnedVertices(
        const asset::MeshPrimitiveAsset& source)
{
    std::vector<detail::MorphedSkinnedVertex> result;
    result.reserve(source.vertices.size());

    for (std::size_t vertexIndex = 0;
         vertexIndex < source.vertices.size();
         ++vertexIndex)
    {
        const asset::StaticMeshVertex& vertex =
            source.vertices[vertexIndex];

        const asset::VertexSkinData& skin =
            source.skinVertices[vertexIndex];

        result.push_back(
            detail::MorphedSkinnedVertex{
                .position = vertex.position,
                .normal = vertex.normal,
                .tangent = vertex.tangent,
                .texCoord0 = vertex.texCoord0,
                .joints = skin.joints,
                .weights = skin.weights
            });
    }

    return result;
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
    graphics::ShaderProgram& morphComputeProgram,
    const asset::MeshAsset& meshAsset,
    const RuntimeMesh& runtimeMesh)
{
    clear();

    if (!morphComputeProgram.isValid() ||
        !meshAsset.isValid() ||
        !runtimeMesh.isValid() ||
        meshAsset.primitives.size() !=
            runtimeMesh.primitives().size())
    {
        return false;
    }

    meshAsset_ = &meshAsset;
    runtimeMesh_ = &runtimeMesh;
    morphComputeProgram_ = &morphComputeProgram;

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

    bool dispatched = false;

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
        dispatched = true;
    }

    if (dispatched)
    {
        morphComputeProgram_
            ->makeComputeWritesVisibleToVertexInput();
    }

    return true;
}

void RuntimeMeshInstance::clear() noexcept
{
    primitives_.clear();

    meshAsset_ = nullptr;
    runtimeMesh_ = nullptr;
    morphComputeProgram_ = nullptr;

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
        morphComputeProgram_ == nullptr ||
        !morphComputeProgram_->isValid() ||
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
            !instance.baseVertexBuffer.isValid() ||
            !instance.morphOffsetBuffer.isValid() ||
            !instance.morphDeltaBuffer.isValid() ||
            !instance.morphWeightBuffer.isValid() ||
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
        morphComputeProgram_ == nullptr ||
        !source.hasMorphTargets() ||
        !destination.morphState.initialize(
            source.morphTargets.size()))
    {
        return false;
    }

    GpuMorphData gpuData;

    if (!buildGpuMorphData(source, gpuData))
    {
        return false;
    }

    destination.localBounds =
        gpuData.conservativeBounds;

    destination.maximumPositionDelta =
        gpuData.maximumPositionDelta;

    const std::string primitiveName =
        meshAsset_->name +
        " Morph Primitive " +
        std::to_string(primitiveIndex);

    graphics::BufferDesc vertexBufferDesc;
    vertexBufferDesc.usage =
        graphics::BufferUsage::Dynamic;
    vertexBufferDesc.debugName =
        primitiveName + " Vertex Buffer";

    graphics::BufferDesc baseVertexBufferDesc;
    baseVertexBufferDesc.usage =
        graphics::BufferUsage::Static;
    baseVertexBufferDesc.debugName =
        primitiveName + " Morph Base Vertices";

    destination.baseVertexBuffer =
        graphicsDevice.createBuffer(
            baseVertexBufferDesc,
            std::span<const GpuMorphBaseVertex>{
                gpuData.baseVertices});

    graphics::BufferDesc morphOffsetBufferDesc;
    morphOffsetBufferDesc.usage =
        graphics::BufferUsage::Static;
    morphOffsetBufferDesc.debugName =
        primitiveName + " Morph Vertex Offsets";

    destination.morphOffsetBuffer =
        graphicsDevice.createBuffer(
            morphOffsetBufferDesc,
            std::span<const std::uint32_t>{
                gpuData.vertexOffsets});

    graphics::BufferDesc morphDeltaBufferDesc;
    morphDeltaBufferDesc.usage =
        graphics::BufferUsage::Static;
    morphDeltaBufferDesc.debugName =
        primitiveName + " Morph Deltas";

    destination.morphDeltaBuffer =
        graphicsDevice.createBuffer(
            morphDeltaBufferDesc,
            std::span<const GpuMorphDelta>{
                gpuData.deltas});

    graphics::BufferDesc morphWeightBufferDesc;
    morphWeightBufferDesc.usage =
        graphics::BufferUsage::Dynamic;
    morphWeightBufferDesc.debugName =
        primitiveName + " Morph Weights";

    destination.morphWeightBuffer =
        graphicsDevice.createBuffer(
            morphWeightBufferDesc,
            destination.morphState.weights());

    if (source.hasSkin())
    {
        const std::vector<detail::MorphedSkinnedVertex>
            initialVertices =
                buildInitialSkinnedVertices(source);

        destination.vertexBuffer =
            graphicsDevice.createBuffer(
                vertexBufferDesc,
                std::span<
                    const detail::MorphedSkinnedVertex>{
                        initialVertices});
    }
    else
    {
        destination.vertexBuffer =
            graphicsDevice.createBuffer(
                vertexBufferDesc,
                std::span<
                    const asset::StaticMeshVertex>{
                        source.vertices});
    }

    if (!destination.vertexBuffer.isValid() ||
        !destination.baseVertexBuffer.isValid() ||
        !destination.morphOffsetBuffer.isValid() ||
        !destination.morphDeltaBuffer.isValid() ||
        !destination.morphWeightBuffer.isValid())
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
    if (morphComputeProgram_ == nullptr ||
        source.vertices.empty() ||
        source.vertices.size() >
            std::numeric_limits<std::uint32_t>::max() ||
        !destination.morphWeightBuffer.update(
            0,
            destination.morphState.weights()))
    {
        return false;
    }

    destination.baseVertexBuffer.bindShaderStorage(0);
    destination.morphOffsetBuffer.bindShaderStorage(1);
    destination.morphDeltaBuffer.bindShaderStorage(2);
    destination.morphWeightBuffer.bindShaderStorage(3);
    destination.vertexBuffer.bindShaderStorage(4);

    const std::uint32_t vertexCount =
        static_cast<std::uint32_t>(
            source.vertices.size());

    const std::uint32_t outputStrideWords =
        static_cast<std::uint32_t>(
            source.hasSkin()
                ? sizeof(detail::MorphedSkinnedVertex)
                : sizeof(asset::StaticMeshVertex)) /
        sizeof(std::uint32_t);

    const std::uint32_t positionOffsetWords = 0;

    const std::uint32_t normalOffsetWords =
        static_cast<std::uint32_t>(
            source.hasSkin()
                ? offsetof(
                    detail::MorphedSkinnedVertex,
                    normal)
                : offsetof(
                    asset::StaticMeshVertex,
                    normal)) /
        sizeof(std::uint32_t);

    const std::uint32_t tangentOffsetWords =
        static_cast<std::uint32_t>(
            source.hasSkin()
                ? offsetof(
                    detail::MorphedSkinnedVertex,
                    tangent)
                : offsetof(
                    asset::StaticMeshVertex,
                    tangent)) /
        sizeof(std::uint32_t);

    if (!morphComputeProgram_->setUInt(
            "uVertexCount",
            vertexCount) ||
        !morphComputeProgram_->setUInt(
            "uOutputStrideWords",
            outputStrideWords) ||
        !morphComputeProgram_->setUInt(
            "uPositionOffsetWords",
            positionOffsetWords) ||
        !morphComputeProgram_->setUInt(
            "uNormalOffsetWords",
            normalOffsetWords) ||
        !morphComputeProgram_->setUInt(
            "uTangentOffsetWords",
            tangentOffsetWords))
    {
        return false;
    }

    constexpr std::uint32_t workGroupSize = 64;

    return morphComputeProgram_->dispatchCompute(
        (vertexCount + workGroupSize - 1) /
            workGroupSize);
}

} // namespace stylized::render
