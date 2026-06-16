cmake_minimum_required(VERSION 3.20)
project(SplatApp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release so the BVH and path tracer are fast out of the box.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

find_package(assimp REQUIRED)
find_package(glm REQUIRED)

add_executable(SplatApp
    src/main.cpp
    src/debug_ui.cpp
    src/shader.cpp
    src/camera.cpp
    src/stb_image.cpp
    src/tiny_bvh.cpp
    src/json.cpp
    src/ray_model.cpp
    src/ray_camera.cpp
    src/image_buffer.cpp
    src/sampling.cpp
    src/point_light.cpp
    src/env_light.cpp
    src/photon_tracer.cpp
    src/photon_mapper.cpp
    src/path_tracer.cpp
    src/surface_splatter.cpp
    src/volume_splatter.cpp
    src/combined_splatter.cpp
    src/scene.cpp
    src/scene_gl.cpp
    src/scene_config.cpp
    src/bsdf.cpp
    src/kernel_texture.cpp
    src/mesh.cpp
    src/model.cpp
    src/tinyexr.cpp
    external/glad/src/glad.c
    external/tinyexr/miniz.c
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(SplatApp PRIVATE
    include
    external/glad/include
    external/stb
    external/tinybvh
    external/imgui
    external/imgui/backends
    external/nlohmann
    external/tinyexr
)

target_link_libraries(SplatApp PRIVATE glfw glm::glm assimp::assimp)

# Enable native SIMD for tinybvh (SSE4.2 / AVX2). Safe to disable if cross-compiling.
if(NOT MSVC)
    target_compile_options(SplatApp PRIVATE -march=native)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 REQUIRED)
    include(Catch)
    add_subdirectory(tests)
endif()
