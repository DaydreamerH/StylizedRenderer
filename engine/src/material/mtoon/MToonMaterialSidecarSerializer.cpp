#include <material/mtoon/MToonMaterialSidecarSerializer.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace stylized::material
{
    
namespace
{
    
using Json = nlohmann::json;

void setError(
    MToonSidecarError& error,
    const std::string& material,
    const std::string& field,
    const std::string& message
)
{
    error.material = material;
    error.field = field;
    error.message = message;
}

Json toJson(const glm::vec3& value)
{
    return Json::array({
        value.x,
        value.y,
        value.z
    });
}

Json toJson(const glm::vec4& value)
{
    return Json::array({
        value.x,
        value.y,
        value.z,
        value.w
    });
}

bool readFloat(
    const Json& source,
    const char* field,
    float& destination,
    const float minimum,
    const float maximum,
    const std::string& materialName,
    MToonSidecarError& error
)
{
    const auto iterator = source.find(field);

    if (iterator == source.end())
        return true;

    if (!iterator->is_number())
    {
        setError(
            error,
            materialName,
            field,
            "Expected a number.");

        return false;
    }

    const float value =
        iterator->get<float>();

    if (!std::isfinite(value))
    {
        setError(
            error,
            materialName,
            field,
            "Value must be finite.");

        return false;
    }

    destination =
        std::clamp(
            value,
            minimum,
            maximum);

    return true;
}

bool readBool(
    const Json& source,
    const char* field,
    bool& destination,
    const std::string& materialName,
    MToonSidecarError& error)
{
    const auto iterator =
        source.find(field);

    if (iterator == source.end())
    {
        return true;
    }

    if (!iterator->is_boolean())
    {
        setError(
            error,
            materialName,
            field,
            "Expected a boolean.");

        return false;
    }

    destination =
        iterator->get<bool>();

    return true;
}

bool readString(
    const Json& source,
    const char* field,
    std::string& destination,
    const std::string& materialName,
    MToonSidecarError& error)
{
    const auto iterator =
        source.find(field);

    if (iterator == source.end())
    {
        return true;
    }

    if (!iterator->is_string())
    {
        setError(
            error,
            materialName,
            field,
            "Expected a string.");

        return false;
    }

    const std::string& value =
        iterator->get_ref<const std::string&>();

    if (value.size() > 128U)
    {
        setError(
            error,
            materialName,
            field,
            "String exceeds 128 characters.");

        return false;
    }

    destination = value;
    return true;
}

bool readOptionalFloat(
    const Json& source,
    const char* field,
    std::optional<float>& destination,
    const float minimum,
    const float maximum,
    const std::string& materialName,
    MToonSidecarError& error)
{
    const auto iterator = source.find(field);
    if (iterator == source.end() || iterator->is_null())
    {
        destination.reset();
        return true;
    }

    float value = 0.0F;
    if (!readFloat(
            source,
            field,
            value,
            minimum,
            maximum,
            materialName,
            error))
    {
        return false;
    }

    destination = value;
    return true;
}

template<std::size_t ComponentCount>
bool readVector(
    const Json& source,
    const char* field,
    float* destination,
    float minimum,
    float maximum,
    const std::string& materialName,
    MToonSidecarError& error);

bool readOptionalVector3(
    const Json& source,
    const char* field,
    std::optional<glm::vec3>& destination,
    const std::string& materialName,
    MToonSidecarError& error)
{
    const auto iterator = source.find(field);
    if (iterator == source.end() || iterator->is_null())
    {
        destination.reset();
        return true;
    }

    glm::vec3 value{0.0F};
    if (!readVector<3>(
            source,
            field,
            &value.x,
            0.0F,
            100.0F,
            materialName,
            error))
    {
        return false;
    }

    destination = value;
    return true;
}

template<std::size_t ComponentCount>
bool readVector(
    const Json& source,
    const char* field,
    float* destination,
    const float minimum,
    const float maximum,
    const std::string& materialName,
    MToonSidecarError& error)
{
    const auto iterator = source.find(field);

    if (iterator == source.end())
    {
        return true;
    }

    if (!iterator->is_array() ||
        iterator->size() != ComponentCount)
    {
        setError(
            error,
            materialName,
            field,
            "Expected a numeric array.");

        return false;
    }

    for (std::size_t index = 0;
         index < ComponentCount;
         ++index)
    {
        if (!(*iterator)[index].is_number())
        {
            setError(
                error,
                materialName,
                field,
                "Expected a numeric array.");

            return false;
        }

        const float value =
            (*iterator)[index].get<float>();

        if (!std::isfinite(value))
        {
            setError(
                error,
                materialName,
                field,
                "Components must be finite.");

            return false;
        }

        destination[index] =
            std::clamp(
                value,
                minimum,
                maximum);
    }

    return true;
}

bool readTexturePath(
    const Json& source,
    const char* field,
    std::filesystem::path& destination,
    const std::string& materialName,
    MToonSidecarError& error)
{
    const auto iterator = source.find(field);

    if (iterator == source.end())
    {
        return true;
    }

    if (!iterator->is_string())
    {
        setError(
            error,
            materialName,
            field,
            "Expected a texture path string.");

        return false;
    }

    const std::filesystem::path path{
        iterator->get<std::string>()
    };

    if (path.is_absolute() ||
        path.has_root_name() ||
        path.has_root_directory())
    {
        setError(
            error,
            materialName,
            field,
            "Texture path must be relative.");

        return false;
    }

    destination = path;
    return true;
}

const char* outlineWidthModeToString(
    const OutlineWidthMode mode) noexcept
{
    switch (mode)
    {
    case OutlineWidthMode::World:
        return "world";

    case OutlineWidthMode::Screen:
        return "screen";
    }

    return "screen";
}

bool readOutlineWidthMode(
    const Json& source,
    const char* field,
    OutlineWidthMode& destination,
    const std::string& materialName,
    MToonSidecarError& error)
{
    const auto iterator =
        source.find(field);

    if (iterator == source.end())
    {
        return true;
    }

    if (!iterator->is_string())
    {
        setError(
            error,
            materialName,
            field,
            "Expected an outline width mode string.");

        return false;
    }

    const std::string& value =
        iterator->get_ref<const std::string&>();

    if (value == "world")
    {
        destination =
            OutlineWidthMode::World;

        return true;
    }

    if (value == "screen")
    {
        destination =
            OutlineWidthMode::Screen;

        return true;
    }

    setError(
        error,
        materialName,
        field,
        "Outline width mode must be world or screen.");

    return false;
}

Json texturePathsToJson(
    const MToonSidecarTexturePaths& textures)
{
    return Json{
        {"baseColor", textures.baseColor.generic_string()},
        {"shade", textures.shade.generic_string()},
        {"toonRamp", textures.toonRamp.generic_string()},
        {"normal", textures.normal.generic_string()},
        {"shadingShift", textures.shadingShift.generic_string()},
        {"matcap", textures.matcap.generic_string()},
        {"rimMask", textures.rimMask.generic_string()},
        {"emission", textures.emission.generic_string()},
        {
            "outlineWidthMask",
            textures.outlineWidthMask.generic_string()
        },
        {"occlusion", textures.occlusion.generic_string()},
        {"specular", textures.specular.generic_string()},
    };
}

Json outlineToJson(
    const MToonSidecarMaterial& material)
{
    return Json{
        {"enabled", material.outlineEnabled},
        {
            "widthMode",
            outlineWidthModeToString(
                material.outlineWidthMode)
        },
        {"width", material.outlineWidth},
        {"color", toJson(material.outlineColor)},
        {"lightingMix", material.outlineLightingMix}
    };
}

Json screenOutlineToJson(
    const MToonSidecarMaterial& material)
{
    Json result{
        {"enabled", material.screenOutlineEnabled},
        {"depthEnabled", material.screenOutlineDepthEnabled},
        {"normalEnabled", material.screenOutlineNormalEnabled},
        {"detectSelfDepth", material.outlineDetectSelfDepth},
        {"detectSelfNormal", material.outlineDetectSelfNormal}
    };

    if (!material.outlineGroup.empty())
    {
        result["group"] = material.outlineGroup;
    }

    if (material.screenOutlineWidth.has_value())
    {
        result["width"] = *material.screenOutlineWidth;
    }

    if (material.screenOutlineDepthThreshold.has_value())
    {
        result["depthThreshold"] =
            *material.screenOutlineDepthThreshold;
    }

    if (material.screenOutlineNormalThreshold.has_value())
    {
        result["normalThreshold"] =
            *material.screenOutlineNormalThreshold;
    }

    if (material.screenOutlineColor.has_value())
    {
        result["color"] =
            toJson(*material.screenOutlineColor);
    }

    return result;
}

Json materialToJson(
    const MToonSidecarMaterial& material)
{
    Json result{
        {"name", material.name},
        {"baseColorFactor", toJson(material.baseColorFactor)},
        {"shadeColor", toJson(material.shadeColor)},
        {"shadingShift", material.shadingShift},
        {
            "shadingShiftTextureScale",
            material.shadingShiftTextureScale
        },
        {"shadingToony", material.shadingToony},
        {"normalScale", material.normalScale},
        {"surfaceOffset", material.surfaceOffset},
        {
            "shadowNormalInfluence",
            material.shadowNormalInfluence
        },
        {
            "receiveShadow",
            material.receiveShadow
        },
        {
            "shadowCutoffEnabled",
            material.shadowCutoffEnabled
        },
        {
            "shadowCutoff",
            material.shadowCutoff
        },
        {
            "sphericalFaceNormalEnabled",
            material.sphericalFaceNormalEnabled
        },
        {
            "sphericalFaceNormalCenter",
            toJson(material.sphericalFaceNormalCenter)
        },
        {
            "sphericalFaceNormalRadius",
            material.sphericalFaceNormalRadius
        },
        {
            "sphericalFaceNormalSoftness",
            material.sphericalFaceNormalSoftness
        },
        {
            "sphericalFaceNormalBlend",
            material.sphericalFaceNormalBlend
        },
        {"giEqualization", material.giEqualization},
        {"matcapColor", toJson(material.matcapColor)},
        {"matcapStrength", material.matcapStrength},
        {"rimColor", toJson(material.rimColor)},
        {"rimFresnelPower", material.rimFresnelPower},
        {"rimLift", material.rimLift},
        {"rimLightingMix", material.rimLightingMix},
        {"emissionColor", toJson(material.emissionColor)},
        {"emissionStrength", material.emissionStrength},
        {"screenOutline", screenOutlineToJson(material)},
        {"outline", outlineToJson(material)},
        {"textures", texturePathsToJson(material.textures)},
        {"occlusionStrength", material.occlusionStrength},
        {"specularColor", toJson(material.specularColor)},
        {"specularStrength", material.specularStrength},
        {"specularPower", material.specularPower}
    };

    return result;
}

bool parseScreenOutline(
    const Json& source,
    MToonSidecarMaterial& material,
    MToonSidecarError& error)
{
    const auto iterator = source.find("screenOutline");
    if (iterator == source.end())
    {
        return true;
    }

    if (!iterator->is_object())
    {
        setError(
            error,
            material.name,
            "screenOutline",
            "Expected an object.");
        return false;
    }

    const Json& outline = *iterator;
    material.hasScreenOutline = true;
    return
        readString(
            outline, "group", material.outlineGroup,
            material.name, error) &&
        readBool(
            outline, "enabled", material.screenOutlineEnabled,
            material.name, error) &&
        readBool(
            outline, "depthEnabled", material.screenOutlineDepthEnabled,
            material.name, error) &&
        readBool(
            outline, "normalEnabled", material.screenOutlineNormalEnabled,
            material.name, error) &&
        readBool(
            outline, "detectSelfDepth", material.outlineDetectSelfDepth,
            material.name, error) &&
        readBool(
            outline, "detectSelfNormal", material.outlineDetectSelfNormal,
            material.name, error) &&
        readOptionalFloat(
            outline, "width", material.screenOutlineWidth,
            0.0F, 100.0F, material.name, error) &&
        readOptionalFloat(
            outline, "depthThreshold",
            material.screenOutlineDepthThreshold,
            0.000001F, 1.0F, material.name, error) &&
        readOptionalFloat(
            outline, "normalThreshold",
            material.screenOutlineNormalThreshold,
            0.000001F, 2.0F, material.name, error) &&
        readOptionalVector3(
            outline, "color", material.screenOutlineColor,
            material.name, error);
}

bool parseTexturePaths(
    const Json& source,
    MToonSidecarMaterial& material,
    MToonSidecarError& error)
{
    const auto iterator =
        source.find("textures");

    if (iterator == source.end())
    {
        return true;
    }

    if (!iterator->is_object())
    {
        setError(
            error,
            material.name,
            "textures",
            "Expected an object.");

        return false;
    }

    return
        readTexturePath(
            *iterator,
            "baseColor",
            material.textures.baseColor,
            material.name,
            error) &&
        readTexturePath(
            *iterator,
            "shade",
            material.textures.shade,
            material.name,
            error) &&
        readTexturePath(
            *iterator,
            "toonRamp",
            material.textures.toonRamp,
            material.name,
            error) &&
        readTexturePath(
            *iterator,
            "normal",
            material.textures.normal,
            material.name,
            error) &&
        readTexturePath(
            *iterator,
            "shadingShift",
            material.textures.shadingShift,
            material.name,
            error) &&
        readTexturePath(
            *iterator,
            "matcap",
            material.textures.matcap,
            material.name,
            error) &&
        readTexturePath(
            *iterator,
            "rimMask",
            material.textures.rimMask,
            material.name,
            error) &&
        readTexturePath(
            *iterator,
            "emission",
            material.textures.emission,
            material.name,
            error) &&
        readTexturePath(
            *iterator,
            "outlineWidthMask",
            material.textures.outlineWidthMask,
            material.name,
            error) &&
        readTexturePath(
            *iterator,
            "occlusion",
            material.textures.occlusion,
            material.name,
            error) &&
        readTexturePath(
            *iterator,
            "specular",
            material.textures.specular,
            material.name,
            error);
}

bool parseOutline(
    const Json& source,
    MToonSidecarMaterial& material,
    MToonSidecarError& error)
{
    const auto iterator =
        source.find("outline");

    if (iterator == source.end())
    {
        return true;
    }

    if (!iterator->is_object())
    {
        setError(
            error,
            material.name,
            "outline",
            "Expected an object.");

        return false;
    }

    return
        readBool(
            *iterator,
            "enabled",
            material.outlineEnabled,
            material.name,
            error) &&
        readOutlineWidthMode(
            *iterator,
            "widthMode",
            material.outlineWidthMode,
            material.name,
            error) &&
        readFloat(
            *iterator,
            "width",
            material.outlineWidth,
            0.0F,
            100.0F,
            material.name,
            error) &&
        readVector<3>(
            *iterator,
            "color",
            &material.outlineColor.x,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readFloat(
            *iterator,
            "lightingMix",
            material.outlineLightingMix,
            0.0F,
            1.0F,
            material.name,
            error);
}

bool parseMaterial(
    const Json& source,
    MToonSidecarMaterial& material,
    MToonSidecarError& error)
{
    if (!source.is_object())
    {
        setError(
            error,
            {},
            "materials",
            "Material entry must be an object.");

        return false;
    }

    const auto nameIterator =
        source.find("name");

    if (nameIterator == source.end() ||
        !nameIterator->is_string() ||
        nameIterator->get_ref<
            const std::string&>().empty())
    {
        setError(
            error,
            {},
            "name",
            "Material name is required.");

        return false;
    }

    material.name =
        nameIterator->get<std::string>();

    const bool parsed =
        readString(
            source,
            "outlineGroup",
            material.outlineGroup,
            material.name,
            error) &&
        readBool(
            source,
            "outlineDetectSelfDepth",
            material.outlineDetectSelfDepth,
            material.name,
            error) &&
        readBool(
            source,
            "outlineDetectSelfNormal",
            material.outlineDetectSelfNormal,
            material.name,
            error) &&
        readVector<4>(
            source,
            "baseColorFactor",
            &material.baseColorFactor.x,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readVector<3>(
            source,
            "shadeColor",
            &material.shadeColor.x,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "shadingShift",
            material.shadingShift,
            -1.0F,
            1.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "shadingShiftTextureScale",
            material.shadingShiftTextureScale,
            -2.0F,
            2.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "shadingToony",
            material.shadingToony,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "normalScale",
            material.normalScale,
            0.0F,
            2.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "surfaceOffset",
            material.surfaceOffset,
            -0.01F,
            0.01F,
            material.name,
            error) &&
        readFloat(
            source,
            "shadowNormalInfluence",
            material.shadowNormalInfluence,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readBool(
            source,
            "receiveShadow",
            material.receiveShadow,
            material.name,
            error) &&
        readBool(
            source,
            "shadowCutoffEnabled",
            material.shadowCutoffEnabled,
            material.name,
            error) &&
        readFloat(
            source,
            "shadowCutoff",
            material.shadowCutoff,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readBool(
            source,
            "sphericalFaceNormalEnabled",
            material.sphericalFaceNormalEnabled,
            material.name,
            error) &&
        readVector<3>(
            source,
            "sphericalFaceNormalCenter",
            &material.sphericalFaceNormalCenter.x,
            -1000000.0F,
            1000000.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "sphericalFaceNormalRadius",
            material.sphericalFaceNormalRadius,
            0.0001F,
            1000000.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "sphericalFaceNormalSoftness",
            material.sphericalFaceNormalSoftness,
            0.0F,
            1000000.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "sphericalFaceNormalBlend",
            material.sphericalFaceNormalBlend,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "giEqualization",
            material.giEqualization,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "occlusionStrength",
            material.occlusionStrength,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readVector<3>(
            source,
            "specularColor",
            &material.specularColor.x,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "specularStrength",
            material.specularStrength,
            0.0F,
            4.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "specularPower",
            material.specularPower,
            1.0F,
            256.0F,
            material.name,
            error) &&
        readVector<3>(
            source,
            "matcapColor",
            &material.matcapColor.x,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "matcapStrength",
            material.matcapStrength,
            0.0F,
            4.0F,
            material.name,
            error) &&
        readVector<3>(
            source,
            "rimColor",
            &material.rimColor.x,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "rimFresnelPower",
            material.rimFresnelPower,
            0.1F,
            16.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "rimLift",
            material.rimLift,
            -1.0F,
            1.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "rimLightingMix",
            material.rimLightingMix,
            0.0F,
            1.0F,
            material.name,
            error) &&
        readVector<3>(
            source,
            "emissionColor",
            &material.emissionColor.x,
            0.0F,
            100.0F,
            material.name,
            error) &&
        readFloat(
            source,
            "emissionStrength",
            material.emissionStrength,
            0.0F,
            10.0F,
            material.name,
            error) &&
        parseOutline(
            source,
            material,
            error) &&
        parseTexturePaths(
            source,
            material,
            error);

    return parsed &&
        parseScreenOutline(
            source,
            material,
            error);
}

Json sidecarToJson(
    const MToonMaterialSidecar& sidecar)
{
    Json materials =
        Json::array();

    for (const MToonSidecarMaterial& material :
         sidecar.materials)
    {
        materials.push_back(
            materialToJson(material));
    }

    return Json{
        {"version", sidecar.version},
        {"materials", std::move(materials)}
    };
}

bool parseSidecarJson(
    const Json& root,
    MToonMaterialSidecar& destination,
    MToonSidecarError& error)
{
    if (!root.is_object())
    {
        setError(
            error,
            {},
            {},
            "Sidecar root must be an object.");

        return false;
    }

    const auto versionIterator =
        root.find("version");

    if (versionIterator == root.end() ||
        !versionIterator->is_number_unsigned())
    {
        setError(
            error,
            {},
            "version",
            "Missing or invalid sidecar version.");

        return false;
    }

    const std::uint32_t sourceVersion =
        versionIterator->get<std::uint32_t>();

    if (sourceVersion <
            MToonMaterialSidecar::minimumSupportedVersion ||
        sourceVersion >
            MToonMaterialSidecar::currentVersion)
    {
        setError(
            error,
            {},
            "version",
            "Unsupported sidecar version.");

        return false;
    }

    const auto materialsIterator =
        root.find("materials");

    if (materialsIterator == root.end() ||
        !materialsIterator->is_array())
    {
        setError(
            error,
            {},
            "materials",
            "Expected a material array.");

        return false;
    }

    MToonMaterialSidecar parsed;

    parsed.version =
        MToonMaterialSidecar::currentVersion;

    std::unordered_set<std::string>
        materialNames;

    for (const Json& materialJson :
         *materialsIterator)
    {
        MToonSidecarMaterial material;

        if (!parseMaterial(
                materialJson,
                material,
                error))
        {
            return false;
        }

        if (!materialNames.emplace(
                material.name).second)
        {
            setError(
                error,
                material.name,
                "name",
                "Duplicate material name.");

            return false;
        }

        parsed.materials.push_back(
            std::move(material));
    }

    destination = std::move(parsed);
    return true;
}

} // namespace

bool serializeMToonSidecar(
    const MToonMaterialSidecar& sidecar,
    std::string& destination,
    MToonSidecarError& error)
{
    error.clear();

    if (sidecar.version !=
        MToonMaterialSidecar::currentVersion)
    {
        setError(
            error,
            {},
            "version",
            "Unsupported sidecar version.");

        return false;
    }

    try
    {
        MToonMaterialSidecar validated;

        if (!parseSidecarJson(
                sidecarToJson(sidecar),
                validated,
                error))
        {
            return false;
        }

        destination =
            sidecarToJson(validated).dump(2);

        return true;
    }
    catch (const std::exception& exception)
    {
        setError(
            error,
            {},
            {},
            exception.what());

        return false;
    }
}

bool deserializeMToonSidecar(
    const std::string_view source,
    MToonMaterialSidecar& destination,
    MToonSidecarError& error)
{
    error.clear();

    try
    {
        const Json root =
            Json::parse(
                source.begin(),
                source.end());

        MToonMaterialSidecar parsed;

        if (!parseSidecarJson(
                root,
                parsed,
                error))
        {
            return false;
        }

        destination = std::move(parsed);
        return true;
    }
    catch (const std::exception& exception)
    {
        setError(
            error,
            {},
            {},
            exception.what());

        return false;
    }
}

bool loadMToonSidecarFile(
    const std::filesystem::path& path,
    MToonMaterialSidecar& destination,
    MToonSidecarError& error)
{
    error.clear();

    std::ifstream input{
        path,
        std::ios::binary
    };

    if (!input.is_open())
    {
        setError(
            error,
            {},
            {},
            "Failed to open sidecar file: " +
                path.string());

        return false;
    }

    const std::string source{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };

    if (input.bad())
    {
        setError(
            error,
            {},
            {},
            "Failed to read sidecar file: " +
                path.string());

        return false;
    }

    return deserializeMToonSidecar(
        source,
        destination,
        error);
}

bool saveMToonSidecarFile(
    const std::filesystem::path& path,
    const MToonMaterialSidecar& sidecar,
    MToonSidecarError& error)
{
    error.clear();

    std::string serialized;

    if (!serializeMToonSidecar(
            sidecar,
            serialized,
            error))
    {
        return false;
    }

    std::filesystem::path temporaryPath = path;
    temporaryPath += ".tmp";

    std::filesystem::path backupPath = path;
    backupPath += ".bak";

    {
        std::ofstream output{
            temporaryPath,
            std::ios::binary |
                std::ios::trunc
        };

        if (!output.is_open())
        {
            setError(
                error,
                {},
                {},
                "Failed to open temporary sidecar file: " +
                    temporaryPath.string());

            return false;
        }

        output.write(
            serialized.data(),
            static_cast<std::streamsize>(
                serialized.size()));

        output.close();

        if (!output)
        {
            std::error_code cleanupError;

            std::filesystem::remove(
                temporaryPath,
                cleanupError);

            setError(
                error,
                {},
                {},
                "Failed to write temporary sidecar file: " +
                    temporaryPath.string());

            return false;
        }
    }

    std::error_code filesystemError;

    const bool destinationExists =
        std::filesystem::exists(
            path,
            filesystemError);

    if (filesystemError)
    {
        std::error_code cleanupError;

        std::filesystem::remove(
            temporaryPath,
            cleanupError);

        setError(
            error,
            {},
            {},
            "Failed to inspect sidecar destination: " +
                path.string());

        return false;
    }

    if (destinationExists)
    {
        std::filesystem::remove(
            backupPath,
            filesystemError);

        filesystemError.clear();

        std::filesystem::rename(
            path,
            backupPath,
            filesystemError);

        if (filesystemError)
        {
            std::error_code cleanupError;

            std::filesystem::remove(
                temporaryPath,
                cleanupError);

            setError(
                error,
                {},
                {},
                "Failed to prepare existing sidecar for replacement: " +
                    path.string());

            return false;
        }
    }

    filesystemError.clear();

    std::filesystem::rename(
        temporaryPath,
        path,
        filesystemError);

    if (filesystemError)
    {
        if (destinationExists)
        {
            std::error_code restoreError;

            std::filesystem::rename(
                backupPath,
                path,
                restoreError);
        }

        std::error_code cleanupError;

        std::filesystem::remove(
            temporaryPath,
            cleanupError);

        setError(
            error,
            {},
            {},
            "Failed to replace sidecar file: " +
                path.string());

        return false;
    }

    if (destinationExists)
    {
        std::error_code cleanupError;

        std::filesystem::remove(
            backupPath,
            cleanupError);
    }

    return true;
}

} // namespace stylized::material
