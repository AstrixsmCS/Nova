# ==== Project ====

cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)
project(Nova VERSION 0.1.0 LANGUAGES CXX C)

# === Language ===

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# === Target ===

add_executable(Nova)

file(GLOB_RECURSE NOVA_SOURCES CONFIGURE_DEPENDS
		Source/*.cpp
		Source/*.hpp
)

target_sources(Nova PRIVATE ${NOVA_SOURCES})
target_include_directories(Nova PRIVATE Source)

# == Build configuration ==

target_compile_definitions(Nova PRIVATE
		$<$<CONFIG:Debug>:NV_DEBUG>
		$<$<CONFIG:Release>:NV_RELEASE>

		NOMINMAX
)

# == Compiler options ==

if(MSVC)
	target_compile_options(Nova PRIVATE
			/W4
			/MP
			/Gy
			/Zc:preprocessor
	)
else()
	target_compile_options(Nova PRIVATE
			-Wall
			-Wextra
			-Wpedantic
	)
endif()

# === Dependencies ===

add_subdirectory(ThirdParty)

target_link_libraries(Nova PRIVATE
		spdlog::spdlog
		SDL3::SDL3-static
)
