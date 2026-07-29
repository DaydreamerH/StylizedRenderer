#pragma once

#include <stylized/math/Bounds.hpp>
#include <stylized/scene/Camera.hpp>

#include <glm/vec3.hpp>

namespace stylized::platform
{
class Window;
} // namespace stylized::platform

class OrbitCameraController
{
public:
    explicit OrbitCameraController(stylized::scene::Camera& camera) noexcept;

    void update(stylized::platform::Window& window);

    void focus(const stylized::math::Bounds& bounds) noexcept;

    void setTarget(const glm::vec3& target) noexcept;
    void setDistance(float distance) noexcept;

private:
    void updateCamera() noexcept;

    stylized::scene::Camera& camera_;

    glm::vec3 target_{0.0f};

    float distance_ = 3.f;
    float yaw_ = 0.f;
    float pitch_ = 0.f;

    double previousCursorX_ = 0.0;
    double previousCursorY_ = 0.0;

    bool hasPreviousCursor_ = false;
};