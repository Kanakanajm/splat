add_executable(SplatTests
    smoke_test.cpp
    ray_model_test.cpp
    bvh_intersect_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ray_model.cpp
    ${CMAKE_SOURCE_DIR}/src/tiny_bvh.cpp
)

target_include_directories(SplatTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/tinybvh
)

target_compile_definitions(SplatTests PRIVATE
    TEST_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)

target_link_libraries(SplatTests PRIVATE Catch2::Catch2WithMain assimp::assimp)

catch_discover_tests(SplatTests)
