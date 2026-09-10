# spdlog
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

