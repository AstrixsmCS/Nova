#include "AssetSerializer.hpp"
#include "Importers/SceneImporter.hpp"

#include <fstream>
#include <utility>

bool AssetSerializer::DeserializeScene(const std::filesystem::path& path, SceneAssetData& data)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		return false;

	auto document = nlohmann::json::parse(stream, nullptr, false);

	if (stream.bad() || !document.is_object())
		return false;

	data.Document = std::move(document);
	return true;
}
