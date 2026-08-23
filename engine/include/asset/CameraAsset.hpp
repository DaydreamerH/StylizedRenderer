#pragma once

#include <cstdint>
#include <limits>
#include <string>

#include <glm/vec3.hpp>

namespace stylized::asset
{

struct CameraAsset
{
    static constexpr std::uint32_t invalidNodeIndex =
        std::numeric_limits<std::uint32_t>::max();

    std::string name;
    std::uint32_t nodeIndex = invalidNodeIndex;

    float verticalFieldOfView = 60.0F;
    float aspectRatio = 0.0F;
    float nearPlane = 0.1F;
    float farPlane = 2000.0F;

    glm::vec3 localPosition{0.0F};
    glm::vec3 localForward{0.0F, 0.0F, -1.0F};
    glm::vec3 localUp{0.0F, 1.0F, 0.0F};
};

} // namespace stylized::asset
