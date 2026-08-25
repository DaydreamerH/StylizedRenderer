#pragma once

#include <glm/vec3.hpp>

#include <optional>
#include <string>

namespace stylized::material
{

struct ScreenOutlineMaterialParameters
{
    std::string group;

    bool enabled = true;
    bool depthEnabled = true;
    bool normalEnabled = true;
    bool detectSelfDepth = true;
    bool detectSelfNormal = true;

    std::optional<float> screenWidth;
    std::optional<float> depthThreshold;
    std::optional<float> normalThreshold;
    std::optional<glm::vec3> color;
};

} // namespace stylized::material
