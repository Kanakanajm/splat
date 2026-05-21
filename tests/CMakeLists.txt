add_executable(SplatTests
    smoke_test.cpp
    ray_model_test.cpp
    bvh_intersect_test.cpp
    bvh_intersect_image_test.cpp
    sampling_test.cpp
    point_light_test.cpp
    photon_tracer_test.cpp
    scene_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ray_model.cpp
    ${CMAKE_SOURCE_DIR}/src/tiny_bvh.cpp
    ${CMAKE_SOURCE_DIR}/src/ray_camera.cpp
    ${CMAKE_SOURCE_DIR}/src/image_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/sampling.cpp
    ${CMAKE_SOURCE_DIR}/src/point_light.cpp
    ${CMAKE_SOURCE_DIR}/src/photon_tracer.cpp
    ${CMAKE_SOURCE_DIR}/src/scene.cpp
)

target_include_directories(SplatTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/tinybvh
)

target_compile_definitions(SplatTests PRIVATE
    TEST_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
    TEST_OUTPUT_DIR="${CMAKE_BINARY_DIR}/tests/output"
)

target_link_libraries(SplatTests PRIVATE Catch2::Catch2WithMain assimp::assimp)

catch_discover_tests(SplatTests)
