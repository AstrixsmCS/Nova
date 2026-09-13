#pragma once

#include <cstdint>
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

	virtual uint32_t GetVersion() const { return 1; }

	virtual bool Import(const std::filesystem::path& source, const std::filesystem::path& destination) = 0;
};
