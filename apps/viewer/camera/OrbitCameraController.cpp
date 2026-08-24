#include "OrbitCameraController.hpp"

#include <platform/Window.hpp>

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace
{

constexpr float orbitSensitivity = 0.005F;
constexpr float panSensitivity = 0.0015F;
constexpr float zoomSensitivity = 0.15F;

constexpr float minimumDistance = 0.05F;
constexpr float maximumDistance = 10000.0F;

constexpr float pitchLimit =
    glm::radians(89.0F);

} // namespace

OrbitCameraController::OrbitCameraController(stylized::scene::Camera& camera) noexcept
    : camera_(camera)
{
    updateCamera();
}

void OrbitCameraController::update(
    stylized::platform::Window& window,
    const bool inputEnabled)
{
    double cursorX = 0.0;
    double cursorY = 0.0;

    window.getCursorPosition(cursorX, cursorY);

    if (!hasPreviousCursor_)
    {
        previousCursorX_ = cursorX;
        previousCursorY_ = cursorY;
        hasPreviousCursor_ = true;
    }

    const float deltaX = static_cast<float>(cursorX - previousCursorX_);
    const float deltaY = static_cast<float>(cursorY - previousCursorY_);

    previousCursorX_ = cursorX;
    previousCursorY_ = cursorY;

    if (inputEnabled &&
        window.isMouseButtonPressed(
            stylized::platform::MouseButton::Left))
    {
        yaw_ -= deltaX * orbitSensitivity;
        pitch_ -= deltaY * orbitSensitivity;

        pitch_ = std::clamp(pitch_, -pitchLimit, pitchLimit);

    }

    if (inputEnabled &&
        window.isMouseButtonPressed(
            stylized::platform::MouseButton::Middle))
    {
        const glm::vec3 forward = glm::normalize(target_ - camera_.position());
        const glm::vec3 right = glm::normalize(glm::cross(forward, camera_.up()));
        const glm::vec3 cameraUp = glm::normalize(glm::cross(right, forward));

        const float movementScale = distance_ * panSensitivity;

        target_ += right * (-deltaX * movementScale) + cameraUp * (deltaY * movementScale);

    }

    const double scrollDelta = window.consumeScrollDelta();

    if (inputEnabled &&
        scrollDelta != 0.0)
    {
        distance_ *= std::exp(-static_cast<float>(scrollDelta) * zoomSensitivity);
        distance_ = std::clamp(distance_, minimumDistance, maximumDistance);

    }

    uint32_t framebufferWidth = 0;
    uint32_t framebufferHeight = 0;

    window.getFramebufferSize(framebufferWidth, framebufferHeight);

    if (framebufferHeight > 0)
    {
        const float aspectRatio = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);

        if (!camera_.setAspectRatio(aspectRatio)) return;
    }

    // Re-apply the orbit state every manual frame.  Besides keeping the
    // camera synchronized after an input change, this makes switching from
    // an imported camera to manual control immediate.
    updateCamera();
}

void OrbitCameraController::adoptCurrentView() noexcept
{
    const glm::vec3 offset =
        camera_.position() - camera_.target();

    const float distance =
        glm::length(offset);

    if (distance <= minimumDistance)
    {
        return;
    }

    const glm::vec3 direction =
        offset / distance;

    target_ = camera_.target();
    distance_ = std::clamp(
        distance,
        minimumDistance,
        maximumDistance);
    yaw_ = std::atan2(direction.x, direction.z);
    pitch_ = std::clamp(
        std::asin(std::clamp(direction.y, -1.0F, 1.0F)),
        -pitchLimit,
        pitchLimit);

    hasPreviousCursor_ = false;
}

void OrbitCameraController::focus(const stylized::math::Bounds& bounds) noexcept
{
    if (!bounds.isValid()) return;

    target_ = bounds.center();
    const float radius = glm::length(bounds.extent());
    
    if (radius > 0.f)
    {
        const float halfFieldOfView = glm::radians(camera_.verticalFieldOfView() * 0.5F);

        distance_ = radius / std::sin(halfFieldOfView);

        distance_ = std::clamp(distance_, minimumDistance, maximumDistance);
    }
    
    updateCamera();
}

void OrbitCameraController::setTarget(
    const glm::vec3& target) noexcept
{
    target_ = target;
    updateCamera();
}

void OrbitCameraController::setDistance(
    const float distance) noexcept
{
    distance_ = std::clamp(
        distance,
        minimumDistance,
        maximumDistance);

    updateCamera();
}

void OrbitCameraController::updateCamera() noexcept
{
    const float cosPitch = std::cos(pitch_);

    const glm::vec3 direction{
        cosPitch * std::sin(yaw_),
        std::sin(pitch_),
        cosPitch * std::cos(yaw_)
    };

    const glm::vec3 position =
        target_ + direction * distance_;

    if (!camera_.setView(
            position,
            target_,
            glm::vec3{0.0F, 1.0F, 0.0F}))
    {
        return;
    }
}
