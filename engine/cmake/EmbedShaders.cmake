# Compiles GLSL compute shaders to SPIR-V at build time and embeds each module
# as a C array, so the engine carries its shaders inside the binary and never
# looks for a file at runtime. A packaged app has no shader directory to find.

find_program(TERRAIN_GLSLC NAMES glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin)

# terrain_embed_shaders(<target> SHADERS <file>...)
#
# Each <file> is a GLSL compute shader. For `hydraulic_droplet.comp` the
# function generates `<binary_dir>/shaders/hydraulic_droplet_spv.h`, declaring
# `terrain::gpu::shaders::hydraulicDroplet` as a `std::uint32_t` array.
function(terrain_embed_shaders target)
    cmake_parse_arguments(ARG "" "" "SHADERS" ${ARGN})

    set(generatedDirectory ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${generatedDirectory})
    set(generatedHeaders "")

    foreach(shaderPath IN LISTS ARG_SHADERS)
        get_filename_component(shaderName ${shaderPath} NAME_WE)
        terrain_shader_symbol_name(${shaderName} symbolName)

        set(binaryPath ${generatedDirectory}/${shaderName}.spv.inc)
        set(headerPath ${generatedDirectory}/${shaderName}_spv.h)

        # The dependency scan is what makes editing a shared .glsl include
        # rebuild every shader that pulls it in.
        add_custom_command(
            OUTPUT ${binaryPath}
            COMMAND ${TERRAIN_GLSLC} --target-env=vulkan1.2 -O -mfmt=c
                    -I ${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/shaders
                    -MD -MF ${binaryPath}.d
                    -o ${binaryPath} ${CMAKE_CURRENT_SOURCE_DIR}/${shaderPath}
            DEPFILE ${binaryPath}.d
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderPath}
            COMMENT "Compiling shader ${shaderPath}"
            VERBATIM
        )

        file(WRITE ${headerPath}
"#pragma once\n\n#include <cstdint>\n\nnamespace terrain::gpu::shaders {\n\ninline constexpr std::uint32_t ${symbolName}[] =\n#include \"${shaderName}.spv.inc\"\n;\n\n}  // namespace terrain::gpu::shaders\n")

        list(APPEND generatedHeaders ${binaryPath})
    endforeach()

    add_custom_target(${target}_shaders DEPENDS ${generatedHeaders})
    add_dependencies(${target} ${target}_shaders)
    target_include_directories(${target} PRIVATE ${generatedDirectory})
endfunction()

# hydraulic_droplet -> hydraulicDroplet
function(terrain_shader_symbol_name shaderName outputVariable)
    string(REPLACE "_" ";" wordList ${shaderName})
    list(POP_FRONT wordList firstWord)
    set(symbolName ${firstWord})
    foreach(word IN LISTS wordList)
        string(SUBSTRING ${word} 0 1 initial)
        string(SUBSTRING ${word} 1 -1 remainder)
        string(TOUPPER ${initial} initial)
        string(APPEND symbolName ${initial}${remainder})
    endforeach()
    set(${outputVariable} ${symbolName} PARENT_SCOPE)
endfunction()
