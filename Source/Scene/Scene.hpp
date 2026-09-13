#pragma once

#include "Entity.hpp"
#include "Asset/Asset.hpp"

using EntityMap = std::unordered_map<UUID, Entity>;

class Scene : public Asset
{
public:
	Scene(const std::string& name = "UntitledScene", bool initalize = true);
	~Scene();

	Entity CreateEntity(const std::string& name = "Entity");
	Entity CreateEntityWithUUID(UUID uuid, const std::string& name);
	void DestroyEntity(Entity entity);

	Entity GetEntityWithUUID(UUID uuid);
	Entity TryGetEntityWithUUID(UUID uuid);

	template<typename... Components>
	auto GetAllEntitiesWith()
	{
		return m_Registry.view<Components...>();
	}

	template<typename... Components>
	auto GetAllEntitiesWith() const
	{
		return m_Registry.view<Components...>();
	}

	entt::registry& GetRegistry()             { return m_Registry; }
	const entt::registry& GetRegistry() const { return m_Registry; }

	static AssetType GetStaticType() { return AssetType::Scene; }
	AssetType GetAssetType() const override { return GetStaticType(); }
private:
	UUID m_SceneID;
	entt::entity m_SceneEntity = entt::null;
	entt::registry m_Registry;

	std::string m_Name;

	EntityMap m_EntityIDMap;

	friend class Entity;
};
