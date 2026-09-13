#pragma once

#include "Asset/AssetImporter.hpp"

#include <nlohmann/json.hpp>

struct SceneAssetData : public AssetData
{
	nlohmann::json Document;
};

class SceneImporter final : public AssetImporter
{
public:
	uint32_t GetVersion() const override { return 1; }

	bool Import(const std::filesystem::path& source, const std::filesystem::path& destination) override;
};
