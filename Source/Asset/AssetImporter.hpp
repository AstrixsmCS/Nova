#pragma once

#include "Asset.hpp"

#include <filesystem>

// Base type for the in-memory asset payload an importer produces.
struct AssetData
{
	virtual ~AssetData() = default;
};

class AssetImporter
{
public:
	virtual ~AssetImporter() = default;

	virtual bool Import(const std::filesystem::path& source, const std::filesystem::path& destination) = 0;
};
