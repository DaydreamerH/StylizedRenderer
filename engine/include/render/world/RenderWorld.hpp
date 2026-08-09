#pragma once

#include <math/Bounds.hpp>
#include <math/Frustum.hpp>

#include <graphics/device/GraphicsTypes.hpp>

#include <cstdint>
#include <vector>
#include <cstddef>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace stylized::material
{

struct MaterialInstance;

} // namespace stylized::material

namespace stylized::render
{

class RuntimeMeshPrimitive;
class RuntimeMaterial;

enum class RenderMaterialClass : std::uint8_t
{
    Opaque,
    Masked,
    Transparent
};

enum class RenderItemFlags : std::uint32_t
{
    None = 0,
    CastShadow = 1u << 0,
    ReceiveShadow = 1u << 1,
    DoubleSided = 1u << 2
};

constexpr RenderItemFlags operator|(
    const RenderItemFlags left,
    const RenderItemFlags right) noexcept
{
    return static_cast<RenderItemFlags>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr RenderItemFlags operator&(
    const RenderItemFlags left,
    const RenderItemFlags right) noexcept
{
    return static_cast<RenderItemFlags>(
        static_cast<std::uint32_t>(left) &
        static_cast<std::uint32_t>(right));
}

constexpr bool hasFlag(
    const RenderItemFlags value,
    const RenderItemFlags flag) noexcept
{
    return (value & flag) != RenderItemFlags::None;
}

struct DirectionalLightData
{
    glm::vec3 direction{0.0F, -1.0F, 0.0F};
    glm::vec3 color{1.0F};
    float intensity = 1.0F;
};

struct EnvironmentLightData
{
    glm::vec3 skyColor{
        0.04F,
        0.05F,
        0.07F
    };

    glm::vec3 groundColor{
        0.015F,
        0.012F,
        0.01F
    };

    float intensity = 1.0F;
};

struct ShadowView
{
    glm::mat4 viewProjection{1.0F};

    graphics::Extent2D extent{
        2048,
        2048
    };
};

struct ShadowRenderItem
{
    const RuntimeMeshPrimitive* primitive = nullptr;

    glm::mat4 world{1.0F};
};

struct RenderView
{
    glm::mat4 view{1.F};
    glm::mat4 projection{1.F};
    glm::mat4 viewProjection{1.F};
    
    glm::vec3 cameraPosition{0.F};

    graphics::Extent2D viewport{};

    float exposure = 1.F;

    DirectionalLightData mainLight;

    EnvironmentLightData environmentLight;

    math::Frustum frustum;
};

struct RenderItem
{
    const RuntimeMeshPrimitive* primitive = nullptr;

    const material::MaterialInstance* materialInstance = nullptr;

    RuntimeMaterial* runtimeMaterial = nullptr;

    glm::mat4 world{1.0F};
    glm::mat3 normalMatrix{1.0F};

    math::Bounds worldBounds;

    std::uint32_t objectId = 0;

    RenderMaterialClass materialClass = 
        RenderMaterialClass::Opaque;
    
    RenderItemFlags flags = 
        RenderItemFlags::CastShadow | RenderItemFlags::ReceiveShadow;
};

struct RenderStats
{
    std::size_t totalItems = 0;
    std::size_t visibleItems = 0;
    std::size_t culledItems = 0;
    std::size_t drawCalls = 0;

    std::size_t opaqueItems = 0;
    std::size_t maskedItems = 0;
    std::size_t transparentItems = 0;
};

struct RenderWorld
{
    RenderView mainView;
    ShadowView shadowView;

    std::vector<RenderItem> items;
    std::vector<ShadowRenderItem> shadowItems;

    RenderStats renderStats;

    void clear() noexcept
    {
        items.clear();
        shadowItems.clear();

        renderStats = {};
        shadowView = {};
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return items.empty();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return items.size();
    }

};

} // namespace stylized::render
