#pragma once

#include "AssetImporter.hpp"
#include "AssetRegistry.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

// Synchronous, main-thread-only. All file I/O, path resolution, and persistence
// live here. AssetRegistry is a dumb map owned by this class.
class AssetManager
{
public:
	static void Initialize(const std::filesystem::path& root = "Assets");
	static void Update();
	static void Shutdown();

	static AssetHandle             RegisterAsset(const std::filesystem::path& path);
	static std::shared_ptr<Asset>  GetAsset(const std::filesystem::path& path);
	static std::shared_ptr<Asset>  GetAsset(AssetHandle handle);

	template<typename TAsset>
	static std::shared_ptr<TAsset> GetAsset(AssetHandle handle)
	{
		static_assert(std::is_base_of_v<Asset, TAsset>, "TAsset must derive from Asset");
		return std::dynamic_pointer_cast<TAsset>(GetAsset(handle));
	}

	// Conversion only. Directly loaded assets (such as Lua) return false.
	static bool Import(AssetHandle handle);
	static bool Reimport(AssetHandle handle);

	static bool IsAssetLoaded(AssetHandle handle);
	static bool IsAssetMissing(AssetHandle handle);
	static void UnloadAsset(AssetHandle handle);

	static const AssetRegistry& GetRegistry() { return s_Registry; }
private:
	static std::unique_ptr<AssetData> DeserializeAsset(AssetType type, const std::filesystem::path& path);
	static std::shared_ptr<Asset> FinalizeAsset(AssetType type, AssetData& data);

	// Registry persistence
	static bool LoadRegistry();
	static bool SaveRegistry();

	// Path helpers
	static std::filesystem::path ResolvePath(const std::filesystem::path& relativePath);
	static std::filesystem::path GetMetadataPath(AssetHandle handle);
	static std::filesystem::path GetCachePath(AssetHandle handle);
	static std::filesystem::path BucketPath(AssetHandle handle, const char* directory, const std::string& extension);
	static bool IsSourcePath(const std::filesystem::path& path);
	static bool RequiresImport(const std::filesystem::path& path);
	static std::string Extension(const std::filesystem::path& path);

	static bool EnsureImported(AssetHandle handle, bool force);
	static AssetHandle FindByPath(const std::filesystem::path& relativePath);

	inline static std::filesystem::path                              s_Root;
	inline static AssetRegistry                                      s_Registry;
	inline static std::unordered_map<std::string, AssetHandle> s_PathToHandle;
	inline static std::unordered_map<AssetType, std::unique_ptr<AssetImporter>> s_Importers;
	inline static std::unordered_map<AssetHandle, std::shared_ptr<Asset>> s_LoadedAssets;
	inline static std::unordered_set<AssetHandle>                    s_LoadingAssets;
	inline static std::unordered_set<AssetHandle>                    s_ImportingAssets;
	inline static bool                                               s_Initialized = false;
};
