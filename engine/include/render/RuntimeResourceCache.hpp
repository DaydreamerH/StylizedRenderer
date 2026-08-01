#pragma once

#include <asset/AssetHandle.hpp>
#include <core/NonCopyable.hpp>
#include <graphics/GraphicsDevice.hpp>
#include <graphics/Texture2D.hpp>
#include <render/RuntimeMesh.hpp>

#include <cstdint>
#include <unordered_map>

namespace stylized::asset
{

class AssetRegistry;
struct MeshAsset;
struct TextureAsset;
enum class TexturePixelFormat : std::uint8_t;
enum class ColorSpace : std::uint8_t;

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

    void clear() noexcept;
    
private:
    [[nodiscard]] graphics::TextureFormat toGraphicsTextureFormat(
        const asset::TexturePixelFormat format,
        const asset::ColorSpace colorSpace) const noexcept;

    [[nodiscard]] graphics::Texture2D createWhiteTexture();

    [[nodiscard]] graphics::Texture2D createErrorTexture();

    [[nodiscard]] graphics::Texture2D uploadTexture(const asset::TextureAsset& textureAsset);

    graphics::GraphicsDevice& graphicsDevice_;

    std::unordered_map<std::uint64_t, RuntimeMesh> meshes_;
    std::unordered_map<std::uint64_t, graphics::Texture2D> textures_;

    graphics::Texture2D whiteTexture_;
    graphics::Texture2D errorTexture_;

    bool initialized_ = false;
};

} // namespace stylized::render
