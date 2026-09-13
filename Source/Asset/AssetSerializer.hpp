#pragma once

#include "Asset.hpp"

#include <cstdint>
#include <filesystem>

struct SceneAssetData;

struct AssetHeader
{
	char Magic[4] = { 'N', 'O', 'V', 'A' };
	uint32_t Version = 1;
	AssetType Type = AssetType::None;
	uint64_t PayloadSize = 0;
};

class AssetSerializer
{
public:
	// Serialize/Deserialize pairs are added here as importers are implemented.
	static bool DeserializeScene(const std::filesystem::path& path, SceneAssetData& data);
};
