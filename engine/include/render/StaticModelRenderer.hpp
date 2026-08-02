#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/ShaderProgram.hpp>
#include <render/RenderWorld.hpp>

namespace stylized::asset
{

class AssetRegistry;

} // namespace stylized::asset

namespace stylized::graphics
{
    
class GraphicsDevice;    

} // namespace stylized::graphics


namespace stylized::render
{

class RuntimeResourceCache;

class StaticModelRenderer final : public core::NonCopyable
{
private:
    graphics::GraphicsDevice& graphicsDevice_;
    const asset::AssetRegistry& assetRegistry_;
    RuntimeResourceCache& resourceCache_;

    graphics::ShaderProgram shader_;
    bool initialized_ = false;

public:
    StaticModelRenderer(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::AssetRegistry& assetRegistry,
        RuntimeResourceCache& resourceCache
    ) noexcept;


    ~StaticModelRenderer() = default;

    [[nodiscard]] bool initialize();

    [[nodiscard]] bool render(const RenderWorld& renderWorld);
};

} // namespace stylized::render
