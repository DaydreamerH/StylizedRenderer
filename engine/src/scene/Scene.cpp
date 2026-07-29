#include <stylized/scene/Scene.hpp>

namespace stylized::scene
{

EntityId Scene::createEntity()
{
    if (!freeIndices_.empty())
    {
        const std::uint32_t index = freeIndices_.back();

        freeIndices_.pop_back();

        EntitySlot& slot = slots_[index];

        slot.transform = Transform{};
        slot.alive = true;

        return EntityId{
            .index = index,
            .generation = slot.generation
        };
    }

    if (slots_.size() >= EntityId::invalidIndex) return{};

    const std::uint32_t index = static_cast<std::uint32_t>(slots_.size());
    EntitySlot& slot = slots_.emplace_back();
    slot.alive = true;

    return EntityId{
        .index = index,
        .generation = slot.generation
    };
}

bool Scene::destroyEntity(const EntityId entity)
{
    if (!isAlive(entity)) return false;

    EntitySlot& slot = slots_[entity.index];

    slot.transform = Transform{};
    slot.alive = false;
    ++slot.generation;

    freeIndices_.push_back(entity.index);

    return true;
}

bool Scene::isAlive(const EntityId entity) const noexcept
{
    if (entity.isNull()) return false;
    if (entity.index >= slots_.size()) return false;

    const EntitySlot& slot = slots_[entity.index];

    return slot.alive && slot.generation == entity.generation;
}

Transform* Scene::tryTransform(
    const EntityId entity) noexcept
{
    if (!isAlive(entity))
    {
        return nullptr;
    }

    return &slots_[entity.index].transform;
}

const Transform* Scene::tryTransform(
    const EntityId entity) const noexcept
{
    if (!isAlive(entity))
    {
        return nullptr;
    }

    return &slots_[entity.index].transform;
}

} // namespace stylized::scene
