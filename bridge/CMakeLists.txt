cmake_minimum_required(VERSION 3.24)
project(TerrainBridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TERRAIN_ENGINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../engine ${CMAKE_CURRENT_BINARY_DIR}/engine)

execute_process(
    COMMAND node -p "require('node-addon-api').include_dir"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE NODE_ADDON_API_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_library(engine SHARED
    src/addon.cpp
    ${CMAKE_JS_SRC}
)

set_target_properties(engine PROPERTIES
    PREFIX ""
    SUFFIX ".node"
)

target_include_directories(engine PRIVATE
    ${CMAKE_JS_INC}
    ${NODE_ADDON_API_INCLUDE_DIR}
)

target_link_libraries(engine PRIVATE
    terrain_engine
    ${CMAKE_JS_LIB}
)

# Node-API version 8 is the highest both current Node and current Electron
# implement, so one binary serves the test runner and the utilityProcess.
target_compile_definitions(engine PRIVATE
    NAPI_DISABLE_CPP_EXCEPTIONS
    NAPI_VERSION=8
)
