#pragma once

#include "Asset.hpp"

#include <string>
#include <unordered_map>

inline const std::unordered_map<std::string, AssetType> AssetExtensionMap =
{
	{ ".lua",       AssetType::Script },
};

inline std::string_view GetAssetCacheExtension(AssetType type)
{
	switch (type)
	{
		case AssetType::Script:         return ".lua";

		default:                       return {};
	}
}
