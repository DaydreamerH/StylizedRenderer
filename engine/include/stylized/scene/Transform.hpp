#pragma once

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace stylized::scene
{

class Transform
{
public:
    Transform() noexcept = default;

    Transform(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale) noexcept;

    [[nodiscard]] const glm::vec3& translation() const noexcept;
    [[nodiscard]] const glm::quat& rotation() const noexcept;
    [[nodiscard]] const glm::vec3& scale() const noexcept;

    void setTranslation(const glm::vec3& translation) noexcept;
    [[nodiscard]] bool setRotation(const glm::quat& rotation) noexcept;
    void setScale(const glm::vec3& scale) noexcept;

    [[nodiscard]] glm::mat4 localMatrix() const noexcept;

private:
    glm::vec3 translation_{0.0F};

    glm::quat rotation_{1.F, 0.F, 0.F, 0.F};

    glm::vec3 scale_{1.F};
};

} // namespace stylized::scene
