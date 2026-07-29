#pragma once

#include <stylized/core/NonCopyable.hpp>
#include <stylized/scene/Entity.hpp>
#include <stylized/scene/Transform.hpp>

#include <cstdint>
#include <vector>

namespace stylized::scene
{

class Scene final : public core::NonCopyable
{
public:
    Scene() = default;
    ~Scene() = default;

    [[nodiscard]] EntityId createEntity();

    bool destroyEntity(EntityId entity);

    [[nodiscard]] bool isAlive(EntityId entity) const noexcept;
    [[nodiscard]] Transform* tryTransform(EntityId entity) noexcept;
    [[nodiscard]] const Transform* tryTransform(EntityId entity) const noexcept;

private:
    struct EntitySlot
    {
        Transform transform;
        std::uint32_t generation = 0;
        bool alive = false;
    };

    std::vector<EntitySlot> slots_;
    std::vector<std::uint32_t> freeIndices_;
};

} // namespace stylized::scene
