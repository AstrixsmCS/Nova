#include "AssetSerializer.hpp"
#include "Importers/SceneImporter.hpp"
#include "Importers/ShaderImporter.hpp"
#include "Importers/TextureImporter.hpp"

#include <fstream>
#include <utility>

bool AssetSerializer::DeserializeScene(const std::filesystem::path& path, SceneAssetData& outData)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		return false;

	auto document = nlohmann::json::parse(stream, nullptr, false);

	if (stream.bad() || !document.is_object())
		return false;

	outData.Document = std::move(document);
	return true;
}

bool AssetSerializer::DeserializeShader(const std::filesystem::path& path, ShaderAssetData& outData)
{
	if (!std::filesystem::exists(path))
		return false;

	outData.CachedPath = path;
	return true;
}

bool AssetSerializer::SerializeTexture(const std::filesystem::path& path, const TextureAssetData& data)
{
	if (data.Pixels.empty() || data.Width == 0 || data.Height == 0)
		return false;

	std::error_code ec;
	if (path.has_parent_path())
		std::filesystem::create_directories(path.parent_path(), ec);
	if (ec)
		return false;

	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream)
		return false;

	AssetHeader assetHeader
	{
		.Magic       = { 'N', 'O', 'V', 'A' },
		.Version     = 1,
		.Type        = AssetType::Texture,
		.PayloadSize = sizeof(TextureHeader) + data.Pixels.size()
	};

	TextureHeader texHeader
	{
		.Width          = data.Width,
		.Height         = data.Height,
		// .Format         = static_cast<uint32_t>(data.Format),
		.SizeBytes      = static_cast<uint32_t>(data.Pixels.size()),
		.GenerateMipmaps = data.MipLevels > 1 ? 1u : 0u,
		.MipLevels      = data.MipLevels
	};

	stream.write(reinterpret_cast<const char*>(&assetHeader), sizeof(AssetHeader));
	stream.write(reinterpret_cast<const char*>(&texHeader),   sizeof(TextureHeader));
	stream.write(reinterpret_cast<const char*>(data.Pixels.data()), data.Pixels.size());

	return stream.good();
}

bool AssetSerializer::DeserializeTexture(const std::filesystem::path& path, TextureAssetData& outData)
{
	/*std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream)
		return false;

	const std::streamsize fileSize = stream.tellg();
	stream.seekg(0, std::ios::beg);

	AssetHeader assetHeader{};
	stream.read(reinterpret_cast<char*>(&assetHeader), sizeof(AssetHeader));

	if (!stream || assetHeader.Magic[0] != 'N' || assetHeader.Magic[1] != 'O' || assetHeader.Magic[2] != 'V' || assetHeader.Magic[3] != 'A')
	{
		return false;
	}

	if (assetHeader.Version != 1 || assetHeader.Type != AssetType::Texture)
		return false;

	TextureHeader textureHeader{};
	stream.read(reinterpret_cast<char*>(&textureHeader), sizeof(TextureHeader));

	if (!stream || textureHeader.Width == 0 || textureHeader.Height == 0 || textureHeader.SizeBytes == 0 || textureHeader.MipLevels == 0)
	{
		return false;
	}

	const Format format = static_cast<Format>(textureHeader.Format);

	if (format != Format::RGBA8_SRGB && format != Format::RGBA8_UNorm && format != Format::RGBA32_Float)
	{
		return false;
	}

	const uint64_t expectedPixelBytes = static_cast<uint64_t>(textureHeader.Width) * textureHeader.Height * Utils::GetFormatBytesPerPixel(format);

	if (textureHeader.SizeBytes != expectedPixelBytes)
		return false;

	const uint64_t expectedPayloadSize = sizeof(TextureHeader) + textureHeader.SizeBytes;

	if (assetHeader.PayloadSize != expectedPayloadSize)
		return false;

	const uint64_t expectedFileSize = sizeof(AssetHeader) + expectedPayloadSize;

	if (static_cast<uint64_t>(fileSize) != expectedFileSize)
		return false;

	outData.Width     = textureHeader.Width;
	outData.Height    = textureHeader.Height;
	outData.Format    = format;
	outData.MipLevels = textureHeader.MipLevels;

	outData.Pixels.resize(textureHeader.SizeBytes);
	stream.read(reinterpret_cast<char*>(outData.Pixels.data()), textureHeader.SizeBytes);

	return static_cast<bool>(stream);*/

	return false;
}
