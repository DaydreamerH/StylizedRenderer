#pragma once

#include <graphics/GraphicsTypes.hpp>

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
    graphics::DepthTexture* depth = nullptr;
    graphics::Framebuffer* framebuffer = nullptr;

    float deltaTime = 0.0F;
    float exposure = 1.0F;
};

} // namespace stylized::render