cmake_minimum_required(VERSION 3.24)
project(TerrainEngine LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TERRAIN_ENGINE_BUILD_TESTS "Build the engine test binary" ON)

add_library(terrain_engine STATIC
    src/version.cpp
)

target_include_directories(terrain_engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(terrain_engine PUBLIC cxx_std_20)
target_compile_definitions(terrain_engine PRIVATE
    TERRAIN_ENGINE_VERSION="${PROJECT_VERSION}"
)

# The bridge links the engine into a shared .node module.
set_target_properties(terrain_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(Terrain::Engine ALIAS terrain_engine)

if(TERRAIN_ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
