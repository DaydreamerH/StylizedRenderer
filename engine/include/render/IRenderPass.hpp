#pragma once

#include <graphics/GraphicsTypes.hpp>
#include <render/FrameContext.hpp>

#include <string_view>

namespace stylized::render
{

class IRenderPass
{
public:
    virtual ~IRenderPass() = default;

    [[nodiscard]] virtual bool resize(
        graphics::Extent2D extent) = 0;

    [[nodiscard]] virtual bool execute(
        FrameContext& frame) = 0;

    [[nodiscard]] virtual std::string_view name()
        const noexcept = 0;
};

} // namespace stylized::render