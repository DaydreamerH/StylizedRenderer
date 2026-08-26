#pragma once

#include <graphics/device/GraphicsTypes.hpp>

namespace stylized::graphics
{

class RenderTexture;
class DepthTexture;
class Framebuffer;

} // namespace stylized::graphics

namespace stylized::render
{

struct RenderWorld;

struct FrameContext
{
    graphics::Extent2D framebufferSize{};

    RenderWorld* renderWorld = nullptr;

    graphics::RenderTexture* hdrColor = nullptr;
    graphics::RenderTexture* ldrColor = nullptr;
    graphics::RenderTexture* normal = nullptr;
    graphics::RenderTexture* materialId = nullptr;
    graphics::RenderTexture* outlineMask = nullptr;
    graphics::DepthTexture* depth = nullptr;
    graphics::Framebuffer* framebuffer = nullptr;

    graphics::DepthTexture* shadowMap = nullptr;
    graphics::RenderTexture* faceHairShadowMask = nullptr;

    float deltaTime = 0.0F;
    bool shadowsEnabled = true;
    float exposure = 1.0F;
    bool toneMappingEnabled = true;
    bool fxaaEnabled = true;
};

} // namespace stylized::render
