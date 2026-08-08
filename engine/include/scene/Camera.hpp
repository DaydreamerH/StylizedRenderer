#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace stylized::scene
{
    
class Camera
{
public:
    Camera() noexcept = default;
    Camera(float verticalFieldOfView, float aspectRatio, float nearPlane, float farPlane) noexcept;

    [[nodiscard]] bool setPerspective(float verticalFieldOfView, float aspectRatio, float nearPlane, float farPlane) noexcept;
    [[nodiscard]] bool setView(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) noexcept;
    [[nodiscard]] bool setAspectRatio(float aspectRatio) noexcept;

    [[nodiscard]] float verticalFieldOfView() const noexcept;
    [[nodiscard]] float aspectRatio() const noexcept;
    [[nodiscard]] float nearPlane() const noexcept;
    [[nodiscard]] float farPlane() const noexcept;

    [[nodiscard]] const glm::vec3& position() const noexcept;
    [[nodiscard]] const glm::vec3& target() const noexcept;
    [[nodiscard]] const glm::vec3& up() const noexcept;

    [[nodiscard]] glm::mat4 viewMatrix() const noexcept;
    [[nodiscard]] glm::mat4 projectionMatrix() const noexcept;
    [[nodiscard]] glm::mat4 viewProjectionMatrix() const noexcept;

private:
    float verticalFieldOfView_ = 60.0F;
    float aspectRatio_ = 16.0F / 9.0F;
    float nearPlane_ = 0.1F;
    float farPlane_ = 2000.0F;

    glm::vec3 position_{0.0F, 0.0F, 3.0F};
    glm::vec3 target_{0.0F, 0.0F, 0.0F};
    glm::vec3 up_{0.0F, 1.0F, 0.0F};
};

} // namespace stylized::scene
