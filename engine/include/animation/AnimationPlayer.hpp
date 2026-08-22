#pragma once

namespace stylized::asset
{

struct AnimationClipAsset;
struct SceneAsset;

} // namespace stylized::asset

namespace stylized::animation
{

class ScenePose;
class SceneMorphPose;

class AnimationPlayer final
{
public:
    [[nodiscard]] bool setClip(
        const asset::AnimationClipAsset* clip) noexcept;

    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;

    void setLooping(bool looping) noexcept;

    void setPlaybackSpeed(float speed) noexcept;

    void seek(float timeSeconds) noexcept;

    [[nodiscard]] bool update(
        float deltaTime,
        const asset::SceneAsset& sceneAsset,
        ScenePose& pose,
        SceneMorphPose& morphPose) noexcept;

    [[nodiscard]] const asset::AnimationClipAsset*
        clip() const noexcept;

    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] bool isLooping() const noexcept;

    [[nodiscard]] float currentTime() const noexcept;
    [[nodiscard]] float playbackSpeed() const noexcept;

private:
    [[nodiscard]] bool sample(
        ScenePose& pose,
        SceneMorphPose& morphPose) noexcept;

    const asset::AnimationClipAsset* clip_ = nullptr;

    const asset::SceneAsset* validatedScene_ = nullptr;

    float currentTime_ = 0.0F;
    float playbackSpeed_ = 1.0F;

    bool playing_ = false;
    bool looping_ = true;

    bool samplePending_ = false;
    bool resetPoseOnNextSample_ = false;
};

} // namespace stylized::animation
