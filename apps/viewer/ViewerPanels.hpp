#pragma once

#include <core/NonCopyable.hpp>
#include <material/MaterialTemplate.hpp>

#include <cstddef>
#include <filesystem>

struct GLFWwindow;

namespace stylized::asset
{
class AssetRegistry;
struct SceneAsset;
}

namespace stylized::render
{
struct RenderWorld;
}

class ViewerPanels final : public stylized::core::NonCopyable
{
public:
    ViewerPanels() = default;
    ~ViewerPanels();

    [[nodiscard]] bool initialize(GLFWwindow* window);
    void beginFrame() noexcept;

    void draw(
        const std::filesystem::path& modelPath,
        const stylized::asset::AssetRegistry& assets,
        const stylized::asset::SceneAsset* scene,
        const stylized::render::RenderWorld& renderWorld,
        std::size_t drawCallCount,
        stylized::material::MaterialKind& materialKind,
        bool& shadowsEnabled,
        float& exposure,
        bool& toneMappingEnabled) const;

    void endFrame() noexcept;
    void shutdown() noexcept;

private:
    bool initialized_ = false;
};

