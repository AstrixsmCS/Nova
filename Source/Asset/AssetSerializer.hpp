#pragma once

#include "Asset.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

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
	// e.g.:
	//   static bool SerializeMesh  (const std::filesystem::path& path, const MeshAssetData& data);
	//   static bool DeserializeMesh(const std::filesystem::path& path, MeshAssetData& outData);
};
