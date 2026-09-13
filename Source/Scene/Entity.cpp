#include "Scene.hpp"

UUID Entity::GetSceneUUID() const
{
	return m_Scene->m_SceneID;
}

Entity Entity::GetParent() const
{
	return m_Scene->TryGetEntityWithUUID(GetParentUUID());
}

bool Entity::IsAncestorOf(Entity entity) const
{
	const auto& children = Children();

	if (children.empty())
		return false;

	for (UUID child : children)
	{
		if (child == entity.GetUUID())
			return true;
	}

	for (UUID child : children)
	{
		if (m_Scene->GetEntityWithUUID(child).IsAncestorOf(entity))
			return true;
	}

	return false;
}

template<typename T, typename... Args>
T& Entity::AddComponent(Args&&... args)
{
	return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
}

template<typename T>
T& Entity::GetComponent()
{
	return m_Scene->m_Registry.get<T>(m_EntityHandle);
}

template<typename T>
const T& Entity::GetComponent() const
{
	return m_Scene->m_Registry.get<T>(m_EntityHandle);
}

template<typename T>
T* Entity::TryGetComponent()
{
	return m_Scene->m_Registry.try_get<T>(m_EntityHandle);
}

template<typename T>
const T* Entity::TryGetComponent() const
{
	return m_Scene->m_Registry.try_get<T>(m_EntityHandle);
}

template<typename... T>
bool Entity::HasComponent()
{
	return m_Scene->m_Registry.all_of<T...>(m_EntityHandle);
}

template<typename... T>
bool Entity::HasComponent() const
{
	return m_Scene->m_Registry.all_of<T...>(m_EntityHandle);
}

template<typename...T>
bool Entity::HasAny()
{
	return m_Scene->m_Registry.any_of<T...>(m_EntityHandle);
}

template<typename...T>
bool Entity::HasAny() const
{
	return m_Scene->m_Registry.any_of<T...>(m_EntityHandle);
}

template<typename T>
void Entity::RemoveComponent()
{
	m_Scene->m_Registry.remove<T>(m_EntityHandle);
}

template<typename T>
void Entity::RemoveComponentIfExists()
{
	m_Scene->m_Registry.remove<T>(m_EntityHandle);
}

bool Entity::IsValid() const { return (m_EntityHandle != entt::null) && m_Scene && m_Scene->m_Registry.valid(m_EntityHandle); }
Entity::operator bool() const { return IsValid(); }
