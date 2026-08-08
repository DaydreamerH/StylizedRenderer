#pragma once

#include <asset/AssetHandle.hpp>
#include <core/NonCopyable.hpp>
#include <graphics/ShaderProgram.hpp>
#include <material/MaterialTemplate.hpp>

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
    
struct MaterialInstance;

} // namespace stylized::material


namespace stylized::render
{
    
class RuntimeResourceCache;

class RuntimeMaterial final : public core::NonCopyable
{
public:
    RuntimeMaterial() = default;
    RuntimeMaterial(
        graphics::GraphicsDevice& graphicsDevice,
        const asset::AssetRegistry& assetRegistry,
        asset::AssetHandle<material::MaterialTemplate> templateHandle
    );
    
    RuntimeMaterial(RuntimeMaterial&&) noexcept = default;
    RuntimeMaterial& operator=(RuntimeMaterial&&) noexcept = default;

    ~RuntimeMaterial() = default;

    [[nodiscard]] bool isValid() const noexcept;
    
    [[nodiscard]] material::MaterialKind
    kind() const noexcept;

    [[nodiscard]] asset::AssetHandle<material::MaterialTemplate>
    templateHandle() const noexcept;

    [[nodiscard]] graphics::ShaderProgram* shader() noexcept;

    [[nodiscard]] const graphics::ShaderProgram* 
    shader() const noexcept;

    bool bind(
        const material::MaterialInstance& instance,
        RuntimeResourceCache& resourceCache,
        const asset::AssetRegistry& assetRegistry    
    );

private:
    asset::AssetHandle<material::MaterialTemplate> templateHandle_;

    material::MaterialKind kind_ = material::MaterialKind::Unlit;

    graphics::ShaderProgram shader_;

};

} // namespace stylized::render
