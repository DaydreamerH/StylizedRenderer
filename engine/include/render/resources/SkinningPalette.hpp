#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/resources/Buffer.hpp>
#include <math/Bounds.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>

namespace stylized::animation
{

class ScenePose;

} // namespace stylized::animation

namespace stylized::asset
{

struct SkinAsset;

} // namespace stylized::asset

namespace stylized::graphics
{

class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{

class SkinningPalette final : public core::NonCopyable
{
public:
    SkinningPalette() = default;

    SkinningPalette(SkinningPalette&& other) noexcept = default;

    SkinningPalette& operator=(
        SkinningPalette&& other) noexcept = default;

    ~SkinningPalette() = default;

    [[nodiscard]] bool initializeGpuBuffer(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::SkinAsset& skin);

    [[nodiscard]] bool update(
        const asset::SkinAsset& skin,
        std::uint32_t meshNodeIndex,
        const animation::ScenePose& pose);

    [[nodiscard]] bool upload();

    void bind(
        std::uint32_t binding) const noexcept;

    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] bool isGpuReady() const noexcept;

    [[nodiscard]] const std::vector<glm::mat4>&
        matrices() const noexcept;

    [[nodiscard]] const math::Bounds&
        currentLocalBounds() const noexcept;

private:
    std::vector<glm::mat4> matrices_;

    graphics::Buffer gpuBuffer_;

    std::size_t jointCapacity_ = 0;

    bool uploaded_ = false;

    math::Bounds currentLocalBounds_;
};

} // namespace stylized::render

