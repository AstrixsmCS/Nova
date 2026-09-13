#pragma once

#include "Asset.hpp"

#include <string>
#include <unordered_map>

inline const std::unordered_map<std::string, AssetType> AssetExtensionMap =
{
	{ ".nscene",       AssetType::Scene }
};

inline std::string_view GetAssetCacheExtension(AssetType type)
{
	switch (type)
	{
		default:                       return {};
	}
}
