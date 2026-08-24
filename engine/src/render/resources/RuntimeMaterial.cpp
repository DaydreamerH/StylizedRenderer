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
        if (!shader_.setInt(
                "uBaseColorTexture",
                0))
        {
            shader_ = {};
        }
        break;
    case material::MaterialKind::MToon:
        if (!shader_.setInt(
                "uBaseColorTexture",
                0) ||
            !shader_.setInt(
                "uShadowMap",
                1
            ) ||
            !shader_.setInt(
                "uNormalTexture",
                2
            ) ||
            !shader_.setInt(
                "uShadeTexture",
                3
            ) ||
            !shader_.setInt(
                "uShadingShiftTexture",
                4
            ) ||
            !shader_.setInt(
                "uMatcapTexture",
                5
            ) ||
            !shader_.setInt(
                "uRimMaskTexture",
                6
            ) ||
            !shader_.setInt(
                "uEmissionTexture",
                7
            ) ||
            !shader_.setInt(
                "uToonRampTexture",
                8) ||
            !shader_.setInt(
                "uOcclusionTexture",
                9
            ) ||
            !shader_.setInt(
                "uSpecularTexture",
                10
            ))
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
    {
        const graphics::Texture2D& baseColorTexture =
            resourceCache.getOrCreateTexture(
                instance.baseColorTexture,
                assetRegistry);

        if (!baseColorTexture.isValid())
        {
            return false;
        }

        if (!shader_.setVec4(
                "uBaseColorFactor",
                instance.baseColorFactor))
        {
            return false;
        }

        baseColorTexture.bind(0);

        return true;
    }

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

        const graphics::Texture2D& shadeTexture =
            parameters.textures.shadeTexture.isNull()
            ? resourceCache.whiteTexture()
            : resourceCache.getOrCreateTexture(
                parameters.textures.shadeTexture,
                assetRegistry
            );

        const graphics::Texture2D& shadingShiftTexture =
            parameters.textures.shadingShiftTexture.isNull()
            ? resourceCache.blackTexture()
            : resourceCache.getOrCreateTexture(
                parameters.textures.shadingShiftTexture,
                assetRegistry
            );

        const graphics::Texture2D& toonRampTexture =
            parameters.textures.toonRampTexture.isNull()
            ? resourceCache.whiteTexture()
            : resourceCache.getOrCreateTexture(
                parameters.textures.toonRampTexture,
                assetRegistry
            );

        const graphics::Texture2D& normalTexture =
            parameters.textures.normalTexture.isNull()
            ? resourceCache.neutralNormalTexture()
            : resourceCache.getOrCreateTexture(
                parameters.textures.normalTexture,
                assetRegistry
            );

        const graphics::Texture2D& matcapTexture =
            parameters.textures.matcapTexture.isNull()
            ? resourceCache.blackTexture()
            : resourceCache.getOrCreateTexture(
                parameters.textures.matcapTexture,
                assetRegistry
            );

        const graphics::Texture2D& rimMaskTexture =
            parameters.textures.rimMaskTexture.isNull()
            ? resourceCache.whiteTexture()
            : resourceCache.getOrCreateTexture(
                parameters.textures.rimMaskTexture,
                assetRegistry
            );

        const graphics::Texture2D& emissionTexture =
            parameters.textures.emissionTexture.isNull()
            ? resourceCache.blackTexture()
            : resourceCache.getOrCreateTexture(
                parameters.textures.emissionTexture,
                assetRegistry
            );

        const graphics::Texture2D& occlusionTexture =
            parameters.textures.occlusionTexture.isNull()
            ? resourceCache.whiteTexture()
            : resourceCache.getOrCreateTexture(
                parameters.textures.occlusionTexture,
                assetRegistry
            );

        const graphics::Texture2D& specularTexture =
            parameters.textures.specularTexture.isNull()
            ? resourceCache.blackTexture()
            : resourceCache.getOrCreateTexture(
                parameters.textures.specularTexture,
                assetRegistry
            );

        if (!shadeTexture.isValid() ||
            !toonRampTexture.isValid() ||
            !shadingShiftTexture.isValid() ||
            !normalTexture.isValid() ||
            !matcapTexture.isValid() ||
            !rimMaskTexture.isValid() ||
            !emissionTexture.isValid() ||
            !occlusionTexture.isValid() ||
            !specularTexture.isValid())
        {
            return false;
        }

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

        if (!shader_.setFloat(
                "uShadingShiftTextureScale",
                parameters.shadingShiftTextureScale))
        {
            return false;
        }

        if (!shader_.setFloat(
            "uNormalScale",
            parameters.normalScale
        ))
        {
            return false;
        }

        if (!shader_.setFloat(
                "uSurfaceOffset",
                parameters.surfaceOffset))
        {
            return false;
        }

        if (!shader_.setFloat(
                "uShadowNormalInfluence",
                parameters.shadowNormalInfluence))
        {
            return false;
        }

        if (!shader_.setFloat(
                "uShadowCutoff",
                parameters.shadowCutoff))
        {
            return false;
        }

        if (!shader_.setInt(
                "uShadowCutoffEnabled",
                parameters.shadowCutoffEnabled ? 1 : 0))
        {
            return false;
        }

        if (!shader_.setInt(
                "uSphericalFaceNormalEnabled",
                parameters.sphericalFaceNormalEnabled ? 1 : 0) ||
            !shader_.setVec3(
                "uSphericalFaceNormalCenter",
                parameters.sphericalFaceNormalCenter) ||
            !shader_.setFloat(
                "uSphericalFaceNormalRadius",
                parameters.sphericalFaceNormalRadius) ||
            !shader_.setFloat(
                "uSphericalFaceNormalSoftness",
                parameters.sphericalFaceNormalSoftness) ||
            !shader_.setFloat(
                "uSphericalFaceNormalBlend",
                parameters.sphericalFaceNormalBlend))
        {
            return false;
        }

        if (!shader_.setFloat(
            "uGiEqualization",
            parameters.giEqualization
        ))
        {
            return false;
        }

        if (!shader_.setVec3(
            "uMatcapColor",
            parameters.matcapColor.x,
            parameters.matcapColor.y,
            parameters.matcapColor.z))
        {
            return false;
        }

        if (!shader_.setFloat(
            "uMatcapStrength",
            parameters.matcapStrength))
        {
            return false;
        }

        if (!shader_.setVec3(
            "uRimColor",
            parameters.rimColor.x,
            parameters.rimColor.y,
            parameters.rimColor.z))
        {
            return false;
        }

        if (!shader_.setFloat(
            "uRimFresnelPower",
            parameters.rimFresnelPower))
        {
            return false;
        }

        if (!shader_.setFloat(
            "uRimLift",
            parameters.rimLift))
        {
            return false;
        }

        if (!shader_.setFloat(
            "uRimLightingMix",
            parameters.rimLightingMix))
        {
            return false;
        }

        if (!shader_.setVec3(
            "uEmissionColor",
            parameters.emissionColor.x,
            parameters.emissionColor.y,
            parameters.emissionColor.z))
        {
            return false;
        }

        if (!shader_.setFloat(
            "uEmissionStrength",
            parameters.emissionStrength))
        {
            return false;
        }

        if (!shader_.setFloat(
            "uOcclusionStrength",
            parameters.occlusionStrength
        ))
        {
            return false;
        }

        if (!shader_.setVec3(
                "uSpecularColor",
                parameters.specularColor) ||
            !shader_.setFloat(
                "uSpecularStrength",
                parameters.specularStrength) ||
            !shader_.setFloat(
                "uSpecularPower",
                parameters.specularPower))
        {
            return false;
        }

        baseColorTexture.bind(0);
        normalTexture.bind(2);
        shadeTexture.bind(3);
        shadingShiftTexture.bind(4);
        matcapTexture.bind(5);
        rimMaskTexture.bind(6);
        emissionTexture.bind(7);
        toonRampTexture.bind(8);
        occlusionTexture.bind(9);
        specularTexture.bind(10);

        return true;
    }
    }

    return false;
}

} // namespace stylized::render
