add_executable(SplatTests
    smoke_test.cpp
)

target_link_libraries(SplatTests PRIVATE Catch2::Catch2WithMain)

catch_discover_tests(SplatTests)
