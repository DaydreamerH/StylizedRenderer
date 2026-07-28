#include <stylized/scene/Camera.hpp>

#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace stylized::scene
{
    
namespace 
{
    constexpr float minimumFieldOfView = 1.0F;
    constexpr float maximumFieldOfView = 179.0F;
    constexpr float directionEpsilon = 0.000001F;
} // namespace 

Camera::Camera(const float verticalFieldOfView, const float aspectRatio, const float nearPlane, const float farPlane) noexcept
{
    if (!setPerspective(verticalFieldOfView, aspectRatio, nearPlane, farPlane))
    {
        return;
    }
}

bool Camera::setPerspective(const float verticalFieldOfView, const float aspectRatio, const float nearPlane, const float farPlane) noexcept
{
    if (!std::isfinite(verticalFieldOfView) ||
        !std::isfinite(aspectRatio) ||
        !std::isfinite(nearPlane) ||
        !std::isfinite(farPlane)) 
        return false;
    
    if (verticalFieldOfView < minimumFieldOfView ||
        verticalFieldOfView > maximumFieldOfView) 
        return false;

    if (aspectRatio <= 0.0F ||
        nearPlane <= 0.0F ||
        farPlane <= nearPlane)
        return false;

    verticalFieldOfView_ = verticalFieldOfView;
    aspectRatio_ = aspectRatio;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;

    return true;
}

bool Camera::setView(
    const glm::vec3& position,
    const glm::vec3& target,
    const glm::vec3& up) noexcept
{
    const glm::vec3 viewDirection = target - position;

    if (glm::dot(viewDirection, viewDirection) <= directionEpsilon)
    {
        return false;
    }

    if (glm::dot(up, up) <= directionEpsilon)
    {
        return false;
    }

    const glm::vec3 side = glm::cross(viewDirection, up);

    if (glm::dot(side, side) <= directionEpsilon)
    {
        return false;
    }

    position_ = position;
    target_ = target;
    up_ = glm::normalize(up);

    return true;
}

bool Camera::setAspectRatio(
    const float aspectRatio) noexcept
{
    if (!std::isfinite(aspectRatio) ||
        aspectRatio <= 0.0F)
    {
        return false;
    }

    aspectRatio_ = aspectRatio;

    return true;
}

float Camera::verticalFieldOfView() const noexcept
{
    return verticalFieldOfView_;
}

float Camera::aspectRatio() const noexcept
{
    return aspectRatio_;
}

float Camera::nearPlane() const noexcept
{
    return nearPlane_;
}

float Camera::farPlane() const noexcept
{
    return farPlane_;
}

const glm::vec3& Camera::position() const noexcept
{
    return position_;
}

const glm::vec3& Camera::target() const noexcept
{
    return target_;
}

const glm::vec3& Camera::up() const noexcept
{
    return up_;
}

glm::mat4 Camera::viewMatrix() const noexcept
{
    return glm::lookAtRH(
        position_,
        target_,
        up_);
}

glm::mat4 Camera::projectionMatrix() const noexcept
{
    return glm::perspectiveRH_NO(
        glm::radians(verticalFieldOfView_),
        aspectRatio_,
        nearPlane_,
        farPlane_);
}

glm::mat4 Camera::viewProjectionMatrix() const noexcept
{
    return projectionMatrix() * viewMatrix();
}


} // namespace stylized::scene
