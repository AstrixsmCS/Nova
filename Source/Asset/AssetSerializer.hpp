#pragma once

#include "Asset.hpp"

#include <cstdint>
#include <filesystem>

struct SceneAssetData;
struct ShaderAssetData;
struct TextureAssetData;

struct AssetHeader
{
	char Magic[4] = { 'N', 'O', 'V', 'A' };
	uint32_t Version = 1;
	AssetType Type = AssetType::None;
	uint64_t PayloadSize = 0;
};

struct TextureHeader
{
	uint32_t Width;
	uint32_t Height;
	uint32_t Format;
	uint32_t SizeBytes;
	uint32_t GenerateMipmaps;
	uint32_t WrapMode;
	uint32_t MinFilter;
	uint32_t MagFilter;
	uint32_t MipLevels;
};

class AssetSerializer
{
public:
	// Serialize/Deserialize pairs are added here as importers are implemented.
	static bool DeserializeScene(const std::filesystem::path& path, SceneAssetData& outData);
	static bool DeserializeShader(const std::filesystem::path& path, ShaderAssetData& outData);

	static bool SerializeTexture(const std::filesystem::path& path, const TextureAssetData& data);
	static bool DeserializeTexture(const std::filesystem::path& path, TextureAssetData& outData);
};
