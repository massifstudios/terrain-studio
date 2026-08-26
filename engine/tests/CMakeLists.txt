include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.11.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(Catch2)

add_executable(terrain_engine_tests
    test_version.cpp
    test_heightfield.cpp
    test_massifdem.cpp
    test_png.cpp
    test_hillshade.cpp
    test_registry.cpp
    test_graph.cpp
    test_evaluator.cpp
    test_curve.cpp
    test_noise.cpp
    test_nodes.cpp
    test_erosion.cpp
)

target_link_libraries(terrain_engine_tests PRIVATE
    terrain_engine
    Catch2::Catch2WithMain
)

target_compile_definitions(terrain_engine_tests PRIVATE
    TERRAIN_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(terrain_engine_tests)
