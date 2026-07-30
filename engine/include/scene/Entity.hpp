#pragma once

#include <cstdint>
#include <limits>

namespace stylized::scene
{
    
struct EntityId
{
    static constexpr std::uint32_t invalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index = invalidIndex;
    std::uint32_t generation = 0;

    [[nodiscard]] bool isNull() const noexcept
    {
        return index == invalidIndex;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return !isNull();
    }

    [[nodiscard]] friend bool operator==(const EntityId&, const EntityId&) noexcept = default;
};


} // namespace stylized::scene
