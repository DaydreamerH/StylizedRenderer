#pragma once

#include <core/NonCopyable.hpp>
#include <scene/Entity.hpp>
#include <scene/Transform.hpp>

#include <cstdint>
#include <span>
#include <vector>
#include <glm/mat4x4.hpp>

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

    [[nodiscard]] EntityId parent(EntityId entity) const noexcept;
    [[nodiscard]] std::span<const EntityId> children(EntityId entity) const noexcept;
    bool setParent(EntityId child, EntityId newParent);

    [[nodiscard]] const glm::mat4* tryWorldMatrix(const EntityId entity) noexcept;

private:
    struct EntitySlot
    {
        Transform transform;

        EntityId parent;
        std::vector<EntityId> children;

        glm::mat4 worldMatrix{1.F};
        bool worldTransformDirty = true;

        std::uint32_t generation = 0;
        bool alive = false;
    };

    std::vector<EntitySlot> slots_;
    std::vector<std::uint32_t> freeIndices_;

    [[nodiscard]] bool wouldCreateCycle(EntityId child, EntityId newParent) const noexcept;

    void removeChild(EntityId parent, EntityId child);

    void markWorldTransformDirty(EntityId entity) noexcept;
};

} // namespace stylized::scene
