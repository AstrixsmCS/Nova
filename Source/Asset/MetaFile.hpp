#pragma once

#include "Asset.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <vector>

class MetaFile
{
public:
	MetaFile() = default;

	MetaFile(AssetHandle handle, AssetType type)
		: m_Handle(handle), m_Type(type)
	{
	}

	static MetaFile Create(AssetHandle handle, AssetType type);
	static void SetDefaultTypeSettings(AssetType type, MetaFile& meta);

	bool Load(const std::filesystem::path& path);
	bool Save(const std::filesystem::path& path) const;

	AssetHandle GetHandle() const { return m_Handle; }
	AssetType   GetType()   const { return m_Type;   }

	const nlohmann::json& GetTypeSettings() const { return m_TypeSettings; }
	nlohmann::json&       GetTypeSettings()       { return m_TypeSettings; }

	const nlohmann::json& GetBuildSignature() const { return m_BuildSignature; }
	nlohmann::json&       GetBuildSignature()       { return m_BuildSignature; }

	const std::vector<AssetHandle>& GetDependencies() const { return m_Dependencies; }

	void AddDependency(AssetHandle dependency);
	void ClearDependencies();

private:
	inline static constexpr uint32_t FormatVersion = 1;

	AssetHandle m_Handle{ 0 };
	AssetType   m_Type = AssetType::None;

	std::vector<AssetHandle> m_Dependencies;

	nlohmann::json m_TypeSettings = nlohmann::json::object();
	nlohmann::json m_BuildSignature = nlohmann::json::object();
};
