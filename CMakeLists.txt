cmake_minimum_required(VERSION 4.0.2)
project(atlas)

set(CMAKE_CXX_STANDARD 20)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

set(SDLSHADERCROSS_VENDORED ON)

# This assumes the SDL source is available in vendored/SDL
add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL_shadercross EXCLUDE_FROM_ALL)

add_executable(atlas main.cpp
        main.cpp
        src/ResourceManager.cpp
        src/ResourceManager.h
        src/SpriteBatch.cpp
        src/SpriteBatch.h
        src/math.h
        src/Camera.h
        src/Camera.cpp
        src/Transform.h
        src/CameraSystem.cpp
        src/CameraSystem.h
        src/hex/HexMapData.cpp
        src/hex/HexMapData.h
        src/hex/HexMapRenderer.cpp
        src/hex/HexMapRenderer.h
)

# Link to the actual SDL3 library.
target_link_libraries(atlas
        PRIVATE
        SDL3::SDL3
        SDL3_shadercross::SDL3_shadercross
)

add_custom_command(
        TARGET atlas
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_SOURCE_DIR}/content "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/content")
