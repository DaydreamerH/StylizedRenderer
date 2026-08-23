#pragma once

#include <glm/mat4x4.hpp>

namespace stylized::animation
{
class ScenePose;
}

namespace stylized::asset
{
struct CameraAsset;
}

namespace stylized::scene
{
class Camera;
}

class SceneCameraController
{
public:
    [[nodiscard]] bool update(
        const stylized::asset::CameraAsset& cameraAsset,
        const stylized::animation::ScenePose& scenePose,
        const glm::mat4& instanceWorldMatrix,
        stylized::scene::Camera& camera) const noexcept;
};