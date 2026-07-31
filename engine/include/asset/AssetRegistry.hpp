#pragma once

#include <asset/AssetHandle.hpp>
#include <core/NonCopyable.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace stylized::asset
{

class AssetRegistry final : public core::NonCopyable
{
public:
    AssetRegistry() = default;
    ~AssetRegistry() = default;

    template<typename T, typename... Args>
    [[nodiscard]] AssetHandle<T> emplace(Args&&... args);

    template<typename T>
    [[nodiscard]] bool contains(AssetHandle<T> handle) const noexcept;

    template<typename T>
    [[nodiscard]] T* get(AssetHandle<T> handle) noexcept;

    template<typename T>
    [[nodiscard]] const T* get(AssetHandle<T> handle) const noexcept;

    template<typename T>
    bool remove(AssetHandle<T> handle);

    void clear() noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    struct AssetEntryBase
    {
        virtual ~AssetEntryBase() = default;

        [[nodiscard]] virtual std::type_index type() const noexcept = 0;
    };

    template<typename T>
    struct AssetEntry final : AssetEntryBase
    {
        T asset;

        template<typename... Args>
        explicit AssetEntry(Args&&... args)
            : asset(std::forward<Args>(args)...)
        {
        }

        [[nodiscard]] std::type_index type() const noexcept override
        {
            return typeid(T);
        }
    };
    
    [[nodiscard]] AssetId nextAssetId() const noexcept;

    void advanceAssetId() noexcept;

    std::unordered_map<std::uint64_t, std::unique_ptr<AssetEntryBase>> assets_;

    std::uint64_t nextAssetId_ = 1;
};

template<typename T, typename... Args>
AssetHandle<T> AssetRegistry::emplace(Args&&... args)
{
    const AssetId id = nextAssetId();

    if (id.isNull()) return {};

    auto entry = std::make_unique<AssetEntry<T>>(std::forward<Args>(args)...);

    const bool inserted = assets_.emplace(id.value, std::move(entry)).second;

    if (!inserted) return {};

    advanceAssetId();

    return AssetHandle<T>{id};
}

template<typename T>
bool AssetRegistry::contains(const AssetHandle<T> handle) const noexcept
{
    return get(handle) != nullptr;
}

template<typename T>
T* AssetRegistry::get(const AssetHandle<T> handle) noexcept
{
    if (handle.isNull()) return nullptr;

    const auto iterator = assets_.find(handle.id().value);

    if (iterator == assets_.end()) return nullptr;

    AssetEntryBase* entry = iterator->second.get();

    if (entry->type() != std::type_index{typeid(T)}) return nullptr;

    return &static_cast<AssetEntry<T>*>(entry)->asset;
}

template<typename T>
const T* AssetRegistry::get(const AssetHandle<T> handle) const noexcept
{
    if (handle.isNull()) return nullptr;

    const auto iterator = assets_.find(handle.id().value);

    if (iterator == assets_.end()) return nullptr;

    const AssetEntryBase* entry = iterator->second.get();

    if (entry->type() != std::type_index{typeid(T)}) return nullptr;

    return &static_cast<const AssetEntry<T>*>(entry)->asset;
}

template<typename T>
bool AssetRegistry::remove(const AssetHandle<T> handle)
{
    if (!contains(handle)) return false;

    return assets_.erase(handle.id().value) == 1;
}

inline void AssetRegistry::clear() noexcept
{
    assets_.clear();
}

inline std::size_t AssetRegistry::size() const noexcept
{
    return assets_.size();
}

inline bool AssetRegistry::empty() const noexcept
{
    return assets_.empty();
}

inline AssetId AssetRegistry::nextAssetId() const noexcept
{
    return AssetId{nextAssetId_};
}

inline void AssetRegistry::advanceAssetId() noexcept
{
    if (nextAssetId_ == std::numeric_limits<std::uint64_t>::max())
    {
        nextAssetId_ = 0;
        return;
    }

    ++nextAssetId_;
}

}
