#include "Scene.hpp"

#include "Components.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

Scene::Scene(const std::string& name, bool /*initialize*/)
	: m_Name(name)
{
	// No optional scene initialization is needed yet.
}

Scene::~Scene() = default;

Entity Scene::CreateEntity(const std::string& name)
{
	UUID uuid;

	while (static_cast<uint64_t>(uuid) == 0 || m_EntityIDMap.contains(uuid))
	{
		uuid = UUID{};
	}

	return CreateEntityWithUUID(uuid, name);
}

Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
{
	if (static_cast<uint64_t>(uuid) == 0 || m_EntityIDMap.contains(uuid))
	{
		assert(false && "Entity UUID must be nonzero and unique");
		return {};
	}

	const auto handle = m_Registry.create();

	m_Registry.emplace<IDComponent>(handle, IDComponent{ uuid });
	m_Registry.emplace<TagComponent>(handle, name.empty() ? "Entity" : name);
	m_Registry.emplace<TransformComponent>(handle);
	m_Registry.emplace<RelationshipComponent>(handle);

	Entity entity{ handle, this };
	m_EntityIDMap.emplace(uuid, entity);

	return entity;
}

void Scene::DestroyEntity(Entity entity)
{
	if (entity.m_Scene != this || !m_Registry.valid(entity.m_EntityHandle))
	{
		return;
	}

	const auto handle = entity.m_EntityHandle;
	const UUID uuid = m_Registry.get<IDComponent>(handle).ID;

	auto& relationship = m_Registry.get<RelationshipComponent>(handle);

	// Remove this entity from its parent's child list.
	const Entity parent = TryGetEntityWithUUID(relationship.ParentHandle);

	if (parent)
	{
		auto& parentRelationship = m_Registry.get<RelationshipComponent>(parent.m_EntityHandle);

		std::erase(parentRelationship.Children, uuid);
	}

	// Keep children alive and detach them from this parent.
	for (UUID childUUID : relationship.Children)
	{
		const Entity child = TryGetEntityWithUUID(childUUID);

		if (!child)
			continue;

		auto& childRelationship = m_Registry.get<RelationshipComponent>(child.m_EntityHandle);

		if (childRelationship.ParentHandle == uuid)
			childRelationship.ParentHandle = UUID{ 0 };
	}

	m_EntityIDMap.erase(uuid);
	m_Registry.destroy(handle);
}

Entity Scene::GetEntityWithUUID(UUID uuid)
{
	Entity entity = TryGetEntityWithUUID(uuid);

	assert(entity.IsValid() && "Entity UUID was not found");

	return entity;
}

Entity Scene::TryGetEntityWithUUID(UUID uuid)
{
	const auto iterator = m_EntityIDMap.find(uuid);

	if (iterator == m_EntityIDMap.end())
		return {};

	const Entity entity = iterator->second;

	if (!m_Registry.valid(entity.m_EntityHandle))
		return {};

	return entity;
}
