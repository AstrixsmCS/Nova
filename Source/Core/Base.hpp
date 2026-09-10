#pragma once

// ==== Platform Detection ====

#if defined(_WIN64) || defined(_WIN32)
#define NV_PLATFORM_WINDOWS
#elif defined(__linux__)
#define NV_PLATFORM_LINUX
#else
#error "Unsupported platform! Nova supports Windows and Linux."
#endif

// ==== Compiler Detection ====

#if defined(__clang__)
#define NV_COMPILER_CLANG
#elif defined(__GNUC__)
#define NV_COMPILER_GCC
#elif defined(_MSC_VER)
#define NV_COMPILER_MSVC
#else
#error "Unknown compiler! Nova only supports MSVC, GCC, and Clang."
#endif
