#include "SceneImporter.hpp"

#include "Asset/AssetSerializer.hpp"
#include "Core/Log.hpp"

bool SceneImporter::Import(const std::filesystem::path& source, const std::filesystem::path& destination)
{
	std::error_code ec;
	std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec)
	{
		NV_ERROR("SceneImporter: failed to copy '{}' to cache: {}", source.string(), ec.message());
		return false;
	}

	return true;
}
