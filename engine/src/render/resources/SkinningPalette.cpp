#include <render/resources/SkinningPalette.hpp>

#include <animation/ScenePose.hpp>
#include <asset/MeshAsset.hpp>
#include <graphics/device/GraphicsDevice.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>

#include <glm/matrix.hpp>

namespace stylized::render
{

namespace
{

constexpr float minimumAbsoluteDeterminant =
    1.0e-8F;

} // namespace

bool SkinningPalette::initializeGpuBuffer(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::SkinAsset& skin)
{
    clear();

    if (!skin.isValid())
    {
        gpuBuffer_ = {};
        jointCapacity_ = 0;

        return false;
    }

    const std::size_t jointCount =
        skin.jointNodeIndices.size();

    if (gpuBuffer_.isValid() &&
        jointCount == jointCapacity_)
    {
        return true;
    }

    gpuBuffer_ = {};
    jointCapacity_ = 0;

    if (jointCount >
        std::numeric_limits<std::size_t>::max() /
            sizeof(glm::mat4))
    {
        return false;
    }

    graphics::BufferDesc bufferDesc;

    bufferDesc.size =
        jointCount * sizeof(glm::mat4);

    bufferDesc.usage =
        graphics::BufferUsage::Dynamic;

    bufferDesc.debugName =
        "Skinning Palette Buffer";

    graphics::Buffer buffer =
        graphicsDevice.createBuffer(bufferDesc);

    if (!buffer.isValid())
    {
        return false;
    }

    gpuBuffer_ = std::move(buffer);
    jointCapacity_ = jointCount;

    return true;
}

bool SkinningPalette::update(
    const asset::SkinAsset& skin,
    const std::uint32_t meshNodeIndex,
    const animation::ScenePose& pose)
{
    clear();

    if (!skin.isValid() ||
        !pose.isInitialized() ||
        pose.worldMatricesDirty() ||
        !gpuBuffer_.isValid() ||
        jointCapacity_ !=
            skin.jointNodeIndices.size())
    {
        return false;
    }

    const glm::mat4* meshWorld =
        pose.worldMatrix(meshNodeIndex);

    if (meshWorld == nullptr)
    {
        return false;
    }

    const float determinant =
        glm::determinant(*meshWorld);

    if (!std::isfinite(determinant) ||
        std::abs(determinant) <=
            minimumAbsoluteDeterminant)
    {
        return false;
    }

    const glm::mat4 inverseMeshWorld =
        glm::inverse(*meshWorld);

    matrices_.reserve(
        skin.jointNodeIndices.size());

    for (std::size_t jointIndex = 0;
         jointIndex <
             skin.jointNodeIndices.size();
         ++jointIndex)
    {
        const std::uint32_t jointNodeIndex =
            skin.jointNodeIndices[jointIndex];

        const glm::mat4* jointWorld =
            pose.worldMatrix(jointNodeIndex);

        if (jointWorld == nullptr)
        {
            clear();
            return false;
        }

        matrices_.push_back(
            inverseMeshWorld *
            *jointWorld *
            skin.inverseBindMatrices[jointIndex]);
    }

    currentLocalBounds_.reset();

    for (std::size_t boundsIndex = 0;
         boundsIndex < matrices_.size();
         ++boundsIndex)
    {
        const math::Bounds& jointBounds =
            skin.jointLocalBounds[boundsIndex];

        if (!jointBounds.isValid())
        {
            continue;
        }

        currentLocalBounds_.expand(
            jointBounds.transformed(
                matrices_[boundsIndex]));
    }

    if (!currentLocalBounds_.isValid())
    {
        clear();
        return false;
    }

    return true;
}

bool SkinningPalette::upload()
{
    uploaded_ = false;

    if (!gpuBuffer_.isValid() ||
        matrices_.empty() ||
        matrices_.size() != jointCapacity_)
    {
        return false;
    }

    if (!gpuBuffer_.update(
            0,
            std::span<const glm::mat4>{
                matrices_}))
    {
        return false;
    }

    uploaded_ = true;

    return true;
}

void SkinningPalette::clear() noexcept
{
    matrices_.clear();
    currentLocalBounds_.reset();
    uploaded_ = false;
}

bool SkinningPalette::empty() const noexcept
{
    return matrices_.empty();
}

std::size_t SkinningPalette::size() const noexcept
{
    return matrices_.size();
}

void SkinningPalette::bind(
    const std::uint32_t binding) const noexcept
{
    if (!isGpuReady())
    {
        return;
    }

    gpuBuffer_.bindShaderStorage(binding);
}

bool SkinningPalette::isGpuReady() const noexcept
{
    return
        gpuBuffer_.isValid() &&
        jointCapacity_ > 0 &&
        uploaded_;
}

const std::vector<glm::mat4>&
SkinningPalette::matrices() const noexcept
{
    return matrices_;
}

const math::Bounds&
SkinningPalette::currentLocalBounds() const noexcept
{
    return currentLocalBounds_;
}

} // namespace stylized::render
