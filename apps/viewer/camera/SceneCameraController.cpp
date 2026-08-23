#include "SceneCameraController.hpp"

#include <animation/ScenePose.hpp>
#include <asset/CameraAsset.hpp>
#include <scene/Camera.hpp>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec4.hpp>

namespace
{

constexpr float directionEpsilon = 1.0e-8F;

} // namespace

bool SceneCameraController::update(
    const stylized::asset::CameraAsset& cameraAsset,
    const stylized::animation::ScenePose& scenePose,
    const glm::mat4& instanceWorldMatrix,
    stylized::scene::Camera& camera) const noexcept
{
    const glm::mat4* nodeWorldMatrix =
        scenePose.worldMatrix(cameraAsset.nodeIndex);

    if (nodeWorldMatrix == nullptr)
    {
        return false;
    }

    const glm::mat4 worldMatrix =
        instanceWorldMatrix *
        *nodeWorldMatrix;

    const glm::vec3 position =
        glm::vec3(
            worldMatrix *
            glm::vec4(
                cameraAsset.localPosition,
                1.0F));

    const glm::mat3 worldBasis{worldMatrix};

    glm::vec3 forward =
        worldBasis * cameraAsset.localForward;

    glm::vec3 upCandidate =
        worldBasis * cameraAsset.localUp;

    if (glm::dot(forward, forward) <= directionEpsilon ||
        glm::dot(upCandidate, upCandidate) <= directionEpsilon)
    {
        return false;
    }

    forward = glm::normalize(forward);
    upCandidate = glm::normalize(upCandidate);

    glm::vec3 right =
        glm::cross(forward, upCandidate);

    if (glm::dot(right, right) <= directionEpsilon)
    {
        return false;
    }

    right = glm::normalize(right);

    const glm::vec3 up =
        glm::normalize(
            glm::cross(right, forward));

    if (!camera.setPerspective(
            cameraAsset.verticalFieldOfView,
            camera.aspectRatio(),
            cameraAsset.nearPlane,
            cameraAsset.farPlane))
    {
        return false;
    }

    return camera.setView(
        position,
        position + forward,
        up);
}