#include <render/FramePipeline.hpp>

#include <utility>

namespace stylized::render
{
    
bool FramePipeline::addPass(std::unique_ptr<IRenderPass> pass)
{
    if (!pass) return false;

    passes_.push_back(std::move(pass));
    return true;
}

bool FramePipeline::resize(const graphics::Extent2D extent)
{
    if (extent.width == 0 || extent.height == 0) return false;

    for (const std::unique_ptr<IRenderPass>& pass : passes_)
    {
        if (!pass->resize(extent)) return false;
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

    for (const std::unique_ptr<IRenderPass>& pass : passes_)
    {
        if (!pass->execute(frame))
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

    return passes_[index].get();
}

} // namespace stylized::render
