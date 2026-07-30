#pragma once

#include <array>
#include <limits>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace stylized::math
{
    
class Bounds
{
public:
    Bounds() noexcept = default;
    Bounds(const glm::vec3& minimum, const glm::vec3& maximum) noexcept;

    [[nodiscard]] bool isValid() const noexcept;

    [[nodiscard]] const glm::vec3& minimum() const noexcept;
    [[nodiscard]] const glm::vec3& maximum() const noexcept;

    [[nodiscard]] glm::vec3 center() const noexcept;
    [[nodiscard]] glm::vec3 extent() const noexcept;
    [[nodiscard]] glm::vec3 size() const noexcept;

    void reset() noexcept;

    void expand(const glm::vec3& point) noexcept;
    void expand(const Bounds& bounds) noexcept;

    [[nodiscard]] Bounds transformed(const glm::mat4& transform) const noexcept;

private:
    glm::vec3 minimum_{std::numeric_limits<float>::max()};
    glm::vec3 maximum_{std::numeric_limits<float>::lowest()};
};

} // namespace styliezed::math