cmake_minimum_required(VERSION 3.24)
project(TerrainEngine LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TERRAIN_ENGINE_BUILD_TESTS "Build the engine test binary" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CMakeDependentOption)
include(EmbedShaders)
include(FetchContent)

find_package(ZLIB REQUIRED)

# Vulkan is resolved at runtime, so only the headers are a build dependency;
# a machine with no loader still builds, and erodes on the CPU.
cmake_dependent_option(TERRAIN_ENGINE_ENABLE_VULKAN
    "Build the Vulkan compute backend for erosion" ON "TERRAIN_GLSLC" OFF)

include(CheckLanguage)
check_language(CUDA)
cmake_dependent_option(TERRAIN_ENGINE_ENABLE_CUDA
    "Build the CUDA compute backend for erosion" ON "CMAKE_CUDA_COMPILER" OFF)

if(TERRAIN_ENGINE_ENABLE_VULKAN)
    FetchContent_Declare(
        VulkanHeaders
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG v1.4.313
        GIT_SHALLOW ON
    )
    FetchContent_MakeAvailable(VulkanHeaders)
endif()

add_library(terrain_engine STATIC
    src/version.cpp
    src/heightfield.cpp
    src/color_field.cpp
    src/point_set.cpp
    src/hash.cpp
    src/curve.cpp
    src/color_ramp.cpp
    src/color_space.cpp
    src/histogram.cpp
    src/noise.cpp
    src/json_writer.cpp
    src/resample.cpp
    src/thumbnail.cpp
    src/registry.cpp
    src/plugin_host.cpp
    src/graph.cpp
    src/evaluator.cpp
    src/evaluation_context.cpp
    src/formats/massifdem.cpp
    src/formats/png_image.cpp
    src/formats/hillshade.cpp
    src/formats/heightfield_png.cpp
    src/formats/raw_float32.cpp
    src/formats/elevation_import.cpp
    src/formats/texture_maps.cpp
    src/formats/point_files.cpp
    src/nodes/node_support.cpp
    src/nodes/shape_support.cpp
    src/nodes/builtin_nodes.cpp
    src/nodes/input_nodes.cpp
    src/nodes/generate_nodes.cpp
    src/nodes/landform_nodes.cpp
    src/nodes/filter_nodes.cpp
    src/nodes/transform_nodes.cpp
    src/nodes/adjust_nodes.cpp
    src/nodes/mask_nodes.cpp
    src/nodes/erosion_support.cpp
    src/nodes/erosion_nodes.cpp
    src/gpu/erosion_backend.cpp
    src/gpu/denormal_mode.cpp
    src/nodes/combine_nodes.cpp
    src/nodes/texture_nodes.cpp
    src/nodes/color_nodes.cpp
    src/nodes/vegetation_nodes.cpp
    src/nodes/output_nodes.cpp
)

target_include_directories(terrain_engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>
)
target_include_directories(terrain_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/gpu)

target_link_libraries(terrain_engine PRIVATE ZLIB::ZLIB ${CMAKE_DL_LIBS})

if(TERRAIN_ENGINE_ENABLE_CUDA)
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    set_property(TARGET terrain_engine PROPERTY CUDA_STANDARD 20)
    # The CUDA runtime is linked statically so the engine still loads on a
    # machine with no CUDA installed; the backend then finds no device and the
    # CPU path runs. It is a PUBLIC requirement because the bridge links this
    # static library from a CXX-only project, where CMake adds no runtime of
    # its own and the kernel registration symbols would go unresolved.
    set_property(TARGET terrain_engine PROPERTY CUDA_RUNTIME_LIBRARY Static)
    target_link_libraries(terrain_engine PUBLIC CUDA::cudart_static)
    set_property(TARGET terrain_engine PROPERTY CUDA_ARCHITECTURES native)
    target_sources(terrain_engine PRIVATE src/gpu/cuda_erosion.cu)
    target_compile_definitions(terrain_engine PRIVATE TERRAIN_WITH_CUDA=1)

    # These four are the determinism contract expressed as compiler flags, not
    # a tuning choice. Fusing a multiply-add, approximating a divide or a root,
    # or keeping denormals the host has been told to flush would each give a
    # different terrain from the same graph.
    target_compile_options(terrain_engine PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:-fmad=false>
        $<$<COMPILE_LANGUAGE:CUDA>:--prec-div=true>
        $<$<COMPILE_LANGUAGE:CUDA>:--prec-sqrt=true>
        $<$<COMPILE_LANGUAGE:CUDA>:-ftz=true>
    )
endif()

if(TERRAIN_ENGINE_ENABLE_VULKAN)
    target_sources(terrain_engine PRIVATE
        src/gpu/vulkan_api.cpp
        src/gpu/vulkan_device.cpp
        src/gpu/vulkan_erosion.cpp
        src/gpu/float_parity.cpp
    )
    target_link_libraries(terrain_engine PRIVATE Vulkan::Headers)
    target_compile_definitions(terrain_engine PRIVATE TERRAIN_WITH_VULKAN=1)
    terrain_embed_shaders(terrain_engine SHADERS
        src/gpu/shaders/float_parity.comp
        src/gpu/shaders/thermal_scatter.comp
        src/gpu/shaders/apply_delta.comp
        src/gpu/shaders/apply_net_change.comp
        src/gpu/shaders/add_constant.comp
        src/gpu/shaders/hydraulic_droplet.comp
    )

    add_executable(terrain_erosion_benchmark tools/erosion_benchmark.cpp)
    target_link_libraries(terrain_erosion_benchmark PRIVATE terrain_engine)
    target_include_directories(terrain_erosion_benchmark PRIVATE src/gpu)
    target_compile_features(terrain_erosion_benchmark PRIVATE cxx_std_20)

    add_executable(terrain_gpu_probe tools/gpu_probe.cpp)
    target_link_libraries(terrain_gpu_probe PRIVATE terrain_engine Vulkan::Headers ${CMAKE_DL_LIBS})
    target_include_directories(terrain_gpu_probe PRIVATE src/gpu)
    target_compile_features(terrain_gpu_probe PRIVATE cxx_std_20)
endif()

target_compile_features(terrain_engine PUBLIC cxx_std_20)
target_compile_definitions(terrain_engine PRIVATE
    TERRAIN_ENGINE_VERSION="${PROJECT_VERSION}"
)

# A contracted multiply-add rounds once where the GPU's rounds twice, or the
# other way about. The erosion result has to match on both, so the host is
# denied the contraction the shaders cannot be given.
if(NOT MSVC)
    target_compile_options(terrain_engine PRIVATE -ffp-contract=off)
endif()

# The bridge links the engine into a shared .node module.
set_target_properties(terrain_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(Terrain::Engine ALIAS terrain_engine)

if(TERRAIN_ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
