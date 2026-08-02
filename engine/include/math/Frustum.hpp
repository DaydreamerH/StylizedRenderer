#pragma once

#include <math/Bounds.hpp>

#include <array>
#include <cstddef>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace stylized::math
{
    
struct FrustumPlane
{
    glm::vec3 normal{0.0F};
    float distance = 0.0F;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] float signedDistance(const glm::vec3& point) const noexcept;
};

class Frustum
{
public:
    enum class PlaneIndex : std::size_t
    {
        Left = 0,
        Right,
        Bottom,
        Top,
        Near,
        Far,
        Count
    };

    Frustum() noexcept = default;

    [[nodiscard]] static Frustum fromViewProjection(const glm::mat4& viewProjection) noexcept;
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool contains(const glm::vec3& point) const noexcept;
    [[nodiscard]] bool intersects(const Bounds& bounds) const noexcept;

private:
    static constexpr std::size_t planeCount = static_cast<std::size_t>(PlaneIndex::Count);
    std::array<FrustumPlane, planeCount> planes_{};
};


} // namespace stylized::math
