cmake_minimum_required(VERSION 3.24)
project(TerrainExamplePlugin LANGUAGES C)

# The only thing a plugin needs: the SDK header. Nothing is linked.
set(TERRAIN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/sdk"
    CACHE PATH "Directory holding terrain_node_sdk.h")

add_library(terrain_example_plugin MODULE speckle.c)

set_target_properties(terrain_example_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "speckle"
    C_VISIBILITY_PRESET hidden
)

# The loader looks for the platform's own extension, and CMake gives a MODULE
# library `.so` on macOS unless it is told otherwise.
if(APPLE)
    set_target_properties(terrain_example_plugin PROPERTIES SUFFIX ".dylib")
endif()

target_include_directories(terrain_example_plugin PRIVATE ${TERRAIN_SDK_DIR})
target_compile_features(terrain_example_plugin PRIVATE c_std_11)
