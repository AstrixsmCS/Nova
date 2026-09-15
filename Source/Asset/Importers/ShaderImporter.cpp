#include "ShaderImporter.hpp"

#include "Core/Log.hpp"

bool ShaderImporter::Import(const ImportContext& context)
{
	std::error_code ec;
	std::filesystem::copy_file(context.Source, context.Destination, std::filesystem::copy_options::overwrite_existing, ec);

	if (ec)
	{
		NV_ERROR("ShaderImporter: failed to copy '{}' to cache: {}", context.Source.string(), ec.message());
		return false;
	}

	return true;
}
