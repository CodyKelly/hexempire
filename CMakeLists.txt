cmake_minimum_required(VERSION 4.0.2)
project(atlas)

set(CMAKE_CXX_STANDARD 20)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

set(SDLSHADERCROSS_VENDORED ON)

# This assumes the SDL source is available in vendored/SDL
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL_shadercross EXCLUDE_FROM_ALL)

# SDL_image (required by RmlUi SDL_GPU renderer)
# Use stb_image backend to avoid libpng 2.x compatibility issues
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "Build SDL_image samples" FORCE)
set(SDLIMAGE_PNG OFF CACHE BOOL "Disable libpng (use stb instead)" FORCE)
set(SDLIMAGE_PNG_SAVE OFF CACHE BOOL "Disable PNG save" FORCE)
set(SDLIMAGE_AVIF OFF CACHE BOOL "Disable AVIF" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "Disable WebP" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "Disable JXL" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "Disable TIFF" FORCE)
add_subdirectory(vendored/SDL_image EXCLUDE_FROM_ALL)

# RmlUi Configuration
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "Build RmlUi samples")
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "Build Lua bindings")
add_subdirectory(vendored/RmlUi EXCLUDE_FROM_ALL)

# Tracy Profiler
option(TRACY_ENABLE "Enable Tracy profiler" ON)
if (TRACY_ENABLE)
    add_compile_definitions(TRACY_ENABLE)
endif ()

add_subdirectory(vendored/tracy)

add_executable(atlas main.cpp
        # Tracy profiler
        vendored/tracy/public/TracyClient.cpp

        # Core engine
        src/ResourceManager.cpp
        src/ResourceManager.h
        src/SpriteBatch.cpp
        src/SpriteBatch.h
        src/math.h
        src/Transform.h
        src/CameraSystem.cpp
        src/CameraSystem.h

        # Hex system
        src/hex/HexCoord.h
        src/hex/HexGrid.cpp
        src/hex/HexGrid.h
        src/hex/HexMapData.cpp
        src/hex/HexMapData.h
        src/hex/HexMapRenderer.cpp
        src/hex/HexMapRenderer.h
        src/hex/TerritoryGenerator.cpp
        src/hex/TerritoryGenerator.h
        src/hex/IslandDetector.cpp
        src/hex/IslandDetector.h

        # Game logic
        src/game/GameData.h
        src/game/GameController.cpp
        src/game/GameController.h
        src/game/CombatSystem.cpp
        src/game/CombatSystem.h
        src/game/CombatQueue.cpp
        src/game/CombatQueue.h
        src/game/ReplaySystem.cpp
        src/game/ReplaySystem.h
        src/game/AIController.cpp
        src/game/AIController.h
        src/game/InputHandler.cpp
        src/game/InputHandler.h

        # UI
        src/ui/DiceRenderer.cpp
        src/ui/DiceRenderer.h
        src/ui/UIManager.cpp
        src/ui/UIManager.h

        # RmlUi SDL3 GPU Backend
        vendored/RmlUi/Backends/RmlUi_Platform_SDL.cpp
        vendored/RmlUi/Backends/RmlUi_Renderer_SDL_GPU.cpp
)

# Include directories
target_include_directories(atlas PRIVATE
        vendored/tracy/public
        vendored/RmlUi/Include
        vendored/RmlUi/Backends
)

# RmlUi requires RMLUI_SDL_VERSION_MAJOR to be set for SDL3 support
target_compile_definitions(atlas PRIVATE RMLUI_SDL_VERSION_MAJOR=3)

# Link libraries
target_link_libraries(atlas
        PRIVATE
        SDL3::SDL3
        SDL3_shadercross::SDL3_shadercross
        SDL3_image::SDL3_image
        rmlui_core
)

add_custom_command(
        TARGET atlas
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_SOURCE_DIR}/content "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/content")
