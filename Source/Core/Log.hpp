#pragma once

#include <memory>

#include <spdlog/spdlog.h>

class Log final
{
public:
	static void Initialize();
	static void Shutdown();

	static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }

private:
	static std::shared_ptr<spdlog::logger> s_Logger;
};

#if defined(NOVA_DIST)

	#define NV_TRACE(...)    ((void)0)
	#define NV_INFO(...)     ((void)0)
	#define NV_WARN(...)     ((void)0)
	#define NV_ERROR(...)    ((void)0)
	#define NV_CRITICAL(...) ((void)0)

#else

	#define NV_TRACE(...) SPDLOG_LOGGER_CALL(::Log::GetLogger(), spdlog::level::trace, __VA_ARGS__)
	#define NV_INFO(...) SPDLOG_LOGGER_CALL(::Log::GetLogger(), spdlog::level::info, __VA_ARGS__)
	#define NV_WARN(...) SPDLOG_LOGGER_CALL(::Log::GetLogger(), spdlog::level::warn, __VA_ARGS__)
	#define NV_ERROR(...) SPDLOG_LOGGER_CALL(::Log::GetLogger(), spdlog::level::err, __VA_ARGS__)
	#define NV_CRITICAL(...) SPDLOG_LOGGER_CALL(::Log::GetLogger(), spdlog::level::critical, __VA_ARGS__)

#endif
