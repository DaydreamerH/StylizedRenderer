#include <graphics/resources/GpuTimerQuery.hpp>

#include <graphics/device/GraphicsDevice.hpp>

#include <glad/gl.h>

#include <algorithm>
#include <utility>

namespace stylized::graphics
{

GpuTimerQuery::GpuTimerQuery(
    const GraphicsDevice&) noexcept
{
    glCreateQueries(
        GL_TIME_ELAPSED,
        static_cast<GLsizei>(queryCount),
        ids_.data());

    if (std::any_of(
            ids_.begin(),
            ids_.end(),
            [](const std::uint32_t id)
            {
                return id == 0;
            }))
    {
        release();
    }
}

GpuTimerQuery::GpuTimerQuery(
    GpuTimerQuery&& other) noexcept
    : ids_(std::exchange(other.ids_, {})),
      pending_(std::exchange(other.pending_, {})),
      nextQueryIndex_(std::exchange(
          other.nextQueryIndex_,
          0)),
      activeQueryIndex_(std::exchange(
          other.activeQueryIndex_,
          invalidQueryIndex)),
      elapsedMilliseconds_(std::exchange(
          other.elapsedMilliseconds_,
          0.0)),
      hasResult_(std::exchange(
          other.hasResult_,
          false))
{
}

GpuTimerQuery& GpuTimerQuery::operator=(
    GpuTimerQuery&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    release();

    ids_ = std::exchange(other.ids_, {});
    pending_ = std::exchange(other.pending_, {});
    nextQueryIndex_ = std::exchange(
        other.nextQueryIndex_,
        0);
    activeQueryIndex_ = std::exchange(
        other.activeQueryIndex_,
        invalidQueryIndex);
    elapsedMilliseconds_ = std::exchange(
        other.elapsedMilliseconds_,
        0.0);
    hasResult_ = std::exchange(
        other.hasResult_,
        false);

    return *this;
}

GpuTimerQuery::~GpuTimerQuery()
{
    release();
}

bool GpuTimerQuery::isValid() const noexcept
{
    return std::all_of(
        ids_.begin(),
        ids_.end(),
        [](const std::uint32_t id)
        {
            return id != 0;
        });
}

bool GpuTimerQuery::begin() noexcept
{
    if (!isValid() ||
        activeQueryIndex_ != invalidQueryIndex)
    {
        return false;
    }

    if (pending_[nextQueryIndex_])
    {
        GLint resultAvailable = GL_FALSE;

        glGetQueryObjectiv(
            ids_[nextQueryIndex_],
            GL_QUERY_RESULT_AVAILABLE,
            &resultAvailable);

        if (resultAvailable == GL_FALSE)
        {
            return false;
        }

        GLuint64 elapsedNanoseconds = 0;

        glGetQueryObjectui64v(
            ids_[nextQueryIndex_],
            GL_QUERY_RESULT,
            &elapsedNanoseconds);

        elapsedMilliseconds_ =
            static_cast<double>(elapsedNanoseconds) /
            1'000'000.0;

        hasResult_ = true;
        pending_[nextQueryIndex_] = false;
    }

    glBeginQuery(
        GL_TIME_ELAPSED,
        ids_[nextQueryIndex_]);

    activeQueryIndex_ = nextQueryIndex_;

    return true;
}

void GpuTimerQuery::end() noexcept
{
    if (activeQueryIndex_ == invalidQueryIndex)
    {
        return;
    }

    glEndQuery(GL_TIME_ELAPSED);

    pending_[activeQueryIndex_] = true;

    nextQueryIndex_ =
        (activeQueryIndex_ + 1) % queryCount;

    activeQueryIndex_ = invalidQueryIndex;
}

bool GpuTimerQuery::hasResult() const noexcept
{
    return hasResult_;
}

double GpuTimerQuery::elapsedMilliseconds() const noexcept
{
    return elapsedMilliseconds_;
}

void GpuTimerQuery::release() noexcept
{
    if (activeQueryIndex_ != invalidQueryIndex)
    {
        glEndQuery(GL_TIME_ELAPSED);
        activeQueryIndex_ = invalidQueryIndex;
    }

    if (std::any_of(
            ids_.begin(),
            ids_.end(),
            [](const std::uint32_t id)
            {
                return id != 0;
            }))
    {
        glDeleteQueries(
            static_cast<GLsizei>(queryCount),
            ids_.data());
    }

    ids_ = {};
    pending_ = {};
    nextQueryIndex_ = 0;
    elapsedMilliseconds_ = 0.0;
    hasResult_ = false;
}

} // namespace stylized::graphics
