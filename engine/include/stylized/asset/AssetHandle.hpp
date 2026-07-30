#pragma once

#include <cstdint>

namespace stylized::asset
{
class AssetRegistry;

struct AssetId
{
    std::uint64_t value = 0;

    [[nodiscard]] bool isNull() const noexcept { return value == 0; }
    [[nodiscard]] explicit operator bool() const noexcept { return !isNull(); }
    [[nodiscard]] friend bool operator==(const AssetId&, const AssetId&) noexcept = default;
};

template<typename T>
class AssetHandle
{
public:
    AssetHandle() noexcept = default;

    [[nodiscard]] bool isNull() const noexcept { return id_.isNull(); }
    [[nodiscard]] explicit operator bool() const noexcept { return !isNull(); }
    [[nodiscard]] AssetId id() const noexcept { return id_; }
    [[nodiscard]] friend bool operator==(const AssetHandle&, const AssetHandle&) noexcept = default;

private:
    AssetId id_;
    friend class AssetRegistry;

    explicit AssetHandle(const AssetId id) noexcept
        : id_(id)
    {
    }
};

} // namespace stylized::asset
