#include <render/resources/RuntimeResourceCache.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/MeshAsset.hpp>
#include <asset/TextureAsset.hpp>
#include <asset/MaterialAsset.hpp>

#include <material/MaterialInstance.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <utility>

namespace stylized::render
{
    
RuntimeResourceCache::RuntimeResourceCache(graphics::GraphicsDevice& graphicsDevice)
    : graphicsDevice_(graphicsDevice)
{}

RuntimeResourceCache::~RuntimeResourceCache()
{
    clear();
}

bool RuntimeResourceCache::initialize()
{
    if (initialized_)
    {
        return whiteTexture_.isValid() &&
            blackTexture_.isValid() &&
            neutralNormalTexture_.isValid() &&
            errorTexture_.isValid();
    }

    whiteTexture_ = createWhiteTexture();
    blackTexture_ = createBlackTexture();
    neutralNormalTexture_ = createNeutralNormalTexture();
    errorTexture_ = createErrorTexture();

    initialized_ = true;

    return whiteTexture_.isValid() &&
        blackTexture_.isValid() &&
        neutralNormalTexture_.isValid() &&
        errorTexture_.isValid();
}

const RuntimeMesh*
RuntimeResourceCache::getOrCreateMesh(
    const asset::AssetHandle<asset::MeshAsset> handle,
    const asset::AssetRegistry& assets
)
{
    if (handle.isNull()) return nullptr;

    const std::uint64_t key = handle.id().value;

    const auto existing = meshes_.find(key);
    if (existing != meshes_.end()) return &existing->second;

    const asset::MeshAsset* meshAsset = assets.get(handle);

    if (meshAsset == nullptr)
    {
        return nullptr;
    }

    RuntimeMesh runtimeMesh = RuntimeMesh::create(graphicsDevice_, *meshAsset);

    if (!runtimeMesh.isValid()) return nullptr;

    const auto [iterator, inserted] = meshes_.emplace(key, std::move(runtimeMesh));

    return &iterator->second;
}

const graphics::Texture2D&
RuntimeResourceCache::getOrCreateTexture(
    const asset::AssetHandle<asset::TextureAsset> handle,
    const asset::AssetRegistry& assets
)
{
    if (handle.isNull()) return whiteTexture_;

    const std::uint64_t key = handle.id().value;

    const auto existing = textures_.find(key);

    if (existing != textures_.end())
    {
        return existing->second;
    }

    const asset::TextureAsset* textureAsset =
        assets.get(handle);

    if (textureAsset == nullptr) return errorTexture_;

    if (!textureAsset->isValid()) return errorTexture_;


    graphics::Texture2D texture = uploadTexture(*textureAsset);

    if (!texture.isValid()) return errorTexture_;

    const auto [iterator, inserted] =
        textures_.emplace(
            key,
            std::move(texture));

    if (!inserted)
    {
        return iterator->second;
    }

    return iterator->second;
}

const graphics::Texture2D&
RuntimeResourceCache::whiteTexture() const noexcept
{
    return whiteTexture_;
}

const graphics::Texture2D&
RuntimeResourceCache::blackTexture() const noexcept
{
    return blackTexture_;
}

const graphics::Texture2D&
RuntimeResourceCache::neutralNormalTexture()
    const noexcept
{
    return neutralNormalTexture_;
}

const graphics::Texture2D&
RuntimeResourceCache::errorTexture() const noexcept
{
    return errorTexture_;
}

void RuntimeResourceCache::clear() noexcept
{
    textures_.clear();
    meshes_.clear();

    whiteTexture_ = {};
    blackTexture_ = {};
    neutralNormalTexture_ = {};
    errorTexture_ = {};

    materialInstances_.clear();
    runtimeMaterials_.clear();

    initialized_ = false;
}

graphics::TextureFormat
RuntimeResourceCache::toGraphicsTextureFormat(
    const asset::TexturePixelFormat format,
    const asset::ColorSpace colorSpace) const noexcept
{
    switch (format)
    {
    case asset::TexturePixelFormat::R8:
        return graphics::TextureFormat::R8;

    case asset::TexturePixelFormat::RG8:
        return graphics::TextureFormat::RG8;

    case asset::TexturePixelFormat::RGB8:
        return colorSpace == asset::ColorSpace::Srgb
            ? graphics::TextureFormat::SRGB8
            : graphics::TextureFormat::RGB8;

    case asset::TexturePixelFormat::RGBA8:
        return colorSpace == asset::ColorSpace::Srgb
            ? graphics::TextureFormat::SRGBA8
            : graphics::TextureFormat::RGBA8;
    }

    return graphics::TextureFormat::RGBA8;
}

graphics::Texture2D
RuntimeResourceCache::createWhiteTexture()
{
    constexpr std::array<std::uint8_t, 4> pixels{
        255,
        255,
        255,
        255
    };

    graphics::Texture2DDesc desc;
    desc.width = 1;
    desc.height = 1;
    desc.format = graphics::TextureFormat::RGBA8;
    desc.wrapU = graphics::TextureWrap::Repeat;
    desc.wrapV = graphics::TextureWrap::Repeat;
    desc.minFilter = graphics::TextureFilter::Linear;
    desc.magFilter = graphics::TextureFilter::Linear;
    desc.debugName = "Runtime White Texture";

    return graphicsDevice_.createTexture2D(
        desc,
        std::span<const std::uint8_t>{
            pixels
        });
}

graphics::Texture2D
RuntimeResourceCache::createBlackTexture()
{
    constexpr std::array<std::uint8_t, 4> pixels{
        0,
        0,
        0,
        255
    };

    graphics::Texture2DDesc desc;
    desc.width = 1;
    desc.height = 1;
    desc.format =
        graphics::TextureFormat::RGBA8;
    desc.wrapU =
        graphics::TextureWrap::Repeat;
    desc.wrapV =
        graphics::TextureWrap::Repeat;
    desc.minFilter =
        graphics::TextureFilter::Linear;
    desc.magFilter =
        graphics::TextureFilter::Linear;
    desc.debugName =
        "Runtime Black Texture";

    return graphicsDevice_.createTexture2D(
        desc,
        std::span<const std::uint8_t>{
            pixels
        });
}

graphics::Texture2D
RuntimeResourceCache::createErrorTexture()
{
    constexpr std::array<std::uint8_t, 16> pixels{
        255, 0, 255, 255,
        0, 0, 0, 255,
        0, 0, 0, 255,
        255, 0, 255, 255
    };

    graphics::Texture2DDesc desc;
    desc.width = 2;
    desc.height = 2;
    desc.format = graphics::TextureFormat::RGBA8;
    desc.wrapU = graphics::TextureWrap::Repeat;
    desc.wrapV = graphics::TextureWrap::Repeat;
    desc.minFilter = graphics::TextureFilter::Nearest;
    desc.magFilter = graphics::TextureFilter::Nearest;
    desc.debugName = "Runtime Error Texture";

    return graphicsDevice_.createTexture2D(
        desc,
        std::span<const std::uint8_t>{
            pixels
        });
}

graphics::Texture2D
RuntimeResourceCache::createNeutralNormalTexture()
{
    constexpr std::array<std::uint8_t, 4> pixels{
        128,
        128,
        255,
        255
    };

    graphics::Texture2DDesc desc;
    desc.width = 1;
    desc.height = 1;
    desc.format =
        graphics::TextureFormat::RGBA8;
    desc.wrapU =
        graphics::TextureWrap::Repeat;
    desc.wrapV =
        graphics::TextureWrap::Repeat;
    desc.minFilter =
        graphics::TextureFilter::Linear;
    desc.magFilter =
        graphics::TextureFilter::Linear;
    desc.debugName =
        "Runtime Neutral Normal Texture";

    return graphicsDevice_.createTexture2D(
        desc,
        std::span<const std::uint8_t>{
            pixels
        });
}

graphics::Texture2D
RuntimeResourceCache::uploadTexture(const asset::TextureAsset& textureAsset)
{
    graphics::Texture2DDesc desc;
    desc.width = textureAsset.width;
    desc.height = textureAsset.height;
    desc.format = toGraphicsTextureFormat(
        textureAsset.format,
        textureAsset.colorSpace
    );
    desc.wrapU = graphics::TextureWrap::Repeat;
    desc.wrapV = graphics::TextureWrap::Repeat;
    desc.minFilter = graphics::TextureFilter::Linear;
    desc.magFilter = graphics::TextureFilter::Linear;
    desc.debugName = textureAsset.debugName;

    return graphicsDevice_.createTexture2D(
        desc,
        std::span<const std::byte>{textureAsset.pixels}
    );
}

RuntimeMaterial* RuntimeResourceCache::getOrCreateRuntimeMaterial(
    const asset::AssetHandle<
        material::MaterialTemplate> templateHandle,
    const asset::AssetRegistry& assets
)
{
    if (!initialized_ || templateHandle.isNull()) return nullptr;

    const std::uint64_t key = templateHandle.id().value;

    const auto existing = runtimeMaterials_.find(key);

    if (existing != runtimeMaterials_.end())
        return &existing->second;

    RuntimeMaterial runtimeMaterial{
        graphicsDevice_,
        assets,
        templateHandle
    };

    if (!runtimeMaterial.isValid()) return nullptr;

    const auto [iterator, inserted] =
        runtimeMaterials_.emplace(key, std::move(runtimeMaterial));

    if (!inserted) return nullptr;
    return &iterator->second;
}

material::MaterialInstance*
RuntimeResourceCache::getOrCreateMaterialInstance(
    const asset::AssetHandle<asset::MaterialAsset> materialHandle,
    const asset::AssetHandle<material::MaterialTemplate> templateHandle,
    const asset::AssetRegistry& assets
)
{
    if (templateHandle.isNull()) return nullptr;

    const material::MaterialTemplate* materialTemplate = assets.get(templateHandle);

    if (materialTemplate == nullptr || !materialTemplate->isValid())
        return nullptr;

    const MaterialInstanceKey key {
        .materialId = materialHandle.id().value,
        .templateId = templateHandle.id().value
    };

    const auto existing = materialInstances_.find(key);

    if (existing != materialInstances_.end())
        return &existing->second;

    const asset::MaterialAsset* source = nullptr;

    if (!materialHandle.isNull())
    {
        source = assets.get(materialHandle);

        if (source == nullptr)
        {
            return nullptr;
        }
    }

    material::MaterialInstance instance =
        material::makeMaterialInstance(
            templateHandle,
            materialTemplate->kind,
            source);

    if (!instance.isValid())
    {
        return nullptr;
    }

    const auto [iterator, inserted] =
        materialInstances_.emplace(
            key,
            std::move(instance));

    return &iterator->second;
}

} // namespace stylized::render
