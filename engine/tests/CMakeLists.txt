include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.11.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(Catch2)

# The example plugin is the ABI's regression test: it is built exactly as an
# author would build it, from the SDK header alone, and loaded by the suite.
enable_language(C)
set(TERRAIN_TEST_PLUGIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/plugins")

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/example
    ${CMAKE_CURRENT_BINARY_DIR}/example_plugin
)

add_library(terrain_future_abi_plugin MODULE plugins/future_abi_plugin.c)
target_include_directories(terrain_future_abi_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../sdk)
set_target_properties(terrain_future_abi_plugin PROPERTIES PREFIX "" OUTPUT_NAME "future_abi")
if(APPLE)
    set_target_properties(terrain_future_abi_plugin PROPERTIES SUFFIX ".dylib")
endif()

set_target_properties(terrain_example_plugin terrain_future_abi_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${TERRAIN_TEST_PLUGIN_DIR}
)

add_executable(terrain_engine_tests
    test_version.cpp
    test_plugins.cpp
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
    test_erosion_gpu.cpp
    test_texturing.cpp
)

target_link_libraries(terrain_engine_tests PRIVATE
    terrain_engine
    Catch2::Catch2WithMain
)

# The GPU parity tests reach the backend selector directly, which is engine
# internals rather than the node SDK.
target_include_directories(terrain_engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/gpu)

add_dependencies(terrain_engine_tests terrain_example_plugin terrain_future_abi_plugin)

target_compile_definitions(terrain_engine_tests PRIVATE
    TERRAIN_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    TERRAIN_TEST_PLUGIN_DIR="${TERRAIN_TEST_PLUGIN_DIR}"
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(terrain_engine_tests)
