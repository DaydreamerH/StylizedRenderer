#include <render/pipeline/FramePipeline.hpp>

#include <graphics/GraphicsDevice.hpp>

#include <algorithm>
#include <utility>

namespace stylized::render
{

FramePipeline::FramePipeline(
    graphics::GraphicsDevice& graphicsDevice) noexcept
    : graphicsDevice_(graphicsDevice)
{
}

bool FramePipeline::addPass(std::unique_ptr<IRenderPass> pass)
{
    if (!pass) return false;

    PassEntry entry;
    entry.pass = std::move(pass);
    entry.timer =
        graphicsDevice_.createGpuTimerQuery();

    passes_.push_back(std::move(entry));
    return true;
}

bool FramePipeline::resize(const graphics::Extent2D extent)
{
    if (extent.width == 0 || extent.height == 0) return false;

    for (PassEntry& entry : passes_)
    {
        if (!entry.pass->resize(extent)) return false;
    }

    extent_ = extent;
    hasValidExtent_ = true;

    return true;
}

bool FramePipeline::execute(FrameContext& frame)
{
    if (!hasValidExtent_)
    {
        return false;
    }

    for (PassEntry& entry : passes_)
    {
        const bool timingStarted =
            entry.timer.begin();

        entry.lastExecutionSucceeded =
            entry.pass->execute(frame);

        if (timingStarted)
        {
            entry.timer.end();
        }

        if (!entry.lastExecutionSucceeded)
        {
            return false;
        }
    }

    return true;
}

void FramePipeline::clear() noexcept
{
    passes_.clear();
    extent_ = {};
    hasValidExtent_ = false;
}

std::size_t FramePipeline::passCount()
    const noexcept
{
    return passes_.size();
}

const IRenderPass* FramePipeline::passAt(const std::size_t index) const noexcept
{
    if (index >= passes_.size())
    {
        return nullptr;
    }

    return passes_[index].pass.get();
}

bool FramePipeline::passLastExecutionSucceeded(
    const std::size_t index) const noexcept
{
    return index < passes_.size() &&
        passes_[index].lastExecutionSucceeded;
}

bool FramePipeline::passHasGpuTime(
    const std::size_t index) const noexcept
{
    return index < passes_.size() &&
        passes_[index].timer.hasResult();
}

double FramePipeline::passGpuTimeMilliseconds(
    const std::size_t index) const noexcept
{
    if (index >= passes_.size())
    {
        return 0.0;
    }

    return passes_[index].timer.elapsedMilliseconds();
}

bool FramePipeline::hasCompleteGpuTiming() const noexcept
{
    return !passes_.empty() &&
        std::all_of(
            passes_.begin(),
            passes_.end(),
            [](const PassEntry& entry)
            {
                return entry.timer.hasResult();
            });
}

double FramePipeline::totalGpuTimeMilliseconds() const noexcept
{
    double totalMilliseconds = 0.0;

    for (const PassEntry& entry : passes_)
    {
        if (entry.timer.hasResult())
        {
            totalMilliseconds +=
                entry.timer.elapsedMilliseconds();
        }
    }

    return totalMilliseconds;
}

} // namespace stylized::render
