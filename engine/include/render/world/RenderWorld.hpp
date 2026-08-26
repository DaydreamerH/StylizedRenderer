#pragma once

#include <math/Bounds.hpp>
#include <math/Frustum.hpp>

#include <graphics/device/GraphicsTypes.hpp>

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace stylized::material
{

struct MaterialInstance;

} // namespace stylized::material

namespace stylized::graphics
{

class VertexArray;

} // namespace stylized::graphics

namespace stylized::render
{

class RuntimeMeshPrimitive;
class RuntimeMaterial;
class SkinningPalette;

enum class MToonDebugView : std::uint8_t
{
    Final = 0,
    Base,
    Shade,
    Lighting,
    Rim,
    MatCap,
    Emission
};

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
        0.075F,
        0.045F,
        0.065F
    };

    glm::vec3 groundColor{
        0.025F,
        0.012F,
        0.020F
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

    const graphics::VertexArray* vertexArray = nullptr;

    const material::MaterialInstance* materialInstance = nullptr;

    const SkinningPalette* skinningPalette = nullptr;

    glm::mat4 world{1.0F};

    RenderMaterialClass materialClass = RenderMaterialClass::Opaque;
    bool excludeFromFaceFilteredShadow = false;
};

struct FaceHairShadowRenderItem
{
    const RuntimeMeshPrimitive* primitive = nullptr;
    const graphics::VertexArray* vertexArray = nullptr;
    const material::MaterialInstance* materialInstance = nullptr;
    const SkinningPalette* skinningPalette = nullptr;
    glm::mat4 world{1.0F};
};

struct FaceHairShadowView
{
    bool valid = false;
    glm::mat4 viewProjection{1.0F};
    graphics::Extent2D extent{512, 512};
    glm::vec2 uvOffset{0.0F};
    float alphaCutoff = 0.72F;
    float softness = 0.004F;
    float strength = 0.8F;
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

    MToonDebugView mtoonDebugView =
        MToonDebugView::Final;

    math::Frustum frustum;

    float nearPlane = 0.1F;
    float farPlane = 2000.0F;
};

struct RenderItem
{
    const RuntimeMeshPrimitive* primitive = nullptr;

    const graphics::VertexArray* vertexArray = nullptr;

    const material::MaterialInstance* materialInstance = nullptr;

    RuntimeMaterial* runtimeMaterial = nullptr;

    const SkinningPalette* skinningPalette = nullptr;

    glm::mat4 world{1.0F};
    glm::mat3 normalMatrix{1.0F};

    bool faceSdfFrameValid = false;
    bool receivesFaceHairShadow = false;
    glm::vec3 faceForward{0.0F, 0.0F, 1.0F};
    glm::vec3 faceRight{1.0F, 0.0F, 0.0F};
    glm::vec3 faceUp{0.0F, 1.0F, 0.0F};

    math::Bounds worldBounds;

    std::uint32_t objectId = 0;

    // Zero is reserved for the background. Visible materials receive a
    // compact per-frame screen-outline policy index.
    std::uint32_t outlinePolicyIndex = 0;

    RenderMaterialClass materialClass = 
        RenderMaterialClass::Opaque;
    
    RenderItemFlags flags = 
        RenderItemFlags::CastShadow | RenderItemFlags::ReceiveShadow;
};

struct ScreenOutlinePolicy
{
    const material::MaterialInstance* materialInstance = nullptr;
    std::uint32_t groupId = 0;
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
    FaceHairShadowView faceHairShadowView;

    math::Bounds shadowCasterBounds;
    math::Bounds shadowReceiverBounds;

    std::vector<RenderItem> items;
    std::vector<ShadowRenderItem> shadowItems;
    std::vector<FaceHairShadowRenderItem> faceHairShadowItems;
    std::vector<ScreenOutlinePolicy> outlinePolicies;

    std::unordered_map<
        const material::MaterialInstance*,
        std::uint32_t> outlinePolicyIndices;

    std::unordered_map<std::string, std::uint32_t>
        outlineGroupIds;

    RenderStats renderStats;

    std::uint32_t nextObjectId = 0;

    void clear() noexcept
    {
        items.clear();
        shadowItems.clear();
        faceHairShadowItems.clear();
        outlinePolicies.clear();
        outlinePolicyIndices.clear();
        outlineGroupIds.clear();

        shadowCasterBounds = {};
        shadowReceiverBounds = {};

        renderStats = {};
        shadowView = {};
        faceHairShadowView = {};

        nextObjectId = 0;
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
