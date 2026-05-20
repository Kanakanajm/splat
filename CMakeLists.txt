cmake_minimum_required(VERSION 3.20)
project(SplatApp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(assimp REQUIRED)
find_package(glm REQUIRED)

add_executable(SplatApp
    src/main.cpp
    src/debug_ui.cpp
    src/shader.cpp
    src/camera.cpp
    src/stb_image.cpp
    src/tiny_bvh.cpp
    src/ray_model.cpp
    src/mesh.cpp
    src/model.cpp
    external/glad/src/glad.c
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
)

target_link_libraries(SplatApp PRIVATE glfw glm::glm assimp::assimp)

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 REQUIRED)
    include(Catch)
    add_subdirectory(tests)
endif()
