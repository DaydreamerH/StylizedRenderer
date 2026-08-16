#pragma once

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

namespace stylized::render
{

class SkinningPalette final
{
public:
    [[nodiscard]] bool update(
        const asset::SkinAsset& skin,
        std::uint32_t meshNodeIndex,
        const animation::ScenePose& pose);

    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] const std::vector<glm::mat4>&
        matrices() const noexcept;

private:
    std::vector<glm::mat4> matrices_;
};

} // namespace stylized::render

