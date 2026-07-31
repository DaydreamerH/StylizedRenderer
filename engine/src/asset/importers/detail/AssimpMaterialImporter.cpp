#include <asset/importers/detail/AssimpImportInternal.hpp>

#include <cstddef>
#include <filesystem>
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
            const auto embeddedResult =
                importedScene.GetEmbeddedTextureAndIndex(
                    textureReference.C_Str());

            const aiTexture* embeddedTexture =
                embeddedResult.first;

            std::string cacheKey;
            std::filesystem::path externalPath;

            if (embeddedTexture != nullptr)
            {
                cacheKey =
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

                cacheKey = externalPath.generic_string();
            }

            const auto existing =
                textureIndices.find(cacheKey);

            if (existing != textureIndices.end())
            {
                staged.textureIndex = existing->second;
            }
            else
            {
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

                const std::size_t textureIndex =
                    textures.size();

                textures.push_back(std::move(texture));
                textureIndices.emplace(
                    std::move(cacheKey),
                    textureIndex);
                staged.textureIndex = textureIndex;
            }
        }

        materials.push_back(std::move(staged));
    }

    return true;
}

} // namespace stylized::asset::importers::detail
