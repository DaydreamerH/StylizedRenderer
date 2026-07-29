#include <stylized/scene/Transform.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace stylized::scene
{

namespace 
{
    constexpr float minimumQuaternionLengthSquared = 0.0001F;
} // namespace 

Transform::Transform(
    const glm::vec3& translation,
    const glm::quat& rotation,
    const glm::vec3& scale) noexcept
    : translation_(translation),
      scale_(scale)
{
    if (!setRotation(rotation))
    {
        rotation_ = glm::quat{
            1.0F,
            0.0F,
            0.0F,
            0.0F
        };
    }
}

const glm::vec3&
Transform::translation() const noexcept
{
    return translation_;
}

const glm::quat&
Transform::rotation() const noexcept
{
    return rotation_;
}

const glm::vec3&
Transform::scale() const noexcept
{
    return scale_;
}

void Transform::setTranslation(
    const glm::vec3& translation) noexcept
{
    translation_ = translation;
}

bool Transform::setRotation(
    const glm::quat& rotation) noexcept
{
    const float lengthSquared =
        glm::dot(rotation, rotation);

    if (!std::isfinite(lengthSquared) || lengthSquared <= minimumQuaternionLengthSquared)
    {
        return false;
    }

    rotation_ = glm::normalize(rotation);

    return true;
}

void Transform::setScale(
    const glm::vec3& scale) noexcept
{
    scale_ = scale;
}

glm::mat4 Transform::localMatrix() const noexcept
{
    const glm::mat4 translationMatrix =
        glm::translate(
            glm::mat4{1.0F},
            translation_);

    const glm::mat4 rotationMatrix =
        glm::mat4_cast(rotation_);

    const glm::mat4 scaleMatrix =
        glm::scale(
            glm::mat4{1.0F},
            scale_);

    return translationMatrix *
           rotationMatrix *
           scaleMatrix;
}

} // namespace stylized::scene
