#pragma once

#include <filesystem>
#include <nlohmann/json_fwd.hpp>

class Scene;
class Entity;

class SceneSerializer
{
public:
	explicit SceneSerializer(Scene& scene);

	bool Serialize(const std::filesystem::path& filepath) const;
	bool SerializeToJSON(nlohmann::json& document) const;

	bool Deserialize(const std::filesystem::path& filepath);
	bool DeserializeFromJSON(const nlohmann::json& document);

	static bool SerializeEntity(nlohmann::json& out, Entity entity, Scene& scene);
	static bool DeserializeEntities(const nlohmann::json& entities, Scene& scene);

	inline static constexpr char FileFilter[]        = "Nova Scene (*.nscene)\0*.nscene\0";
	inline static constexpr char DefaultExtension[]  = ".nscene";

private:
	Scene& m_Scene;
};
