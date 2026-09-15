#pragma once

#include "Importers/ImporterContext.hpp"

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

	virtual bool Import(const ImportContext& context) = 0;
};
