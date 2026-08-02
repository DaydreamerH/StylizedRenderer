#include <math/Frustum.hpp>

#include <glm/geometric.hpp>

#include <cmath>
#include <limits>

namespace stylized::math
{
    
namespace
{
constexpr float minimumPlaneLength = 1.0e-6F;

FrustumPlane makePlane(const glm::vec4& equation) noexcept
{
    const glm::vec3 normal{
        equation.x,
        equation.y,
        equation.z
    };

    const float length = glm::length(normal);

    if (!std::isfinite(length) || length <= minimumPlaneLength) return {};

    const float inverseLength = 1.F / length;

    return FrustumPlane{normal * inverseLength, equation.w * inverseLength};
}

glm::vec4 matrixRow(
    const glm::mat4& matrix,
    const glm::mat4::length_type row) noexcept
{
    return glm::vec4{
        matrix[0][row],
        matrix[1][row],
        matrix[2][row],
        matrix[3][row]
    };
}

} // namespace

bool FrustumPlane::isValid() const noexcept
{
    return
        std::isfinite(normal.x) &&
        std::isfinite(normal.y) &&
        std::isfinite(normal.z) &&
        std::isfinite(distance) &&
        glm::length(normal) > minimumPlaneLength;
}

float FrustumPlane::signedDistance(const glm::vec3& point) const noexcept
{
    return glm::dot(normal, point) + distance;
}

Frustum Frustum::fromViewProjection(
    const glm::mat4& viewProjection) noexcept
{
    Frustum frustum;

    const glm::vec4 row0 =
        matrixRow(viewProjection, 0);

    const glm::vec4 row1 =
        matrixRow(viewProjection, 1);

    const glm::vec4 row2 =
        matrixRow(viewProjection, 2);

    const glm::vec4 row3 =
        matrixRow(viewProjection, 3);

    frustum.planes_[
        static_cast<std::size_t>(
            PlaneIndex::Left)] =
        makePlane(row3 + row0);

    frustum.planes_[
        static_cast<std::size_t>(
            PlaneIndex::Right)] =
        makePlane(row3 - row0);

    frustum.planes_[
        static_cast<std::size_t>(
            PlaneIndex::Bottom)] =
        makePlane(row3 + row1);

    frustum.planes_[
        static_cast<std::size_t>(
            PlaneIndex::Top)] =
        makePlane(row3 - row1);

    frustum.planes_[
        static_cast<std::size_t>(
            PlaneIndex::Near)] =
        makePlane(row3 + row2);

    frustum.planes_[
        static_cast<std::size_t>(
            PlaneIndex::Far)] =
        makePlane(row3 - row2);

    return frustum;
}

bool Frustum::isValid() const noexcept
{
    for (const FrustumPlane& plane : planes_)
    {
        if (!plane.isValid())
        {
            return false;
        }
    }

    return true;
}

bool Frustum::contains(const glm::vec3& point) const noexcept
{
    if (!isValid())
    {
        return false;
    }

    for (const FrustumPlane& plane : planes_)
    {
        if (plane.signedDistance(point) < 0.0F)
        {
            return false;
        }
    }

    return true;
}

bool Frustum::intersects(const Bounds& bounds) const noexcept
{
    if (!isValid() ||!bounds.isValid())
    {
        return false;
    }

    const glm::vec3 minimum = bounds.minimum();
    const glm::vec3 maximum = bounds.maximum();

    for (const FrustumPlane& plane : planes_)
    {
        const glm::vec3 positiveVertex{
            plane.normal.x >= 0.0F ? maximum.x : minimum.x,
            plane.normal.y >= 0.0F ? maximum.y : minimum.y,
            plane.normal.z >= 0.0F ? maximum.z : minimum.z
        };

        if (plane.signedDistance(positiveVertex) < 0.0F)
        {
            return false;
        }
    }

    return true;
}

} // namespace stylized::math
