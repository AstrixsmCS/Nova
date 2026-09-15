#include "MetaFile.hpp"

#include "Importers/TextureImporter.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
	#include <Windows.h>
#endif

namespace
{
	std::string HandleToString(AssetHandle handle)
	{
		return std::format("{:016x}", static_cast<uint64_t>(handle));
	}

	bool HandleFromString(const std::string& value, AssetHandle& handle)
	{
		if (value.size() != 16)
			return false;

		uint64_t parsed = 0;

		const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed, 16);

		if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed == 0)
		{
			return false;
		}

		handle = AssetHandle{ parsed };
		return true;
	}
}

MetaFile MetaFile::Create(AssetHandle handle, AssetType type)
{
	MetaFile meta(handle, type);

	SetDefaultTypeSettings(type, meta);

	return meta;
}

void MetaFile::SetDefaultTypeSettings(AssetType type, MetaFile& meta)
{
	nlohmann::json& settings = meta.GetTypeSettings();

	settings = nlohmann::json::object();

	switch (type)
	{
		case AssetType::Texture:
			settings = TextureImportSettings{}.ToJSON();
			break;

		case AssetType::Scene:
		case AssetType::Shader:
		case AssetType::None:
			break;
	}
}

bool MetaFile::Load(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		return false;

	nlohmann::json document = nlohmann::json::parse(stream, nullptr, false);

	if (stream.bad() || document.is_discarded() || !document.is_object())
		return false;

	if (!document.contains("version") || !document["version"].is_number_unsigned())
		return false;

	if (document["version"].get<uint64_t>() != FormatVersion)
		return false;

	if (!document.contains("id") || !document["id"].is_string())
		return false;

	AssetHandle handle{ 0 };
	if (!HandleFromString(document["id"].get<std::string>(), handle))
		return false;

	if (!document.contains("type") || !document["type"].is_number_unsigned())
		return false;

	const uint64_t typeValue = document["type"].get<uint64_t>();
	if (typeValue > static_cast<uint64_t>(std::numeric_limits<uint8_t>::max()))
		return false;

	const AssetType type = static_cast<AssetType>(typeValue);

	switch (type)
	{
		case AssetType::Scene:
		case AssetType::Shader:
		case AssetType::Texture:
			break;
		case AssetType::None:
		default:
			return false;
	}

	if (!document.contains("type_settings") || !document["type_settings"].is_object())
		return false;

	std::vector<AssetHandle> dependencies;

	if (document.contains("dependencies"))
	{
		if (!document["dependencies"].is_array())
			return false;

		dependencies.reserve(document["dependencies"].size());

		for (const nlohmann::json& dependencyValue : document["dependencies"])
		{
			if (!dependencyValue.is_string())
				return false;

			AssetHandle dependency{ 0 };
			if (!HandleFromString(dependencyValue.get<std::string>(), dependency))
				return false;

			if (std::find(dependencies.begin(), dependencies.end(), dependency) == dependencies.end())
				dependencies.push_back(dependency);
		}
	}

	m_Handle       = handle;
	m_Type         = type;
	m_Dependencies = std::move(dependencies);
	m_TypeSettings = document["type_settings"];

	if (document.contains("built"))
	{
		if (!document["built"].is_object())
			return false;

		m_BuildSignature = document["built"];
	}
	else
	{
		m_BuildSignature = nlohmann::json::object();
	}

	return true;
}

bool MetaFile::Save(const std::filesystem::path& path) const
{
	if (static_cast<uint64_t>(m_Handle) == 0 || m_Type == AssetType::None || !m_TypeSettings.is_object())
		return false;

	std::error_code error;

	if (path.has_parent_path())
	{
		std::filesystem::create_directories(path.parent_path(), error);
		if (error)
			return false;
	}

	nlohmann::ordered_json document
	{
		{ "version",      FormatVersion                      },
		{ "id",           HandleToString(m_Handle)           },
		{ "type",         static_cast<uint32_t>(m_Type)      },
		{ "dependencies", nlohmann::ordered_json::array()    },
		{ "type_settings", m_TypeSettings                    },
		{ "built",        m_BuildSignature                   }
	};

	for (AssetHandle dependency : m_Dependencies)
	{
		if (static_cast<uint64_t>(dependency) == 0)
			continue;

		document["dependencies"].push_back(HandleToString(dependency));
	}

	const std::filesystem::path temporaryPath = path.parent_path() / (path.filename().string() + ".tmp");

	{
		std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
		if (!stream)
			return false;

		stream << document.dump(4) << '\n';
		stream.close();

		if (!stream)
		{
			std::filesystem::remove(temporaryPath, error);
			return false;
		}
	}

#if defined(_WIN32)
	const bool moved = MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
	if (!moved)
	{
		std::filesystem::remove(temporaryPath, error);
		return false;
	}
#else
	std::filesystem::rename(temporaryPath, path, error);
	if (error)
	{
		std::filesystem::remove(temporaryPath, error);
		return false;
	}
#endif

	return true;
}

void MetaFile::AddDependency(AssetHandle dependency)
{
	if (static_cast<uint64_t>(dependency) == 0)
		return;

	const auto existing = std::find(m_Dependencies.begin(), m_Dependencies.end(),dependency);

	if (existing == m_Dependencies.end())
		m_Dependencies.push_back(dependency);
}

void MetaFile::ClearDependencies()
{
	m_Dependencies.clear();
}
