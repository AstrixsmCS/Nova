#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>

struct ImportContext
{
	std::filesystem::path Source;
	std::filesystem::path Destination;
	const nlohmann::json* TypeSettings = nullptr;
};
