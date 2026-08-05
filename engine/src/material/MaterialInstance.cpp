#include <material/MaterialInstance.hpp>

#include <asset/MaterialAsset.hpp>

namespace stylized::material
{

MaterialInstance makeMaterialInstance(
    const asset::AssetHandle<MaterialTemplate>
        templateHandle,
    const asset::MaterialAsset& source)
{
    if (templateHandle.isNull())
    {
        return {};
    }

    MaterialInstance instance;

    instance.templateHandle =
        templateHandle;

    instance.baseColorFactor =
        source.baseColorFactor;

    instance.baseColorTexture =
        source.baseColorTexture;

    return instance;
}

} // namespace stylized::material