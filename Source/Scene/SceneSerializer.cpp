#include "SceneSerializer.hpp"

#include "Scene.hpp"
#include "Core/Log.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <vector>

namespace
{
	using Json = nlohmann::json;

	struct EntityRecord
	{
		uint64_t ID = 0;
		std::string Name;
		TransformComponent Transform;
		uint64_t Parent = 0;
		std::vector<uint64_t> Children;
	};

	std::string EncodeID(uint64_t id)
	{
		return std::format("{:016x}", id);
	}

	bool DecodeID(const Json& value, uint64_t& id)
	{
		if (!value.is_string())
			return false;
		const auto& text = value.get_ref<const std::string&>();
		const auto result = std::from_chars(text.data(), text.data() + text.size(), id, 16);
		return text.size() == 16 && result.ec == std::errc{} &&
			   result.ptr == text.data() + text.size();
	}

	bool ReadVector(const Json& value, glm::vec3& vector)
	{
		if (!value.is_array() || value.size() != 3)
			return false;
		for (int i = 0; i < 3; ++i)
		{
			if (!value[i].is_number())
				return false;
			const double number = value[i].get<double>();
			if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max())
				return false;
			vector[i] = static_cast<float>(number);
		}
		return true;
	}

	Json WriteVector(const glm::vec3& value)
	{
		return Json::array({ value.x, value.y, value.z });
	}

	bool ReadRecords(const Json& entities, std::vector<EntityRecord>& records)
	{
		if (!entities.is_array())
			return false;

		std::unordered_map<uint64_t, size_t> indices;
		for (const auto& item : entities)
		{
			EntityRecord record;
			if (!item.is_object()                                                    ||
				!item.contains("Entity")           || !DecodeID(item["Entity"], record.ID) ||
				record.ID == 0                                                       ||
				!item.contains("TagComponent")     || !item["TagComponent"].is_object()    ||
				!item["TagComponent"].contains("Tag") || !item["TagComponent"]["Tag"].is_string() ||
				!item.contains("TransformComponent") || !item["TransformComponent"].is_object()  ||
				!item.contains("Parent")           || !DecodeID(item["Parent"], record.Parent)   ||
				!item.contains("Children")         || !item["Children"].is_array())
				return false;

			const auto& transform = item["TransformComponent"];
			if (!transform.contains("Position") || !ReadVector(transform["Position"], record.Transform.Translation) ||
				!transform.contains("Rotation") || !ReadVector(transform["Rotation"], record.Transform.Rotation)   ||
				!transform.contains("Scale")    || !ReadVector(transform["Scale"],    record.Transform.Scale))
				return false;

			record.Name = item["TagComponent"]["Tag"].get<std::string>();
			for (const auto& child : item["Children"])
			{
				uint64_t id = 0;
				if (!DecodeID(child, id) || id == 0)
					return false;
				record.Children.push_back(id);
			}
			if (!indices.emplace(record.ID, records.size()).second)
				return false;
			records.push_back(std::move(record));
		}

		// Verify both sides of each relationship, preserving child order.
		std::vector<unsigned int> childCounts(records.size(), 0);
		for (const auto& record : records)
		{
			if (record.Parent == record.ID || (record.Parent != 0 && !indices.contains(record.Parent)))
				return false;
			for (uint64_t child : record.Children)
			{
				const auto it = indices.find(child);
				if (it == indices.end() || records[it->second].Parent != record.ID ||
					++childCounts[it->second] != 1)
					return false;
			}
		}
		for (size_t i = 0; i < records.size(); ++i)
			if (childCounts[i] != (records[i].Parent != 0 ? 1u : 0u))
				return false;

		// Reject cycles without recursive traversal or a recursion-depth limit.
		std::vector<uint8_t> state(records.size(), 0);
		for (size_t i = 0; i < records.size(); ++i)
		{
			if (state[i] != 0)
				continue;
			uint64_t current = records[i].ID;
			while (current != 0 && state[indices.at(current)] == 0)
			{
				const size_t index = indices.at(current);
				state[index] = 1;
				current = records[index].Parent;
			}
			if (current != 0 && state[indices.at(current)] == 1)
				return false;
			current = records[i].ID;
			while (current != 0 && state[indices.at(current)] == 1)
			{
				const size_t index = indices.at(current);
				state[index] = 2;
				current = records[index].Parent;
			}
		}
		return true;
	}
}

SceneSerializer::SceneSerializer(Scene& scene)
	: m_Scene(scene)
{
}

bool SceneSerializer::SerializeEntity(nlohmann::json& out, Entity entity, Scene& scene)
{
	if (!entity.IsValid())
		return false;

	const auto  handle   = static_cast<entt::entity>(entity);
	const auto& registry = scene.GetRegistry();

	if (!registry.valid(handle) ||
		!registry.all_of<IDComponent, TagComponent, TransformComponent, RelationshipComponent>(handle))
		return false;

	const auto id = registry.get<IDComponent>(handle).ID;
	if (entity != scene.TryGetEntityWithUUID(id))
		return false;

	Json result;
	result["Entity"] = EncodeID(static_cast<uint64_t>(id));

	// ==== Tag ====
	const auto& tag = registry.get<TagComponent>(handle);
	result["TagComponent"] = { { "Tag", tag.Tag } };

	// ==== Relationship ====
	const auto& relationship = registry.get<RelationshipComponent>(handle);
	result["Parent"]   = EncodeID(static_cast<uint64_t>(relationship.ParentHandle));
	result["Children"] = Json::array();
	for (UUID child : relationship.Children)
		result["Children"].push_back(EncodeID(static_cast<uint64_t>(child)));

	// ==== Transform ====
	const auto& transform = registry.get<TransformComponent>(handle);
	result["TransformComponent"] =
	{
		{ "Position", WriteVector(transform.Translation) },
		{ "Rotation", WriteVector(transform.Rotation)    },
		{ "Scale",    WriteVector(transform.Scale)       }
	};

	out = std::move(result);
	return true;
}

bool SceneSerializer::SerializeToJSON(nlohmann::json& document) const
{
	Json result =
	{
		{ "Version",  1                                                         },
		{ "Scene",    m_Scene.m_Name                                            },
		{ "SceneID",  EncodeID(static_cast<uint64_t>(m_Scene.m_SceneID))        },
		{ "Entities", Json::array()                                             }
	};

	for (auto handle : m_Scene.GetRegistry().view<IDComponent>())
	{
		Json entity;
		if (!SerializeEntity(entity, Entity{ handle, &m_Scene }, m_Scene))
			return false;
		result["Entities"].push_back(std::move(entity));
	}

	auto& entities = result["Entities"];
	std::sort(entities.begin(), entities.end(), [](const Json& a, const Json& b)
	{
		return a["Entity"].get_ref<const std::string&>() <
			b["Entity"].get_ref<const std::string&>();
	});

	std::vector<EntityRecord> records;
	if (static_cast<uint64_t>(m_Scene.m_SceneID) == 0 || !ReadRecords(entities, records))
	{
		NV_ERROR("SceneSerializer: invalid UUID, transform or hierarchy");
		return false;
	}

	document = std::move(result);
	return true;
}

bool SceneSerializer::DeserializeEntities(const nlohmann::json& entities, Scene& scene)
{
	if (!scene.m_EntityIDMap.empty())
		return false;

	std::vector<EntityRecord> records;
	if (!ReadRecords(entities, records))
		return false;

	auto& registry = scene.GetRegistry();
	for (const auto& record : records)
	{
		Entity entity = scene.CreateEntityWithUUID(UUID{ record.ID }, record.Name);
		const auto handle = static_cast<entt::entity>(entity);
		registry.get<TagComponent>(handle).Tag       = record.Name;
		registry.get<TransformComponent>(handle)     = record.Transform;
	}
	for (const auto& record : records)
	{
		const auto handle = static_cast<entt::entity>(scene.GetEntityWithUUID(UUID{ record.ID }));
		auto& relationship = registry.get<RelationshipComponent>(handle);
		relationship.ParentHandle = UUID{ record.Parent };
		for (uint64_t child : record.Children)
			relationship.Children.emplace_back(child);
	}
	return true;
}

bool SceneSerializer::DeserializeFromJSON(const nlohmann::json& document)
{
	uint64_t sceneID = 0;
	if (!document.is_object()                                                      ||
		!document.contains("Version")  || document["Version"] != 1                ||
		!document.contains("Scene")    || !document["Scene"].is_string()           ||
		!document.contains("SceneID")  || !DecodeID(document["SceneID"], sceneID) ||
		sceneID == 0                                                               ||
		!document.contains("Entities"))
		return false;

	Scene temp(document["Scene"].get<std::string>());
	temp.m_SceneID     = UUID{ sceneID };
	temp.m_SceneEntity = entt::null;

	if (!DeserializeEntities(document["Entities"], temp))
	{
		NV_ERROR("SceneSerializer: invalid entity data");
		return false;
	}

	// Commit only after full validation.
	m_Scene.m_Name        = std::move(temp.m_Name);
	m_Scene.m_SceneID     = temp.m_SceneID;
	m_Scene.m_SceneEntity = entt::null;
	m_Scene.m_Registry    = std::move(temp.m_Registry);
	m_Scene.m_EntityIDMap.clear();

	for (auto handle : m_Scene.m_Registry.view<IDComponent>())
	{
		const UUID id = m_Scene.m_Registry.get<IDComponent>(handle).ID;
		m_Scene.m_EntityIDMap.emplace(id, Entity{ handle, &m_Scene });
	}

	return true;
}

bool SceneSerializer::Serialize(const std::filesystem::path& filepath) const
{
	Json document;
	if (!SerializeToJSON(document))
		return false;

	std::error_code error;
	if (filepath.has_parent_path())
		std::filesystem::create_directories(filepath.parent_path(), error);
	if (error)
		return false;

	std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);
	if (!stream)
		return false;

	stream << document.dump(4, ' ', false, Json::error_handler_t::replace) << '\n';
	stream.close();
	return static_cast<bool>(stream);
}

bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
{
	std::ifstream stream(filepath, std::ios::binary);
	if (!stream)
		return false;
	const auto document = Json::parse(stream, nullptr, false);
	return !stream.bad() && DeserializeFromJSON(document);
}
