#include "AssetManager.hpp"

#include "AssetExtensions.hpp"
#include "AssetSerializer.hpp"
#include "MetaFile.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneSerializer.hpp"

#include "Importers/SceneImporter.hpp"
#include "Importers/ShaderImporter.hpp"
#include "Importers/TextureImporter.hpp"

#include "Core/Log.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <format>
#include <fstream>
#include <map>

#if defined(_WIN32)
	#include <Windows.h>
#endif

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{
	std::string PathKey(const std::filesystem::path& path)
	{
		const auto utf8 = path.generic_u8string();
		return std::string(utf8.begin(), utf8.end());
	}

	bool Fingerprint(const std::filesystem::path& path, std::string& result)
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
			return false;

		uint64_t hash = 14695981039346656037ULL;
		char buffer[16384];
		while (stream.read(buffer, sizeof(buffer)) || stream.gcount() != 0)
		{
			for (std::streamsize i = 0; i < stream.gcount(); ++i)
			{
				hash ^= static_cast<unsigned char>(buffer[i]);
				hash *= 1099511628211ULL;
			}
		}

		if (stream.bad() || !stream.eof())
			return false;

		result = std::format("{:016x}", hash);
		return true;
	}

	bool WriteJSON(const std::filesystem::path& path, const nlohmann::json& data)
	{
		std::error_code ec;
		if (path.has_parent_path())
			std::filesystem::create_directories(path.parent_path(), ec);
		if (ec)
			return false;

		const auto tmp = path.parent_path() / (path.filename().string() + ".tmp");
		{
			std::ofstream stream(tmp, std::ios::binary | std::ios::trunc);
			stream << data.dump(4, ' ', false, nlohmann::json::error_handler_t::replace) << '\n';
			stream.close();
			if (!stream)
			{
				std::filesystem::remove(tmp, ec);
				return false;
			}
		}

#if defined(_WIN32)
		if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			std::filesystem::remove(tmp, ec);
			return false;
		}
		return true;
#else
		std::filesystem::rename(tmp, path, ec);
		if (ec) { std::filesystem::remove(tmp, ec); return false; }
		return true;
#endif
	}

	bool ReadJSON(const std::filesystem::path& path, nlohmann::json& out)
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
			return false;
		auto parsed = nlohmann::json::parse(stream, nullptr, false);
		if (parsed.is_discarded())
			return false;
		out = std::move(parsed);
		return true;
	}
}

// ---------------------------------------------------------------------------

void AssetManager::Initialize(const std::filesystem::path& root)
{
	if (s_Initialized)
		return;

	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	if (ec)
	{
		NV_ERROR("AssetManager: failed to create asset root '{}'", root.string());
		return;
	}

	s_Root = std::filesystem::canonical(root, ec);
	if (ec)
	{
		NV_ERROR("AssetManager: failed to canonicalize root '{}'", root.string());
		return;
	}

	s_Importers[AssetType::Scene]   = std::make_unique<SceneImporter>();
	s_Importers[AssetType::Shader]  = std::make_unique<ShaderImporter>();
	s_Importers[AssetType::Texture] = std::make_unique<TextureImporter>();

	if (!LoadRegistry())
	{
		NV_ERROR("AssetManager: registry initialization failed.");
		s_Registry.Clear();
		s_PathToHandle.clear();
		s_Importers.clear();
		s_Root.clear();
		return;
	}

	s_Initialized = true;
	NV_TRACE("AssetManager initialized (root: {}, {} assets)", s_Root.string(), s_Registry.Count());
}

void AssetManager::Update()
{
	// No background work in this synchronous version.
}

void AssetManager::Shutdown()
{
	s_LoadedAssets.clear();
	s_LoadingAssets.clear();
	s_ImportingAssets.clear();
	s_Importers.clear();
	s_Registry.Clear();
	s_Root.clear();
	s_PathToHandle.clear();
	s_Initialized = false;
	NV_TRACE("AssetManager shut down.");
}

// ---------------------------------------------------------------------------
// Registry persistence
// ---------------------------------------------------------------------------

bool AssetManager::LoadRegistry()
{
	const auto registryPath = ResolvePath("AssetRegistry.nvr");
	if (registryPath.empty())
		return false;

	std::error_code error;
	const bool exists = std::filesystem::exists(registryPath, error);

	if (error)
		return false;

	if (!exists)
		return SaveRegistry();

	nlohmann::json document;

	if (!ReadJSON(registryPath, document) ||
		!document.is_object() ||
		!document.contains("version") ||
		document["version"] != 1 ||
		!document.contains("assets") ||
		!document["assets"].is_array())
	{
		NV_ERROR("AssetManager: AssetRegistry.nvr is malformed.");
		return false;
	}

	AssetRegistry registry;
	std::unordered_map<std::string, AssetHandle> pathToHandle;

	for (const auto& item : document["assets"])
	{
		if (!item.is_object() ||
			!item.contains("id")   || !item["id"].is_string() ||
			!item.contains("path") || !item["path"].is_string() ||
			!item.contains("type") || !item["type"].is_number_unsigned())
		{
			NV_ERROR("AssetManager: invalid registry entry.");
			return false;
		}

		const auto id = item["id"].get<std::string>();

		uint64_t value = 0;
		const auto result = std::from_chars(id.data(), id.data() + id.size(), value, 16);

		if (id.size() != 16 || result.ec != std::errc{} || result.ptr != id.data() + id.size() || value == 0)
		{
			NV_ERROR("AssetManager: invalid asset UUID '{}'.", id);
			return false;
		}

		const auto text = item["path"].get<std::string>();
		const auto path = std::filesystem::path(std::u8string(text.begin(), text.end())).lexically_normal();

		if (!IsSourcePath(path))
		{
			NV_ERROR("AssetManager: invalid asset path '{}'.", text);
			return false;
		}

		const auto extension = AssetExtensionMap.find(Extension(path));
		const auto type      = item["type"].get<uint64_t>();

		if (extension == AssetExtensionMap.end() || extension->second == AssetType::None || type != static_cast<uint64_t>(extension->second))
		{
			NV_ERROR("AssetManager: unsupported or mismatched type for '{}'.", text);
			return false;
		}

		const AssetHandle handle{ value };
		const auto key = PathKey(path);

		if (registry.Contains(handle) || pathToHandle.contains(key))
		{
			NV_ERROR("AssetManager: duplicate UUID or path for '{}'.", text);
			return false;
		}

		AssetMetadata metadata;
		metadata.Path     = path;
		metadata.Type     = extension->second;
		metadata.IsLoaded = false;

		registry.Set(handle, metadata);
		pathToHandle.emplace(key, handle);
	}

	s_Registry     = std::move(registry);
	s_PathToHandle = std::move(pathToHandle);

	return true;
}

bool AssetManager::SaveRegistry()
{
	const auto registryPath = ResolvePath("AssetRegistry.nvr");
	if (registryPath.empty())
		return false;

	nlohmann::json document =
	{
		{ "version", 1 },
		{ "assets",  nlohmann::json::array() }
	};

	std::map<std::string, AssetHandle> sorted;
	for (const auto& [handle, metadata] : s_Registry)
		sorted.emplace(PathKey(metadata.Path), handle);

	for (const auto& [path, handle] : sorted)
	{
		const auto& metadata = s_Registry.Get(handle);

		document["assets"].push_back(
		{
			{ "id",   std::format("{:016x}", static_cast<uint64_t>(handle)) },
			{ "path", path },
			{ "type", static_cast<uint32_t>(metadata.Type) }
		});
	}

	return WriteJSON(registryPath, document);
}

// ---------------------------------------------------------------------------
// Asset access
// ---------------------------------------------------------------------------

AssetHandle AssetManager::RegisterAsset(const std::filesystem::path& path)
{
	if (!s_Initialized)
		return AssetHandle{ 0 };

	const auto ext = Extension(path);
	const auto it  = AssetExtensionMap.find(ext);
	if (it == AssetExtensionMap.end())
		return AssetHandle{ 0 };

	const auto relative = path.lexically_normal();
	if (!IsSourcePath(relative))
		return AssetHandle{ 0 };

	const auto absolute = ResolvePath(relative);
	std::error_code ec;
	if (absolute.empty() || !std::filesystem::is_regular_file(absolute, ec) || ec)
		return AssetHandle{ 0 };

	const auto existing = FindByPath(relative);
	if (static_cast<uint64_t>(existing) != 0)
		return s_Registry.Get(existing).Type == it->second ? existing : AssetHandle{ 0 };

	AssetHandle handle;
	while (static_cast<uint64_t>(handle) == 0 || s_Registry.Contains(handle))
		handle = AssetHandle{};

	AssetMetadata metadata;
	metadata.Path = relative;
	metadata.Type = it->second;

	const auto utf8 = relative.generic_u8string();
	const auto key  = std::string(utf8.begin(), utf8.end());
	s_Registry.Set(handle, metadata);
	s_PathToHandle.emplace(key, handle);

	if (!SaveRegistry())
	{
		s_PathToHandle.erase(key);
		s_Registry.Remove(handle);
		return AssetHandle{ 0 };
	}

	return handle;
}

std::shared_ptr<Asset> AssetManager::GetAsset(const std::filesystem::path& path)
{
	return GetAsset(RegisterAsset(path));
}

std::shared_ptr<Asset> AssetManager::GetAsset(AssetHandle handle)
{
	if (!s_Initialized || !s_Registry.Contains(handle))
		return nullptr;

	const auto cached = s_LoadedAssets.find(handle);
	if (cached != s_LoadedAssets.end())
		return cached->second;

	if (s_LoadingAssets.contains(handle))
		return nullptr;

	const auto metadata = s_Registry.Get(handle);

	s_LoadingAssets.insert(handle);

	if (!EnsureImported(handle, false))
	{
		s_LoadingAssets.erase(handle);
		return nullptr;
	}

	const auto cachePath = GetCachePath(handle);
	auto data  = cachePath.empty() ? nullptr : DeserializeAsset(metadata.Type, cachePath);
	auto asset = data ? FinalizeAsset(metadata.Type, *data) : nullptr;

	s_LoadingAssets.erase(handle);

	if (!asset || asset->GetAssetType() != metadata.Type)
		return nullptr;

	asset->Handle = handle;

	AssetMetadata updated = metadata;
	updated.IsLoaded = true;
	s_Registry.Set(handle, updated);

	s_LoadedAssets.emplace(handle, asset);
	return asset;
}

// ---------------------------------------------------------------------------
// Deserialize / Finalize
// ---------------------------------------------------------------------------

std::unique_ptr<AssetData> AssetManager::DeserializeAsset(AssetType type, const std::filesystem::path& path)
{
	switch (type)
	{
		case AssetType::Scene:
		{
			auto data = std::make_unique<SceneAssetData>();
			if (!AssetSerializer::DeserializeScene(path, *data))
				return nullptr;
			return data;
		}
		case AssetType::Shader:
		{
			auto data = std::make_unique<ShaderAssetData>();
			if (!AssetSerializer::DeserializeShader(path, *data))
				return nullptr;
			return data;
		}
		/*case AssetType::Texture:
		{
			auto data = std::make_unique<TextureAssetData>();
			if (!AssetSerializer::DeserializeTexture(path, *data))
				return nullptr;
			return data;
		}*/
		default:
			NV_ERROR("AssetManager: no deserializer for type {}", static_cast<uint32_t>(type));
			return nullptr;
	}
}

std::shared_ptr<Asset> AssetManager::FinalizeAsset(AssetType type, AssetData& data)
{
	switch (type)
	{
		case AssetType::Scene:
		{
			const auto& sceneData = static_cast<const SceneAssetData&>(data);
			auto scene = std::make_shared<Scene>();

			SceneSerializer serializer(*scene);
			if (!serializer.DeserializeFromJSON(sceneData.Document))
				return nullptr;

			return scene;
		}
		/*case AssetType::Shader:
		{
			const auto& shaderData = static_cast<const ShaderAssetData&>(data);

			auto shader = std::make_shared<Shader>();
			shader->Load(shaderData.CachedPath);

			if (!shader->IsValid())
			{
				NV_ERROR("AssetManager: failed to load shader '{}'", shaderData.CachedPath.string());
				return nullptr;
			}

			return shader;
		}
		case AssetType::Texture:
		{
			const auto& texData = static_cast<const TextureAssetData&>(data);

			auto texture = std::make_shared<Texture2D>();

			TextureSpecification spec
			{
				.Format       = texData.Format,
				.GenerateMips = texData.MipLevels > 1,
				.Width        = texData.Width,
				.Height       = texData.Height,
			};

			texture->Create(spec, texData.Pixels.data());

			if (!texture->IsValid())
				return nullptr;

			return texture;
		}*/
		default:
			NV_ERROR("AssetManager: no finalizer for type {}", static_cast<uint32_t>(type));
			return nullptr;
	}
}

// ---------------------------------------------------------------------------
// Import
// ---------------------------------------------------------------------------

bool AssetManager::Import(AssetHandle handle)
{
	return EnsureImported(handle, false);
}

bool AssetManager::Reimport(AssetHandle handle)
{
	return EnsureImported(handle, true);
}

bool AssetManager::EnsureImported(AssetHandle handle, bool force)
{
	if (!s_Initialized || !s_Registry.Contains(handle))
		return false;

	const auto metadata = s_Registry.Get(handle);

	const auto source = ResolvePath(metadata.Path);
	if (source.empty())
		return false;

	const auto importerIt = s_Importers.find(metadata.Type);
	if (importerIt == s_Importers.end() || !importerIt->second)
	{
		NV_ERROR("AssetManager: no importer for '{}'", metadata.Path.string());
		return false;
	}

	const auto cache    = GetCachePath(handle);
	const auto metaPath = GetMetadataPath(handle);
	if (cache.empty() || metaPath.empty())
		return false;

	// Load existing meta or create a fresh one
	MetaFile meta;
	{
		std::error_code ec;
		const bool exists = std::filesystem::exists(metaPath, ec);
		if (ec)
			return false;

		if (exists)
		{
			if (!meta.Load(metaPath))
			{
				NV_ERROR("AssetManager: failed to load metadata '{}'", metaPath.string());
				return false;
			}

			// Reject metadata belonging to a different asset
			if (meta.GetHandle() != handle || meta.GetType() != metadata.Type)
			{
				NV_ERROR("AssetManager: metadata mismatch for '{}'", metaPath.string());
				return false;
			}
		}
		else
		{
			meta = MetaFile::Create(handle, metadata.Type);
		}
	}

	std::error_code ec;
	std::string sourceHash;
	if (!std::filesystem::is_regular_file(source, ec) || ec || !Fingerprint(source, sourceHash))
		return false;

	const nlohmann::json buildSig =
	{
		{ "source_hash",      sourceHash                            },
		{ "hash_algorithm",   "fnv1a64"                            },
		{ "importer_version", importerIt->second->GetVersion()     },
		{ "type",             static_cast<uint32_t>(metadata.Type) },
		{ "settings",         meta.GetTypeSettings()               }
	};

	if (!force)
	{
		if (meta.GetBuildSignature() == buildSig)
		{
			std::error_code cec;
			if (std::filesystem::is_regular_file(cache, cec) && !cec)
				return true;
		}
	}

	if (s_ImportingAssets.contains(handle))
		return false;
	s_ImportingAssets.insert(handle);

	std::filesystem::create_directories(cache.parent_path(), ec);
	if (ec)
	{
		s_ImportingAssets.erase(handle);
		return false;
	}

	const auto tmp = cache.parent_path() / (cache.stem().string() + ".pending" + cache.extension().string());
	if (ResolvePath(tmp.lexically_relative(s_Root)) != tmp)
	{
		s_ImportingAssets.erase(handle);
		return false;
	}

	const ImportContext importContext
	{
		.Source       = source,
		.Destination  = tmp,
		.TypeSettings = &meta.GetTypeSettings()
	};

	const bool ok = importerIt->second->Import(importContext);

	if (!ok)
	{
		std::filesystem::remove(tmp, ec);
		s_ImportingAssets.erase(handle);
		NV_ERROR("AssetManager: import failed for '{}'", source.string());
		return false;
	}

	// Commit
#if defined(_WIN32)
	const bool committed = MoveFileExW(tmp.c_str(), cache.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	std::filesystem::rename(tmp, cache, ec);
	const bool committed = !ec;
#endif

	if (!committed)
	{
		std::filesystem::remove(tmp, ec);
		s_ImportingAssets.erase(handle);
		return false;
	}

	UnloadAsset(handle);

	meta.GetBuildSignature() = buildSig;
	const bool saved = meta.Save(metaPath);

	s_ImportingAssets.erase(handle);
	return saved;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

AssetHandle AssetManager::FindByPath(const std::filesystem::path& relativePath)
{
	const auto utf8 = relativePath.lexically_normal().generic_u8string();
	const auto key  = std::string(utf8.begin(), utf8.end());
	const auto it   = s_PathToHandle.find(key);
	return it != s_PathToHandle.end() ? it->second : AssetHandle{ 0 };
}

bool AssetManager::IsAssetLoaded(AssetHandle handle)
{
	return s_LoadedAssets.contains(handle);
}

bool AssetManager::IsAssetMissing(AssetHandle handle)
{
	if (!s_Registry.Contains(handle))
		return true;
	const auto path = ResolvePath(s_Registry.Get(handle).Path);
	std::error_code ec;
	return path.empty() || !std::filesystem::is_regular_file(path, ec) || ec;
}

void AssetManager::UnloadAsset(AssetHandle handle)
{
	s_LoadedAssets.erase(handle);

	if (s_Registry.Contains(handle))
	{
		AssetMetadata updated = s_Registry.Get(handle);
		updated.IsLoaded = false;
		s_Registry.Set(handle, updated);
	}
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

std::filesystem::path AssetManager::ResolvePath(const std::filesystem::path& relativePath)
{
	if (s_Root.empty() || relativePath.empty() || relativePath.has_root_path())
		return {};

	std::error_code ec;
	const auto absolute = std::filesystem::weakly_canonical(s_Root / relativePath, ec);
	if (ec)
		return {};

	auto root      = s_Root.begin();
	auto component = absolute.begin();
	for (; root != s_Root.end(); ++root, ++component)
		if (component == absolute.end() || *root != *component)
			return {};

	return component != absolute.end() ? absolute : std::filesystem::path{};
}

std::filesystem::path AssetManager::BucketPath(AssetHandle handle, const char* directory, const std::string& extension)
{
	const auto id       = std::format("{:016x}", static_cast<uint64_t>(handle));
	const auto relative = std::filesystem::path(".n-engine") / directory / id.substr(0, 2) / (id + extension);
	const auto resolved = ResolvePath(relative);
	return resolved == s_Root / relative ? resolved : std::filesystem::path{};
}

std::filesystem::path AssetManager::GetMetadataPath(AssetHandle handle)
{
	return BucketPath(handle, "metadata", ".meta");
}

std::filesystem::path AssetManager::GetCachePath(AssetHandle handle)
{
	if (!s_Registry.Contains(handle))
		return {};

	const auto type      = s_Registry.Get(handle).Type;
	const auto extension = GetAssetCacheExtension(type);

	if (extension.empty())
		return {};

	return BucketPath(handle, "cache", std::string(extension));
}

bool AssetManager::IsSourcePath(const std::filesystem::path& path)
{
	if (path.empty() || path.has_root_path())
		return false;
	const auto normalized = path.lexically_normal();
	return *normalized.begin() != ".n-engine" && normalized != "AssetRegistry.nvr"  && *normalized.begin() != "..";
}

std::string AssetManager::Extension(const std::filesystem::path& path)
{
	auto ext = path.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c)
	{
		return static_cast<char>(std::tolower(c));
	});
	return ext;
}
