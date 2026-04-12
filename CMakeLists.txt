cmake_minimum_required(VERSION 3.20)
project(SplatApp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glm REQUIRED)

add_executable(SplatApp
    src/main.cpp
    src/shader.cpp
    src/stb_image.cpp
    external/glad/src/glad.c
)

target_include_directories(SplatApp PRIVATE
    include
    external/glad/include
    external/stb
)

target_link_libraries(SplatApp PRIVATE glfw glm::glm)
