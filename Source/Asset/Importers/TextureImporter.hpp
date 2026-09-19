#pragma once

#include "Asset/AssetImporter.hpp"

#include <nlohmann/json.hpp>
#include <cstdint>
#include <vector>

enum class TextureUsage : uint8_t
{
	Color = 0,
	Normal,
	Data
};

struct TextureImportSettings
{
	TextureUsage Usage        = TextureUsage::Color;
	bool         GenerateMips = true;

	static bool FromJSON(const nlohmann::json& document, TextureImportSettings& settings);
	nlohmann::json ToJSON() const;
};

struct TextureAssetData : public AssetData
{
	std::vector<uint8_t> Pixels;

	uint32_t Width     = 0;
	uint32_t Height    = 0;
	uint32_t MipLevels = 1;

	// Format Format = Format::Invalid;
};


class TextureImporter final : public AssetImporter
{
public:
	uint32_t GetVersion() const override { return 1; }

	bool Import(const ImportContext& context) override;

private:
	bool Import(const ImportContext& context, const TextureImportSettings& settings);
};
