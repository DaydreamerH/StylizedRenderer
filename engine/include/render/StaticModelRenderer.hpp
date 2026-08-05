#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/ShaderProgram.hpp>
#include <render/RenderWorld.hpp>
#include <asset/AssetHandle.hpp>

namespace stylized::asset
{

class AssetRegistry;

} // namespace stylized::asset

namespace stylized::graphics
{
    
class GraphicsDevice;    

} // namespace stylized::graphics

namespace stylized::material
{

struct MaterialTemplate;

} // namespace stylized::material



namespace stylized::render
{

class RuntimeResourceCache;

class StaticModelRenderer final : public core::NonCopyable
{
private:
    graphics::GraphicsDevice& graphicsDevice_;
    const asset::AssetRegistry& assetRegistry_;
    RuntimeResourceCache& resourceCache_;

    asset::AssetHandle<material::MaterialTemplate> materialTemplate_;

    graphics::ShaderProgram shader_;
    bool initialized_ = false;

    std::size_t lastDrawCallCount_ = 0;

public:
    StaticModelRenderer(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::AssetRegistry& assetRegistry,
        RuntimeResourceCache& resourceCache,
        const asset::AssetHandle<
            material::MaterialTemplate>
                materialTemplate
    ) noexcept;


    ~StaticModelRenderer() = default;

    [[nodiscard]] bool initialize();

    [[nodiscard]] bool render(const RenderWorld& renderWorld);

    [[nodiscard]] std::size_t lastDrawCallCount() const noexcept;
};

} // namespace stylized::render
