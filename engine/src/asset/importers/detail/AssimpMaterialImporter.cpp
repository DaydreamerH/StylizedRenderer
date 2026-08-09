#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <assimp/scene.h>

namespace stylized::asset::importers::detail
{

namespace
{

enum class ImportedTextureSemantic
{
    Color,
    Normal
};

[[nodiscard]] bool isGltfModelPath(
    const std::filesystem::path& path)
{
    std::string extension =
        path.extension().string();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](const unsigned char character)
        {
            return static_cast<char>(
                std::tolower(character));
        });

    return extension == ".gltf" ||
        extension == ".glb";
}

void invertNormalMapY(
    TextureAsset& texture) noexcept
{
    const std::size_t bytesPerPixel =
        texture.bytesPerPixel();

    if (bytesPerPixel < 2)
    {
        return;
    }

    for (std::size_t byteOffset = 1;
         byteOffset < texture.pixels.size();
         byteOffset += bytesPerPixel)
    {
        const std::uint8_t green =
            std::to_integer<std::uint8_t>(
                texture.pixels[byteOffset]);

        texture.pixels[byteOffset] =
            static_cast<std::byte>(
                255U - green);
    }
}

[[nodiscard]] AlphaMode readAlphaMode(
    const aiMaterial& sourceMaterial)
{
    aiString value;

    if (sourceMaterial.Get(
            AI_MATKEY_GLTF_ALPHAMODE,
            value) != AI_SUCCESS)
    {
        return AlphaMode::Opaque;
    }

    const std::string mode = value.C_Str();

    if (mode == "MASK")
    {
        return AlphaMode::Mask;
    }

    if (mode == "BLEND")
    {
        return AlphaMode::Blend;
    }

    return AlphaMode::Opaque;
}

[[nodiscard]] bool findBaseColorTexture(
    const aiMaterial& sourceMaterial,
    aiString& textureReference)
{
    if (sourceMaterial.GetTextureCount(
            aiTextureType_BASE_COLOR) > 0)
    {
        return sourceMaterial.GetTexture(
                   aiTextureType_BASE_COLOR,
                   0,
                   &textureReference) == AI_SUCCESS;
    }

    if (sourceMaterial.GetTextureCount(
            aiTextureType_DIFFUSE) > 0)
    {
        return sourceMaterial.GetTexture(
                   aiTextureType_DIFFUSE,
                   0,
                   &textureReference) == AI_SUCCESS;
    }

    return false;
}

[[nodiscard]] bool findNormalTexture(
    const aiMaterial& sourceMaterial,
    aiString& textureReference)
{
    if (sourceMaterial.GetTextureCount(
            aiTextureType_NORMALS) > 0)
    {
        return sourceMaterial.GetTexture(
                   aiTextureType_NORMALS,
                   0,
                   &textureReference) == AI_SUCCESS;
    }

    if (sourceMaterial.GetTextureCount(
            aiTextureType_NORMAL_CAMERA) > 0)
    {
        return sourceMaterial.GetTexture(
                   aiTextureType_NORMAL_CAMERA,
                   0,
                   &textureReference) == AI_SUCCESS;
    }

    return false;
}

[[nodiscard]] bool stageTexture(
    const aiScene& importedScene,
    const std::filesystem::path& modelPath,
    const aiString& textureReference,
    const ImportedTextureSemantic semantic,
    std::vector<TextureAsset>& textures,
    std::unordered_map<std::string, std::size_t>&
        textureIndices,
    std::optional<std::size_t>& destinationIndex)
{
    const bool isNormalMap =
        semantic == ImportedTextureSemantic::Normal;

    const ColorSpace colorSpace =
        isNormalMap
            ? ColorSpace::Linear
            : ColorSpace::Srgb;

    const auto embeddedResult =
        importedScene.GetEmbeddedTextureAndIndex(
            textureReference.C_Str());

    const aiTexture* embeddedTexture =
        embeddedResult.first;

    std::filesystem::path externalPath;

    std::string cacheKey =
        isNormalMap
            ? "normal:"
            : "color:";

    if (embeddedTexture != nullptr)
    {
        cacheKey +=
            "embedded:" +
            (embeddedResult.second >= 0
                 ? std::to_string(embeddedResult.second)
                 : std::string{
                       textureReference.C_Str()});
    }
    else
    {
        externalPath =
            (modelPath.parent_path() /
             std::filesystem::path{
                 textureReference.C_Str()})
                .lexically_normal();

        cacheKey += externalPath.generic_string();
    }

    const auto existing =
        textureIndices.find(cacheKey);

    if (existing != textureIndices.end())
    {
        destinationIndex = existing->second;
        return true;
    }

    TextureAsset texture;

    const bool decoded =
        embeddedTexture != nullptr
            ? decodeEmbeddedTexture(
                  *embeddedTexture,
                  modelPath,
                  texture)
            : decodeExternalTexture(
                  externalPath,
                  texture);

    if (!decoded)
    {
        return false;
    }

    texture.colorSpace = colorSpace;

    if (isNormalMap &&
        isGltfModelPath(modelPath))
    {
        invertNormalMapY(texture);
    }

    const std::size_t textureIndex =
        textures.size();

    textures.push_back(std::move(texture));
    textureIndices.emplace(
        std::move(cacheKey),
        textureIndex);

    destinationIndex = textureIndex;
    return true;
}

} // namespace

bool stageMaterials(
    const aiScene& importedScene,
    const std::filesystem::path& modelPath,
    std::vector<TextureAsset>& textures,
    std::vector<StagedMaterial>& materials)
{
    textures.clear();
    materials.clear();
    materials.reserve(importedScene.mNumMaterials);

    std::unordered_map<std::string, std::size_t>
        textureIndices;

    for (unsigned int materialIndex = 0;
         materialIndex < importedScene.mNumMaterials;
         ++materialIndex)
    {
        const aiMaterial* sourceMaterial =
            importedScene.mMaterials[materialIndex];

        if (sourceMaterial == nullptr)
        {
            return false;
        }

        StagedMaterial staged;

        aiString materialName;
        if (sourceMaterial->Get(
                AI_MATKEY_NAME,
                materialName) == AI_SUCCESS)
        {
            staged.asset.name = materialName.C_Str();
        }

        aiColor4D baseColor{1.0F, 1.0F, 1.0F, 1.0F};

        if (sourceMaterial->Get(
                AI_MATKEY_BASE_COLOR,
                baseColor) != AI_SUCCESS)
        {
            sourceMaterial->Get(
                AI_MATKEY_COLOR_DIFFUSE,
                baseColor);
        }

        staged.asset.baseColorFactor = {
            baseColor.r,
            baseColor.g,
            baseColor.b,
            baseColor.a};

        float metallicFactor =
            staged.asset.metallicFactor;

        if (sourceMaterial->Get(
                AI_MATKEY_METALLIC_FACTOR,
                metallicFactor) == AI_SUCCESS)
        {
            staged.asset.metallicFactor =
                std::clamp(
                    metallicFactor,
                    0.0F,
                    1.0F);
        }

        float roughnessFactor =
            staged.asset.roughnessFactor;

        if (sourceMaterial->Get(
                AI_MATKEY_ROUGHNESS_FACTOR,
                roughnessFactor) == AI_SUCCESS)
        {
            staged.asset.roughnessFactor =
                std::clamp(
                    roughnessFactor,
                    0.04F,
                    1.0F);
        }

        staged.asset.alphaMode =
            readAlphaMode(*sourceMaterial);

        float opacity = 1.0F;
        if (sourceMaterial->Get(
                AI_MATKEY_OPACITY,
                opacity) == AI_SUCCESS)
        {
            staged.asset.baseColorFactor.a *= opacity;

            if (opacity < 1.0F &&
                staged.asset.alphaMode == AlphaMode::Opaque)
            {
                staged.asset.alphaMode = AlphaMode::Blend;
            }
        }

        float alphaCutoff = 0.5F;
        sourceMaterial->Get(
            AI_MATKEY_GLTF_ALPHACUTOFF,
            alphaCutoff);
        staged.asset.alphaCutoff = alphaCutoff;

        int doubleSided = 0;
        sourceMaterial->Get(
            AI_MATKEY_TWOSIDED,
            doubleSided);
        staged.asset.doubleSided = doubleSided != 0;

        aiString textureReference;

        if (findBaseColorTexture(
                *sourceMaterial,
                textureReference))
        {
            if (!stageTexture(
                    importedScene,
                    modelPath,
                    textureReference,
                    ImportedTextureSemantic::Color,
                    textures,
                    textureIndices,
                    staged.baseColorTextureIndex))
            {
                return false;
            }
        }

        if (findNormalTexture(
                *sourceMaterial,
                textureReference))
        {
            if (!stageTexture(
                    importedScene,
                    modelPath,
                    textureReference,
                    ImportedTextureSemantic::Normal,
                    textures,
                    textureIndices,
                    staged.normalTextureIndex))
            {
                return false;
            }
        }

        float normalScale =
            staged.asset.normalScale;

        if (sourceMaterial->Get(
                AI_MATKEY_GLTF_TEXTURE_SCALE(
                    aiTextureType_NORMALS,
                    0),
                normalScale) == AI_SUCCESS)
        {
            staged.asset.normalScale =
                std::max(normalScale, 0.0F);
        }

        materials.push_back(std::move(staged));
    }

    return true;
}

} // namespace stylized::asset::importers::detail
