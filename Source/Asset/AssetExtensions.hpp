#pragma once

#include "Asset.hpp"

#include <string>
#include <unordered_map>

// Recognized editable source formats. Every supported source uses an importer.
inline const std::unordered_map<std::string, AssetType> AssetExtensionMap =
{
	{ ".nscene", AssetType::Scene }
};

inline std::string_view GetAssetCacheExtension(AssetType type)
{
	switch (type)
	{
		case AssetType::Scene:  return ".nscene";
		default:                return {};
	}
}
