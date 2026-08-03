#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/GraphicsTypes.hpp>
#include <render/IRenderPass.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace stylized::render
{
    
class FramePipeline final : public core::NonCopyable
{
public:
    FramePipeline() = default;
    ~FramePipeline() = default;

    [[nodiscard]] bool addPass(std::unique_ptr<IRenderPass> pass);
    [[nodiscard]] bool resize(graphics::Extent2D extent);
    [[nodiscard]] bool execute(FrameContext& frame);

    void clear() noexcept;

    [[nodiscard]] std::size_t passCount() const noexcept;
    [[nodiscard]] const IRenderPass* passAt(std::size_t index) const noexcept;

private:
    std::vector<std::unique_ptr<IRenderPass>> passes_;

    graphics::Extent2D extent_{};
    bool hasValidExtent_ = false;
};

} // namespace stylized::render
