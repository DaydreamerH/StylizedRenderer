#include <material/MaterialInstance.hpp>

#include <asset/MaterialAsset.hpp>

namespace stylized::material
{

MaterialInstance makeMaterialInstance(
    const asset::AssetHandle<MaterialTemplate> templateHandle,
    const MaterialKind kind,
    const asset::MaterialAsset* source)
{
    if (templateHandle.isNull())
    {
        return {};
    }

    MaterialInstance instance;
    instance.templateHandle = templateHandle;

    if (kind == MaterialKind::MToon)
    {
        instance.mtoonParameters.emplace();
    }

    if (source == nullptr)
    {
        return instance;
    }

    instance.baseColorFactor =
        source->baseColorFactor;

    instance.alphaCutoff =
        source->alphaCutoff;

    instance.baseColorTexture =
        source->baseColorTexture;

    instance.metallic =
        source->metallicFactor;

    instance.roughness =
        source->roughnessFactor;

    if (instance.mtoonParameters.has_value())
    {
        MToonMaterialParameters& parameters =
            instance.mtoonParameters.value();

        parameters.shadeColor =
            glm::vec3{source->baseColorFactor} * 0.45F;

        parameters.textures.shadeTexture =
            source->baseColorTexture;

        parameters.normalScale =
            source->normalScale;

        parameters.textures.normalTexture =
            source->normalTexture;
    }

    return instance;
}

} // namespace stylized::material
