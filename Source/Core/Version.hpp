#pragma once

#include "Base.hpp"

#define NV_VERSION "v0.1.0a"

// ==== Build Configuration ====

#ifdef NV_DEBUG
	#define NV_BUILD_CONFIG_NAME "Debug"
#elifdef NV_RELEASE
	#define NV_BUILD_CONFIG_NAME "Release"
#else
	#define NV_BUILD_CONFIG_NAME "Unknown"
#endif

// ==== Build Platform ====

#ifdef NV_PLATFORM_WINDOWS
	#define NV_BUILD_PLATFORM_NAME "Windows x64"
#elif defined(NV_PLATFORM_LINUX)
	#define NV_BUILD_PLATFORM_NAME "Linux"
#else
	#define NV_BUILD_PLATFORM_NAME "Unknown"
#endif

#define NV_VERSION_LONG "Nova " NV_VERSION " (" NV_BUILD_PLATFORM_NAME " " NV_BUILD_CONFIG_NAME ")"

// Stable build version (YEAR.SEASON.MAJOR.MINOR) Season is 1(Winter), 2(Spring), 3(Summer), 4(Fall)
