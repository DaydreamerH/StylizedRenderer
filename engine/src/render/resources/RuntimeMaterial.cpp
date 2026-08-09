#include <render/resources/RuntimeMaterial.hpp>

#include <asset/AssetRegistry.hpp>
#include <graphics/device/GraphicsDevice.hpp>
#include <graphics/resources/Texture2D.hpp>
#include <material/MaterialInstance.hpp>
#include <render/resources/RuntimeResourceCache.hpp>

namespace stylized::render
{
    
RuntimeMaterial::RuntimeMaterial(
    graphics::GraphicsDevice& graphicsDevice,
    const asset::AssetRegistry& assetRegistry,
    const asset::AssetHandle<material::MaterialTemplate> templateHandle
)
    : templateHandle_(templateHandle)
{
    const material::MaterialTemplate* materialTemplate =
        assetRegistry.get(templateHandle_);

    if (materialTemplate == nullptr ||
        !materialTemplate->isValid())
    {
        return;
    }

    kind_ = materialTemplate->kind;

    graphics::ShaderProgramDesc shaderDesc;
    shaderDesc.vertexShaderPath = materialTemplate->vertexShaderPath;
    shaderDesc.fragmentShaderPath = materialTemplate->fragmentShaderPath;
    shaderDesc.debugName = materialTemplate->name;

    shader_ = graphicsDevice.createShaderProgram(shaderDesc);

    if (!shader_.isValid()) return;

    switch (kind_)
    {
    case material::MaterialKind::BasicPbr:
        if (!shader_.setInt(
            "uBaseColorTexture",
            0) ||
        !shader_.setInt(
            "uShadowMap",
            1))
        {
            shader_ = {};
        }
        break;
    case material::MaterialKind::Unlit:
        if (!shader_.setInt("uBaseColorTexture", 0))
            shader_ = {};
        break;
    case material::MaterialKind::DebugNormal:
        break;
    case material::MaterialKind::MToon:
        if (!shader_.setInt(
                "uBaseColorTexture",
                0))
        {
            shader_ = {};
        }
        break;
    }
}

bool RuntimeMaterial::isValid() const noexcept
{
    return !templateHandle_.isNull() &&
        shader_.isValid();
}

material::MaterialKind
RuntimeMaterial::kind() const noexcept
{
    return kind_;
}

asset::AssetHandle<
    material::MaterialTemplate>
RuntimeMaterial::templateHandle() const noexcept
{
    return templateHandle_;
}

graphics::ShaderProgram*
RuntimeMaterial::shader() noexcept
{
    if (!isValid())
    {
        return nullptr;
    }

    return &shader_;
}

const graphics::ShaderProgram*
RuntimeMaterial::shader() const noexcept
{
    if (!isValid())
    {
        return nullptr;
    }

    return &shader_;
}

bool RuntimeMaterial::bind(
    const material::MaterialInstance& instance,
    RuntimeResourceCache& resourceCache,
    const asset::AssetRegistry& assetRegistry
)
{
    if (!isValid() ||
        !instance.isValid() ||
        instance.templateHandle != templateHandle_)
    {
        return false;
    }

    switch (kind_)
    {
    case material::MaterialKind::Unlit:
    {
        const graphics::Texture2D& baseColorTexture =
            resourceCache.getOrCreateTexture(
                instance.baseColorTexture,
                assetRegistry
            );
        
        if (!baseColorTexture.isValid()) return false;
        
        if (!shader_.setVec4(
            "uBaseColorFactor",
            instance.baseColorFactor
        ))
        {
            return false;
        }

        baseColorTexture.bind(0);
        return true;
    }
    case material::MaterialKind::DebugNormal:
        return true;

    case material::MaterialKind::BasicPbr:
    {
        const graphics::Texture2D& baseColorTexture =
            resourceCache.getOrCreateTexture(
                instance.baseColorTexture,
                assetRegistry
            );

        if (!baseColorTexture.isValid()) return false;

        const glm::vec4& baseColorFactor = instance.baseColorFactor;
        if (!shader_.setVec4(
                "uBaseColorFactor",
                baseColorFactor))
            return false;

        if (!shader_.setFloat(
                "uMetallic",
                instance.metallic))
        {
            return false;
        }

        if (!shader_.setFloat(
                "uRoughness",
                instance.roughness))
        {
            return false;
        }

        baseColorTexture.bind(0);
        return true;
    }

    case material::MaterialKind::MToon:
    {
        if (!instance.mtoonParameters.has_value())
        {
            return false;
        }

        const graphics::Texture2D& baseColorTexture =
            resourceCache.getOrCreateTexture(
                instance.baseColorTexture,
                assetRegistry);

        if (!baseColorTexture.isValid())
        {
            return false;
        }

        const material::MToonMaterialParameters&
            parameters =
                instance.mtoonParameters.value();

        if (!shader_.setVec4(
                "uBaseColorFactor",
                instance.baseColorFactor))
        {
            return false;
        }

        if (!shader_.setVec3(
                "uShadeColor",
                parameters.shadeColor.x,
                parameters.shadeColor.y,
                parameters.shadeColor.z))
        {
            return false;
        }

        if (!shader_.setFloat(
                "uShadingShift",
                parameters.shadingShift))
        {
            return false;
        }

        if (!shader_.setFloat(
                "uShadingToony",
                parameters.shadingToony))
        {
            return false;
        }

        baseColorTexture.bind(0);
        return true;
    }
    }

    return false;
}

} // namespace stylized::render
