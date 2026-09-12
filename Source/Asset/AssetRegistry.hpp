#pragma once

#include "Asset.hpp"

#include <filesystem>
#include <unordered_map>

struct AssetMetadata
{
	std::filesystem::path Path;    // Relative to asset root, e.g. "Meshes/Sword.gltf"
	AssetType             Type     = AssetType::None;
	bool                  IsLoaded = false;

	bool IsValid() const { return Type != AssetType::None && !Path.empty(); }
};

// Dumb UUID-to-metadata map. Not thread-safe; AssetManager owns the instance
// and is responsible for synchronization and persistence.
class AssetRegistry
{
public:
	const AssetMetadata& Get(AssetHandle handle) const;
	void                 Set(AssetHandle handle, const AssetMetadata& metadata);

	bool   Contains(AssetHandle handle) const;
	size_t Remove(AssetHandle handle);
	size_t Count() const { return m_Assets.size(); }
	void   Clear();

	auto begin()       { return m_Assets.begin(); }
	auto end()         { return m_Assets.end(); }
	auto begin() const { return m_Assets.cbegin(); }
	auto end()   const { return m_Assets.cend(); }

private:
	std::unordered_map<AssetHandle, AssetMetadata> m_Assets;

	inline static AssetMetadata s_NullMetadata{};
};
