#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <cmath>
#include <numbers>
#include <string>
#include <utility>

#include <assimp/camera.h>
#include <assimp/scene.h>

#include <glm/geometric.hpp>

namespace stylized::asset::importers::detail
{

namespace
{

constexpr float directionEpsilon = 1.0e-8F;

[[nodiscard]] bool isFinite(
    const aiVector3D& value) noexcept
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

[[nodiscard]] bool convertCamera(
    const aiCamera& source,
    const StagedScene& scene,
    CameraAsset& destination) noexcept
{
    // The current renderer only exposes a perspective projection.
    if (source.mOrthographicWidth > 0.0F)
    {
        return false;
    }

    std::uint32_t nodeIndex =
        SceneNodeAsset::invalidNodeIndex;

    if (findSceneNodeIndex(
            scene,
            source.mName.C_Str(),
            nodeIndex) != SceneNodeLookupResult::Found)
    {
        return false;
    }

    if (!isFinite(source.mPosition) ||
        !isFinite(source.mLookAt) ||
        !isFinite(source.mUp) ||
        !std::isfinite(source.mHorizontalFOV) ||
        !std::isfinite(source.mAspect) ||
        !std::isfinite(source.mClipPlaneNear) ||
        !std::isfinite(source.mClipPlaneFar) ||
        source.mHorizontalFOV <= 0.0F ||
        source.mHorizontalFOV >= std::numbers::pi_v<float> ||
        source.mAspect < 0.0F ||
        source.mClipPlaneNear <= 0.0F ||
        source.mClipPlaneFar <= source.mClipPlaneNear)
    {
        return false;
    }

    const glm::vec3 localForward{
        source.mLookAt.x,
        source.mLookAt.y,
        source.mLookAt.z};

    const glm::vec3 localUp{
        source.mUp.x,
        source.mUp.y,
        source.mUp.z};

    if (glm::dot(localForward, localForward) <=
            directionEpsilon ||
        glm::dot(localUp, localUp) <= directionEpsilon ||
        glm::dot(
            glm::cross(localForward, localUp),
            glm::cross(localForward, localUp)) <=
                directionEpsilon)
    {
        return false;
    }

    const float fieldOfViewAspect =
        source.mAspect > 0.0F
            ? source.mAspect
            : 1.0F;

    const float verticalFieldOfViewRadians =
        2.0F * std::atan(
            std::tan(source.mHorizontalFOV * 0.5F) /
            fieldOfViewAspect);

    const float verticalFieldOfView =
        verticalFieldOfViewRadians *
        180.0F /
        std::numbers::pi_v<float>;

    if (!std::isfinite(verticalFieldOfView) ||
        verticalFieldOfView <= 0.0F ||
        verticalFieldOfView >= 180.0F)
    {
        return false;
    }

    destination.name = source.mName.C_Str();
    destination.nodeIndex = nodeIndex;
    destination.verticalFieldOfView =
        verticalFieldOfView;
    destination.aspectRatio = source.mAspect;
    destination.nearPlane = source.mClipPlaneNear;
    destination.farPlane = source.mClipPlaneFar;
    destination.localPosition = {
        source.mPosition.x,
        source.mPosition.y,
        source.mPosition.z};
    destination.localForward =
        glm::normalize(localForward);
    destination.localUp = glm::normalize(localUp);

    return true;
}

} // namespace

bool stageCameras(
    const aiScene& importedScene,
    StagedScene& scene)
{
    scene.asset.cameras.clear();
    scene.asset.cameras.reserve(
        importedScene.mNumCameras);

    if (importedScene.mNumCameras > 0 &&
        importedScene.mCameras == nullptr)
    {
        return false;
    }

    for (unsigned int cameraIndex = 0;
         cameraIndex < importedScene.mNumCameras;
         ++cameraIndex)
    {
        const aiCamera* source =
            importedScene.mCameras[cameraIndex];

        if (source == nullptr)
        {
            return false;
        }

        // Orthographic cameras are deliberately ignored until the runtime
        // Camera exposes an orthographic projection.
        if (source->mOrthographicWidth > 0.0F)
        {
            continue;
        }

        CameraAsset camera;

        if (!convertCamera(
                *source,
                scene,
                camera))
        {
            return false;
        }

        scene.asset.cameras.push_back(
            std::move(camera));
    }

    return true;
}

} // namespace stylized::asset::importers::detail
