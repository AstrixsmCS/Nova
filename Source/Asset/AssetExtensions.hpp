#pragma once

#include "Asset.hpp"

#include <string>
#include <unordered_map>

// Recognized editable source formats. Every supported source uses an importer.
inline const std::unordered_map<std::string, AssetType> AssetExtensionMap =
{
	{ ".nscene", AssetType::Scene   },
	{ ".slang",  AssetType::Shader  },
	{ ".png",    AssetType::Texture },
	{ ".jpg",    AssetType::Texture },
	{ ".jpeg",   AssetType::Texture },
	{ ".hdr",    AssetType::Texture },
	{ ".tga",    AssetType::Texture },
	{ ".gltf",   AssetType::Mesh    },
};

inline std::string_view GetAssetCacheExtension(AssetType type)
{
	switch (type)
	{
		case AssetType::Scene:  return ".nscene";
		case AssetType::Shader: return ".slang";
		case AssetType::Texture: return ".ntex";
		case AssetType::Mesh: return ".nmesh";
		default:                return {};
	}
}
