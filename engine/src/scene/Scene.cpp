#include <stylized/scene/Scene.hpp>

#include <algorithm>

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

    if (isAlive(slot.parent)) removeChild(slot.parent, entity);

    for (const EntityId child : slot.children)
    {
        if (!isAlive(child)) continue;

        slots_[child.index].parent = {};
        markWorldTransformDirty(child);
    }

    slot.transform = Transform{};
    slot.parent = {};
    slot.children.clear();
    slot.worldMatrix = glm::mat4{1.0F};
    slot.worldTransformDirty = true;
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

    markWorldTransformDirty(entity);

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

EntityId Scene::parent(const EntityId entity) const noexcept
{
    if (!isAlive(entity)) return{};

    const EntityId result = slots_[entity.index].parent;

    if (!isAlive(result)) return{};

    return result;
}

std::span<const EntityId> Scene::children(const EntityId entity) const noexcept
{
    if (!isAlive(entity)) return {};

    return slots_[entity.index].children;
}

bool Scene::setParent(const EntityId child, const EntityId newParent)
{
    if (!isAlive(child)) return false;
    if (!newParent.isNull() && !isAlive(newParent)) return false;

    if (child == newParent) return false;

    EntitySlot& childSlot = slots_[child.index];

    if (childSlot.parent == newParent) return true;

    if (wouldCreateCycle(child, newParent)) return false;

    if (!newParent.isNull()) slots_[newParent.index].children.push_back(child);

    const EntityId oldParent = childSlot.parent;

    if (isAlive(oldParent)) removeChild(oldParent, child);

    childSlot.parent = newParent;

    markWorldTransformDirty(child);

    return true;
}

const glm::mat4* Scene::tryWorldMatrix(const EntityId entity) noexcept
{
    if (!isAlive(entity)) return nullptr;

    EntitySlot& slot = slots_[entity.index];

    if (!slot.worldTransformDirty) return &slot.worldMatrix;

    const glm::mat4 localMatrix = slot.transform.localMatrix();

    if (isAlive(slot.parent))
    {
        const glm::mat4* parentWorldMatrix = tryWorldMatrix(slot.parent);

        if (parentWorldMatrix == nullptr) return nullptr;

        slot.worldMatrix = *parentWorldMatrix * localMatrix;
    }
    else slot.worldMatrix = localMatrix;

    slot.worldTransformDirty = false;

    return &slot.worldMatrix;
}

bool Scene::wouldCreateCycle(const EntityId child, const EntityId newParent) const noexcept
{
    EntityId current = newParent;

    while (isAlive(current))
    {
        if (current == child)
        {
            return true;
        }

        current = slots_[current.index].parent;
    }

    return false;
}

void Scene::removeChild(const EntityId parent, const EntityId child)
{
    if (!isAlive(parent)) return;

    std::vector<EntityId>& children = slots_[parent.index].children;

    const auto newEnd = std::remove(children.begin(), children.end(), child);

    children.erase(newEnd, children.end());
}

void Scene::markWorldTransformDirty(const EntityId entity) noexcept
{
    if (!isAlive(entity)) return;

    EntitySlot& slot = slots_[entity.index];

    slot.worldTransformDirty = true;

    for (const EntityId child : slot.children)
    {
        markWorldTransformDirty(child);
    }
}

} // namespace stylized::scene
