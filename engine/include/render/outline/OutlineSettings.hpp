#pragma once

#include <glm/vec3.hpp>

#include <cstdint>

namespace stylized::render
{

enum class GlobalOutlineMode : std::uint8_t
{
    Disabled = 0,
    World,
    Screen
};

enum class OutlineDebugView : std::uint8_t
{
    Final = 0,
    SurfaceNormal,
    LinearDepth,
    ShellOutlineMask,
    ScreenEdge,
    CombinedOutline
};

struct GlobalOutlineSettings
{
    GlobalOutlineMode mode =
        GlobalOutlineMode::Disabled;

    glm::vec3 color{0.0F};

    float worldWidth = 0.01F;
    float screenWidth = 1.0F;

    float depthThreshold = 0.01F;
    float normalThreshold = 0.2F;

    OutlineDebugView debugView =
        OutlineDebugView::Final;
};

} // namespace stylized::render
