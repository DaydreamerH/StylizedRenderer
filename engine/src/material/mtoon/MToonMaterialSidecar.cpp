#include <material/mtoon/MToonMaterialSidecar.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/TextureAsset.hpp>
#include <asset/importers/TextureImporter.hpp>
#include <material/MaterialInstance.hpp>
#include <material/mtoon/MToonMaterialSidecarSerializer.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace stylized::material
{
    
namespace
{

void setConversionError(
    MToonSidecarError& error,
    const std::string_view material,
    const std::string_view field,
    const std::string_view message)
{
    error.material = material;
    error.field = field;
    error.message = message;
}

bool resolveTexturePath(
    const asset::AssetHandle<asset::TextureAsset> handle,
    const asset::AssetRegistry& assets,
    const std::filesystem::path& sidecarDirectory,
    std::filesystem::path& destination,
    const std::string_view materialName,
    const std::string_view field,
    MToonSidecarError& error)
{
    destination.clear();

    if (handle.isNull())
        return true;

    const asset::TextureAsset* texture =
        assets.get(handle);

    if (texture == nullptr)
    {
        setConversionError(
            error,
            materialName,
            field,
            "Texture handle does not reference a valid asset.");

        return false;
    }

    if (texture->sourcePath.empty())
    {
        setConversionError(
            error,
            materialName,
            field,
            "Texture asset does not have a source path.");

        return false;
    }

    std::error_code filesystemError;

    const std::filesystem::path absoluteTexturePath =
        std::filesystem::absolute(
            texture->sourcePath,
            filesystemError
        );

    if (filesystemError)
    {
        setConversionError(
            error,
            materialName,
            field,
            "Failed to resolve texture source path.");

        return false;
    }

    const std::filesystem::path absoluteSidecarDirectory =
        std::filesystem::absolute(
            sidecarDirectory,
            filesystemError);

    if (filesystemError)
    {
        setConversionError(
            error,
            materialName,
            field,
            "Failed to resolve sidecar directory.");

        return false;
    }

    const std::filesystem::path relativePath =
        absoluteTexturePath
            .lexically_normal()
            .lexically_relative(
                absoluteSidecarDirectory
                    .lexically_normal());

    if (relativePath.empty() ||
        relativePath.is_absolute() ||
        relativePath.has_root_name() ||
        relativePath.has_root_directory())
    {
        setConversionError(
            error,
            materialName,
            field,
            "Texture cannot be represented by a relative path.");

        return false;
    }

    destination =
        relativePath.lexically_normal();

    return true;
}

bool restoreTexture(
    const std::filesystem::path& relativePath,
    const asset::ColorSpace colorSpace,
    const asset::AssetHandle<asset::TextureAsset> currentHandle,
    asset::AssetRegistry& assets,
    asset::importers::TextureImporter& importer,
    const std::filesystem::path& sidecarDirectory,
    asset::AssetHandle<asset::TextureAsset>& destination,
    const std::string_view materialName,
    const std::string_view field,
    MToonSidecarError& error)
{
    if (relativePath.empty())
    {
        destination = currentHandle;
        return true;
    }

    if (relativePath.is_absolute() ||
        relativePath.has_root_name() ||
        relativePath.has_root_directory())
    {
        setConversionError(
            error,
            materialName,
            field,
            "Texture path must be relative.");

        return false;
    }

    std::error_code filesystemError;

    const std::filesystem::path resolvedPath =
        std::filesystem::absolute(
            sidecarDirectory / relativePath,
            filesystemError);

    if (filesystemError)
    {
        setConversionError(
            error,
            materialName,
            field,
            "Failed to resolve texture path.");

        return false;
    }

    if (!currentHandle.isNull())
    {
        const asset::TextureAsset* currentTexture =
            assets.get(currentHandle);

        if (currentTexture != nullptr &&
            !currentTexture->sourcePath.empty())
        {
            const std::filesystem::path currentSourcePath =
                std::filesystem::absolute(
                    currentTexture->sourcePath,
                    filesystemError
                );

            if (!filesystemError &&
                currentSourcePath.lexically_normal() ==
                resolvedPath.lexically_normal())
            {
                destination = currentHandle;
                return true;
            }
        }
    }

    const asset::AssetHandle<asset::TextureAsset> imported =
        importer.import(
            resolvedPath,
            colorSpace);

    if (imported.isNull())
    {
        setConversionError(
            error,
            materialName,
            field,
            "Failed to import texture.");

        return false;
    }

    destination = imported;
    return true;
}

} // namespace

bool captureMToonSidecarMaterial(
    const std::string_view materialName,
    const MaterialInstance& instance,
    const asset::AssetRegistry& assets,
    const std::filesystem::path& sidecarDirectory,
    MToonSidecarMaterial& destination,
    MToonSidecarError& error)
{
    error.clear();

    if (materialName.empty())
    {
        setConversionError(
            error,
            {},
            "name",
            "Material name cannot be empty.");

        return false;
    }

    if (!instance.mtoonParameters.has_value())
    {
        setConversionError(
            error,
            materialName,
            "mtoonParameters",
            "Material instance does not contain MToon parameters.");

        return false;
    }

    const MToonMaterialParameters& parameters =
        instance.mtoonParameters.value();

    MToonSidecarMaterial captured;

    captured.name = materialName;
    captured.outlineGroup =
        parameters.outlineGroup;
    captured.baseColorFactor =
        instance.baseColorFactor;

    captured.shadeColor =
        parameters.shadeColor;

    captured.shadingShift =
        parameters.shadingShift;

    captured.shadingShiftTextureScale =
        parameters.shadingShiftTextureScale;

    captured.shadingToony =
        parameters.shadingToony;

    captured.normalScale =
        parameters.normalScale;

    captured.surfaceOffset =
        parameters.surfaceOffset;

    captured.shadowNormalInfluence =
        parameters.shadowNormalInfluence;

    captured.receiveShadow =
        parameters.receiveShadow;

    captured.shadowCutoffEnabled =
        parameters.shadowCutoffEnabled;

    captured.shadowCutoff =
        parameters.shadowCutoff;

    captured.sphericalFaceNormalEnabled =
        parameters.sphericalFaceNormalEnabled;

    captured.sphericalFaceNormalCenter =
        parameters.sphericalFaceNormalCenter;

    captured.sphericalFaceNormalRadius =
        parameters.sphericalFaceNormalRadius;

    captured.sphericalFaceNormalSoftness =
        parameters.sphericalFaceNormalSoftness;

    captured.sphericalFaceNormalBlend =
        parameters.sphericalFaceNormalBlend;

    captured.giEqualization =
        parameters.giEqualization;

    captured.matcapColor =
        parameters.matcapColor;

    captured.matcapStrength =
        parameters.matcapStrength;

    captured.rimColor =
        parameters.rimColor;

    captured.rimFresnelPower =
        parameters.rimFresnelPower;

    captured.rimLift =
        parameters.rimLift;

    captured.rimLightingMix =
        parameters.rimLightingMix;

    captured.emissionColor =
        parameters.emissionColor;

    captured.emissionStrength =
        parameters.emissionStrength;

    captured.outlineEnabled =
        parameters.outline.enabled;

    captured.outlineWidthMode =
        parameters.outline.widthMode;

    captured.outlineWidth =
        parameters.outline.width;

    captured.outlineColor =
        parameters.outline.color;

    captured.outlineLightingMix =
        parameters.outline.lightingMix;

    captured.occlusionStrength =
        parameters.occlusionStrength;

    captured.specularColor =
        parameters.specularColor;

    captured.specularStrength =
        parameters.specularStrength;

    captured.specularPower =
        parameters.specularPower;

    if (!resolveTexturePath(
            instance.baseColorTexture,
            assets,
            sidecarDirectory,
            captured.textures.baseColor,
            materialName,
            "textures.baseColor",
            error) ||
        !resolveTexturePath(
            parameters.textures.shadeTexture,
            assets,
            sidecarDirectory,
            captured.textures.shade,
            materialName,
            "textures.shade",
            error) ||
        !resolveTexturePath(
            parameters.textures.toonRampTexture,
            assets,
            sidecarDirectory,
            captured.textures.toonRamp,
            materialName,
            "textures.toonRamp",
            error) ||
        !resolveTexturePath(
            parameters.textures.normalTexture,
            assets,
            sidecarDirectory,
            captured.textures.normal,
            materialName,
            "textures.normal",
            error) ||
        !resolveTexturePath(
            parameters.textures.shadingShiftTexture,
            assets,
            sidecarDirectory,
            captured.textures.shadingShift,
            materialName,
            "textures.shadingShift",
            error) ||
        !resolveTexturePath(
            parameters.textures.matcapTexture,
            assets,
            sidecarDirectory,
            captured.textures.matcap,
            materialName,
            "textures.matcap",
            error) ||
        !resolveTexturePath(
            parameters.textures.rimMaskTexture,
            assets,
            sidecarDirectory,
            captured.textures.rimMask,
            materialName,
            "textures.rimMask",
            error) ||
        !resolveTexturePath(
            parameters.textures.emissionTexture,
            assets,
            sidecarDirectory,
            captured.textures.emission,
            materialName,
            "textures.emission",
            error) ||
        !resolveTexturePath(
            parameters.textures.outlineWidthMaskTexture,
            assets,
            sidecarDirectory,
            captured.textures.outlineWidthMask,
            materialName,
            "textures.outlineWidthMask",
            error) ||
        !resolveTexturePath(
            parameters.textures.occlusionTexture,
            assets,
            sidecarDirectory,
            captured.textures.occlusion,
            materialName,
            "textures.occlusion",
            error) ||
        !resolveTexturePath(
            parameters.textures.specularTexture,
            assets,
            sidecarDirectory,
            captured.textures.specular,
            materialName,
            "textures.specular",
            error))
    {
        return false;
    }

    destination =
        std::move(captured);

    return true;
}

bool applyMToonSidecarMaterial(
    const MToonSidecarMaterial& source,
    const std::filesystem::path& sidecarDirectory,
    asset::AssetRegistry& assets,
    MaterialInstance& destination,
    MToonSidecarError& error)
{
    error.clear();

    if (source.name.empty())
    {
        setConversionError(
            error,
            {},
            "name",
            "Material name cannot be empty.");

        return false;
    }

    if (!destination.mtoonParameters.has_value())
    {
        setConversionError(
            error,
            source.name,
            "mtoonParameters",
            "Material instance does not contain MToon parameters.");

        return false;
    }

    MaterialInstance applied =
        destination;

    MToonMaterialParameters& parameters =
        applied.mtoonParameters.value();

    parameters.outlineGroup =
        source.outlineGroup;

    applied.baseColorFactor =
        source.baseColorFactor;

    parameters.shadeColor =
        source.shadeColor;

    parameters.shadingShift =
        source.shadingShift;

    parameters.shadingShiftTextureScale =
        source.shadingShiftTextureScale;

    parameters.shadingToony =
        source.shadingToony;

    parameters.normalScale =
        source.normalScale;

    parameters.surfaceOffset =
        source.surfaceOffset;

    parameters.shadowNormalInfluence =
        source.shadowNormalInfluence;

    parameters.receiveShadow =
        source.receiveShadow;

    parameters.shadowCutoffEnabled =
        source.shadowCutoffEnabled;

    parameters.shadowCutoff =
        source.shadowCutoff;

    parameters.sphericalFaceNormalEnabled =
        source.sphericalFaceNormalEnabled;

    parameters.sphericalFaceNormalCenter =
        source.sphericalFaceNormalCenter;

    parameters.sphericalFaceNormalRadius =
        source.sphericalFaceNormalRadius;

    parameters.sphericalFaceNormalSoftness =
        source.sphericalFaceNormalSoftness;

    parameters.sphericalFaceNormalBlend =
        source.sphericalFaceNormalBlend;

    parameters.giEqualization =
        source.giEqualization;

    parameters.matcapColor =
        source.matcapColor;

    parameters.matcapStrength =
        source.matcapStrength;

    parameters.rimColor =
        source.rimColor;

    parameters.rimFresnelPower =
        source.rimFresnelPower;

    parameters.rimLift =
        source.rimLift;

    parameters.rimLightingMix =
        source.rimLightingMix;

    parameters.emissionColor =
        source.emissionColor;

    parameters.emissionStrength =
        source.emissionStrength;

    parameters.outline.enabled =
        source.outlineEnabled;

    parameters.outline.widthMode =
        source.outlineWidthMode;

    parameters.outline.width =
        source.outlineWidth;

    parameters.outline.color =
        source.outlineColor;

    parameters.outline.lightingMix =
        source.outlineLightingMix;

    parameters.occlusionStrength =
        source.occlusionStrength;

    parameters.specularColor =
        source.specularColor;

    parameters.specularStrength =
        source.specularStrength;

    parameters.specularPower =
        source.specularPower;

    asset::importers::TextureImporter importer{
        assets
    };

    if (!restoreTexture(
            source.textures.baseColor,
            asset::ColorSpace::Srgb,
            applied.baseColorTexture,
            assets,
            importer,
            sidecarDirectory,
            applied.baseColorTexture,
            source.name,
            "textures.baseColor",
            error) ||
        !restoreTexture(
            source.textures.shade,
            asset::ColorSpace::Srgb,
            parameters.textures.shadeTexture,
            assets,
            importer,
            sidecarDirectory,
            parameters.textures.shadeTexture,
            source.name,
            "textures.shade",
            error) ||
        !restoreTexture(
            source.textures.toonRamp,
            asset::ColorSpace::Srgb,
            parameters.textures.toonRampTexture,
            assets,
            importer,
            sidecarDirectory,
            parameters.textures.toonRampTexture,
            source.name,
            "textures.toonRamp",
            error) ||
        !restoreTexture(
            source.textures.normal,
            asset::ColorSpace::Linear,
            parameters.textures.normalTexture,
            assets,
            importer,
            sidecarDirectory,
            parameters.textures.normalTexture,
            source.name,
            "textures.normal",
            error) ||
        !restoreTexture(
            source.textures.shadingShift,
            asset::ColorSpace::Linear,
            parameters.textures.shadingShiftTexture,
            assets,
            importer,
            sidecarDirectory,
            parameters.textures.shadingShiftTexture,
            source.name,
            "textures.shadingShift",
            error) ||
        !restoreTexture(
            source.textures.matcap,
            asset::ColorSpace::Srgb,
            parameters.textures.matcapTexture,
            assets,
            importer,
            sidecarDirectory,
            parameters.textures.matcapTexture,
            source.name,
            "textures.matcap",
            error) ||
        !restoreTexture(
            source.textures.rimMask,
            asset::ColorSpace::Linear,
            parameters.textures.rimMaskTexture,
            assets,
            importer,
            sidecarDirectory,
            parameters.textures.rimMaskTexture,
            source.name,
            "textures.rimMask",
            error) ||
        !restoreTexture(
            source.textures.emission,
            asset::ColorSpace::Srgb,
            parameters.textures.emissionTexture,
            assets,
            importer,
            sidecarDirectory,
            parameters.textures.emissionTexture,
            source.name,
            "textures.emission",
            error) ||
        !restoreTexture(
            source.textures.outlineWidthMask,
            asset::ColorSpace::Linear,
            parameters.textures.outlineWidthMaskTexture,
            assets,
            importer,
            sidecarDirectory,
            parameters.textures.outlineWidthMaskTexture,
            source.name,
            "textures.outlineWidthMask",
            error) ||
        !restoreTexture(
            source.textures.occlusion,
            asset::ColorSpace::Linear,
            parameters.textures.occlusionTexture,
            assets,
            importer,
            sidecarDirectory,
            parameters.textures.occlusionTexture,
            source.name,
            "textures.occlusion",
            error) ||
        !restoreTexture(
            source.textures.specular,
            asset::ColorSpace::Linear,
            parameters.textures.specularTexture,
            assets,
            importer,
            sidecarDirectory,
            parameters.textures.specularTexture,
            source.name,
            "textures.specular",
            error))
    {
        return false;
    }

    destination =
        std::move(applied);

    return true;
}

} // namespace stylized::material
