#pragma once

#include <core/NonCopyable.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace stylized::graphics
{

class GraphicsDevice;

class GpuTimerQuery final : public core::NonCopyable
{
public:
    GpuTimerQuery() = default;

    GpuTimerQuery(GpuTimerQuery&& other) noexcept;
    GpuTimerQuery& operator=(GpuTimerQuery&& other) noexcept;

    ~GpuTimerQuery();

    [[nodiscard]] bool isValid() const noexcept;

    [[nodiscard]] bool begin() noexcept;
    void end() noexcept;

    [[nodiscard]] bool hasResult() const noexcept;
    [[nodiscard]] double elapsedMilliseconds() const noexcept;

private:
    friend class GraphicsDevice;

    explicit GpuTimerQuery(
        const GraphicsDevice& graphicsDevice) noexcept;

    void release() noexcept;

    static constexpr std::size_t queryCount = 3;
    static constexpr std::size_t invalidQueryIndex = queryCount;

    std::array<std::uint32_t, queryCount> ids_{};
    std::array<bool, queryCount> pending_{};

    std::size_t nextQueryIndex_ = 0;
    std::size_t activeQueryIndex_ = invalidQueryIndex;

    double elapsedMilliseconds_ = 0.0;
    bool hasResult_ = false;
};

} // namespace stylized::graphics
