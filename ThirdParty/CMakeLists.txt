# ==== spdlog ====
add_subdirectory(spdlog)

# ==== SDL ====

set(SDL_SHARED       OFF)
set(SDL_STATIC       ON)
set(SDL_TEST_LIBRARY OFF)
set(SDL_TESTS        OFF)
set(SDL_EXAMPLES     OFF)
set(SDL_INSTALL      OFF)
set(SDL_INSTALL_DOCS OFF)

set(SDL_AUDIO        OFF)
set(SDL_CAMERA       OFF)
set(SDL_SENSOR       OFF)
set(SDL_POWER        OFF)
set(SDL_DIALOG       ON)
set(SDL_TRAY         OFF)
set(SDL_GPU          OFF)
set(SDL_RENDER       ON)
set(SDL_OPENGL       OFF)
set(SDL_OPENGLES     OFF)
set(SDL_KMSDRM       OFF)
set(SDL_HAPTIC       OFF)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/SDL3 SYSTEM)

target_compile_definitions(SDL3-static INTERFACE SDL_MAIN_HANDLED)

# ==== JSON ====

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/json SYSTEM)

# ==== EnTT ====

add_subdirectory(entt SYSTEM)

# ==== GLM (Header-only library) ====

set(GLM_BUILD_LIBRARY OFF)
set(GLM_BUILD_TESTS   OFF)
set(GLM_BUILD_INSTALL OFF)
set(GLM_ENABLE_CXX_20 ON)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glm SYSTEM)

target_compile_definitions(glm-header-only INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE)

# ==== fastgltf ====
set(FASTGLTF_COMPILE_AS_CPP20 ON)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/fastgltf SYSTEM)

# ==== ImGui (Docking branch) ====

add_library(imgui STATIC
		${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_tables.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_widgets.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_demo.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_sdl3.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_sdlrenderer3.cpp
)

add_library(imgui::imgui ALIAS imgui)

target_include_directories(imgui PUBLIC
		${CMAKE_CURRENT_LIST_DIR}/imgui
		${CMAKE_CURRENT_LIST_DIR}/imgui/backends
)

target_compile_features(imgui PUBLIC cxx_std_17)

target_link_libraries(imgui PUBLIC SDL3-static)
