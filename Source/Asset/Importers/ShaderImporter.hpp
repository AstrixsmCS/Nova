#pragma once

#include "Asset/AssetImporter.hpp"

struct ShaderAssetData : public AssetData
{
	std::filesystem::path CachedPath;
};

class ShaderImporter final : public AssetImporter
{
public:
	uint32_t GetVersion() const override { return 1; }

	bool Import(const ImportContext& context) override;
};
