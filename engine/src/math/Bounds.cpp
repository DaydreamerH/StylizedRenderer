#include <math/Bounds.hpp>

#include <glm/common.hpp>
#include <glm/vec4.hpp>

namespace stylized::math
{

Bounds::Bounds(const glm::vec3& minimum, const glm::vec3& maximum) noexcept
    : minimum_(minimum), maximum_(maximum)
{
}

bool Bounds::isValid() const noexcept
{
    return minimum_.x <= maximum_.x && minimum_.y <= maximum_.y && minimum_.z <= maximum_.z;
}

const glm::vec3& Bounds::minimum() const noexcept
{
    return minimum_;
}

const glm::vec3& Bounds::maximum() const noexcept
{
    return maximum_;
}

glm::vec3 Bounds::center() const noexcept
{
    if (!isValid()) return glm::vec3{0.f};

    return (minimum_ + maximum_) * 0.5f;
}

glm::vec3 Bounds::extent() const noexcept
{
    if (!isValid()) return glm::vec3(0.f);

    return (maximum_ - minimum_) * 0.5f;
}

glm::vec3 Bounds::size() const noexcept
{
    if (!isValid()) return glm::vec3(0.f);

    return maximum_ - minimum_;
}

void Bounds::reset() noexcept
{
    minimum_ = glm::vec3{
        std::numeric_limits<float>::max()
    };

    maximum_ = glm::vec3{
        std::numeric_limits<float>::lowest()
    };
}

void Bounds::expand(const glm::vec3& point) noexcept
{
    minimum_ = glm::min(minimum_, point);
    maximum_ = glm::max(maximum_, point);
}

void Bounds::expand(const Bounds& bounds) noexcept
{
    if (!bounds.isValid())
    {
        return;
    }

    expand(bounds.minimum());
    expand(bounds.maximum());
}

Bounds Bounds::transformed(const glm::mat4& transform) const noexcept
{
    if (!isValid()) return {};

    const std::array<glm::vec3, 8> corners{
        glm::vec3{minimum_.x, minimum_.y, minimum_.z},
        glm::vec3{maximum_.x, minimum_.y, minimum_.z},
        glm::vec3{minimum_.x, maximum_.y, minimum_.z},
        glm::vec3{maximum_.x, maximum_.y, minimum_.z},
        glm::vec3{minimum_.x, minimum_.y, maximum_.z},
        glm::vec3{maximum_.x, minimum_.y, maximum_.z},
        glm::vec3{minimum_.x, maximum_.y, maximum_.z},
        glm::vec3{maximum_.x, maximum_.y, maximum_.z}
    };

    Bounds result;

    for (const glm::vec3& corner : corners)
    {
        const glm::vec4 transformedCorner =
            transform * glm::vec4{corner, 1.0F};

        result.expand(glm::vec3{transformedCorner});
    }

    return result;
}

} // namespace stylized::math