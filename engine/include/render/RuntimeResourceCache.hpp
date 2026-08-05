#pragma once

#include <asset/AssetHandle.hpp>
#include <core/NonCopyable.hpp>
#include <graphics/GraphicsDevice.hpp>
#include <graphics/Texture2D.hpp>
#include <render/RuntimeMesh.hpp>
#include <render/RuntimeMaterial.hpp>
#include <material/MaterialInstance.hpp>

#include <cstdint>
#include <unordered_map>
#include <cstddef>
#include <functional>

namespace stylized::asset
{

class AssetRegistry;
struct MeshAsset;
struct TextureAsset;
enum class TexturePixelFormat : std::uint8_t;
enum class ColorSpace : std::uint8_t;
struct MaterialAsset;

} // namespace stylized::asset

namespace stylized::render
{

class RuntimeResourceCache final : public core::NonCopyable
{
public:
    explicit RuntimeResourceCache(graphics::GraphicsDevice& graphicsDevice);
    ~RuntimeResourceCache();

    [[nodiscard]] bool initialize();
    
    [[nodiscard]] const RuntimeMesh* getOrCreateMesh(
        asset::AssetHandle<asset::MeshAsset> handle,
        const asset::AssetRegistry& assets);
    
    [[nodiscard]] const graphics::Texture2D& getOrCreateTexture(
        asset::AssetHandle<asset::TextureAsset> handle,
        const asset::AssetRegistry& assets
    );
    
    [[nodiscard]] const graphics::Texture2D& whiteTexture() const noexcept;

    [[nodiscard]] const graphics::Texture2D& errorTexture() const noexcept;

    [[nodiscard]] RuntimeMaterial* getOrCreateRuntimeMaterial(
        asset::AssetHandle<
            material::MaterialTemplate> templateHandle,
        const asset::AssetRegistry& assets
    );

    [[nodiscard]] const material::MaterialInstance* getOrCreateMaterialInstance(
        asset::AssetHandle<asset::MaterialAsset> materialHandle,
        asset::AssetHandle<material::MaterialTemplate> templateHandle,
        const asset::AssetRegistry& assets
    );

    void clear() noexcept;
    
private:
    struct MaterialInstanceKey
    {
        std::uint64_t materialId = 0;
        std::uint64_t templateId = 0;

        [[nodiscard]] bool operator==(
            const MaterialInstanceKey&) const noexcept = default;
    };

    struct MaterialInstanceKeyHash
    {
        [[nodiscard]] std::size_t operator()(
            const MaterialInstanceKey& key) const noexcept
        {
            const std::size_t materialHash =
                std::hash<std::uint64_t>{}(
                    key.materialId);

            const std::size_t templateHash =
                std::hash<std::uint64_t>{}(
                    key.templateId);

            return materialHash ^
                (templateHash << 1U);
        }
    };

    [[nodiscard]] graphics::TextureFormat toGraphicsTextureFormat(
        const asset::TexturePixelFormat format,
        const asset::ColorSpace colorSpace) const noexcept;

    [[nodiscard]] graphics::Texture2D createWhiteTexture();

    [[nodiscard]] graphics::Texture2D createErrorTexture();

    [[nodiscard]] graphics::Texture2D uploadTexture(const asset::TextureAsset& textureAsset);

    graphics::GraphicsDevice& graphicsDevice_;

    std::unordered_map<std::uint64_t, RuntimeMesh> meshes_;
    std::unordered_map<std::uint64_t, graphics::Texture2D> textures_;
    std::unordered_map<std::uint64_t, RuntimeMaterial> runtimeMaterials_;

    graphics::Texture2D whiteTexture_;
    graphics::Texture2D errorTexture_;

    std::unordered_map<
        MaterialInstanceKey,
        material::MaterialInstance,
        MaterialInstanceKeyHash>
        materialInstances_;

    bool initialized_ = false;
};

} // namespace stylized::render
