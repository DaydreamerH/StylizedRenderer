#pragma once

#include <core/NonCopyable.hpp>
#include <graphics/GpuTimerQuery.hpp>
#include <graphics/GraphicsTypes.hpp>
#include <render/IRenderPass.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace stylized::graphics
{

class GraphicsDevice;

} // namespace stylized::graphics

namespace stylized::render
{
    
class FramePipeline final : public core::NonCopyable
{
public:
    explicit FramePipeline(
        graphics::GraphicsDevice& graphicsDevice) noexcept;

    ~FramePipeline() = default;

    [[nodiscard]] bool addPass(std::unique_ptr<IRenderPass> pass);
    [[nodiscard]] bool resize(graphics::Extent2D extent);
    [[nodiscard]] bool execute(FrameContext& frame);

    void clear() noexcept;

    [[nodiscard]] std::size_t passCount() const noexcept;
    [[nodiscard]] const IRenderPass* passAt(std::size_t index) const noexcept;

    [[nodiscard]] bool passLastExecutionSucceeded(
        std::size_t index) const noexcept;

    [[nodiscard]] bool passHasGpuTime(
        std::size_t index) const noexcept;

    [[nodiscard]] double passGpuTimeMilliseconds(
        std::size_t index) const noexcept;

    [[nodiscard]] bool hasCompleteGpuTiming() const noexcept;
    [[nodiscard]] double totalGpuTimeMilliseconds() const noexcept;

private:
    struct PassEntry
    {
        std::unique_ptr<IRenderPass> pass;
        graphics::GpuTimerQuery timer;
        bool lastExecutionSucceeded = false;
    };

    graphics::GraphicsDevice& graphicsDevice_;

    std::vector<PassEntry> passes_;

    graphics::Extent2D extent_{};
    bool hasValidExtent_ = false;
};

} // namespace stylized::render
