#include "TextureImporter.hpp"

#include "Asset/AssetSerializer.hpp"
#include "Asset/MetaFile.hpp"
#include "Core/Log.hpp"

#include <stb_image.h>

#include <algorithm>

bool TextureImportSettings::FromJSON(const nlohmann::json& document, TextureImportSettings& settings)
{
	if (!document.is_object())
		return false;

	TextureImportSettings result;

	if (document.contains("usage"))
	{
		if (!document["usage"].is_string())
			return false;

		const std::string usage = document["usage"].get<std::string>();

		if (usage == "color")
			result.Usage = TextureUsage::Color;
		else if (usage == "normal")
			result.Usage = TextureUsage::Normal;
		else if (usage == "data")
			result.Usage = TextureUsage::Data;
		else
			return false;
	}

	if (document.contains("generate_mips"))
	{
		if (!document["generate_mips"].is_boolean())
			return false;

		result.GenerateMips = document["generate_mips"].get<bool>();
	}

	settings = result;
	return true;
}

nlohmann::json TextureImportSettings::ToJSON() const
{
	const char* usage = "color";

	switch (Usage)
	{
		case TextureUsage::Color:
			usage = "color";
			break;

		case TextureUsage::Normal:
			usage = "normal";
			break;

		case TextureUsage::Data:
			usage = "data";
			break;
	}

	return
	{
			{ "usage", usage },
			{ "generate_mips", GenerateMips }
	};
}

bool TextureImporter::Import(const ImportContext& context)
{
	TextureImportSettings settings;

	if (context.TypeSettings && !TextureImportSettings::FromJSON(*context.TypeSettings, settings))
	{
		NV_ERROR("TextureImporter: invalid import settings for '{}'", context.Source.string());

		return false;
	}

	return Import(context, settings);
}

bool TextureImporter::Import(const ImportContext& context, const TextureImportSettings& settings)
{
	std::string extension = context.Source.extension().string();

	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character)
	{
		return static_cast<char>(std::tolower(character));
	});

	const bool isHDR = extension == ".hdr";

	TextureAssetData data;

	if (isHDR)
	{
		int width    = 0;
		int height   = 0;
		int channels = 0;

		float* pixels = stbi_loadf(context.Source.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

		if (!pixels || width <= 0 || height <= 0)
		{
			NV_ERROR("TextureImporter: stbi_loadf failed for '{}': {}", context.Source.string(), stbi_failure_reason());

			if (pixels)
				stbi_image_free(pixels);

			return false;
		}

		data.Width  = static_cast<uint32_t>(width);
		data.Height = static_cast<uint32_t>(height);
		data.Format = Format::RGBA32_Float;

		const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4 * sizeof(float);

		data.Pixels.resize(byteCount);

		std::memcpy(data.Pixels.data(), pixels, byteCount);

		stbi_image_free(pixels);
	}
	else
	{
		int width    = 0;
		int height   = 0;
		int channels = 0;

		stbi_uc* pixels = stbi_load(context.Source.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

		if (!pixels || width <= 0 || height <= 0)
		{
			NV_ERROR("TextureImporter: stbi_load failed for '{}': {}", context.Source.string(), stbi_failure_reason());

			if (pixels)
				stbi_image_free(pixels);

			return false;
		}

		data.Width  = static_cast<uint32_t>(width);
		data.Height = static_cast<uint32_t>(height);

		switch (settings.Usage)
		{
			case TextureUsage::Color:
				data.Format = Format::RGBA8_SRGB;
				break;

			case TextureUsage::Normal:
			case TextureUsage::Data:
				data.Format = Format::RGBA8_UNorm;
				break;
		}

		const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

		data.Pixels.resize(byteCount);

		std::memcpy(data.Pixels.data(), pixels, byteCount);

		stbi_image_free(pixels);
	}

	data.MipLevels = 1;

	if (settings.GenerateMips)
	{
		uint32_t width  = data.Width;
		uint32_t height = data.Height;

		while (width > 1 || height > 1)
		{
			width  = std::max(width / 2, 1u);
			height = std::max(height / 2, 1u);

			++data.MipLevels;
		}
	}

	return AssetSerializer::SerializeTexture(context.Destination, data);
}
