# EmbedShader.cmake
#
# Like EmbedResource.cmake, but first expands GLSL `#include "..."` directives
# by inlining the referenced files (resolved relative to SOURCE_DIR).
#
# dethrace compiles its shaders from embedded C string arrays, so the vendored
# AMD FSR1 headers cannot be included by the GL driver at runtime (GLSL include
# extensions are not available on our GL 3.1 / GLES 3.0 contexts). Expanding
# them at build time keeps the FSR1 sources untouched on disk.
#
# Invoked from add_custom_command():
#   cmake -DSOURCE_DIR=<glrend source dir> -DOUTPUT_DIR=<glrend binary dir>
#         -DFILE=<shader file> -P EmbedShader.cmake

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is not set")
endif()

if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR is not set")
endif()

if(NOT DEFINED FILE)
    message(FATAL_ERROR "FILE is not set")
endif()

file(READ "${SOURCE_DIR}/${FILE}" content)

string(REGEX MATCHALL "#include \"[^\"]+\"" includes "${content}")
foreach(include_line IN LISTS includes)
    string(REGEX REPLACE "^#include \"([^\"]+)\"$" "\\1" include_path "${include_line}")
    if(NOT EXISTS "${SOURCE_DIR}/${include_path}")
        message(FATAL_ERROR "EmbedShader: cannot resolve ${include_line} in ${FILE}")
    endif()
    file(READ "${SOURCE_DIR}/${include_path}" include_content)
    string(REPLACE "${include_line}" "${include_content}" content "${content}")
endforeach()

# The shader source is written out first because the byte-array conversion
# below needs a file to read in hex mode.
set(expanded_file "${OUTPUT_DIR}/${FILE}.expanded")
file(WRITE "${expanded_file}" "${content}")

string(REPLACE "." "_" generated_name "${FILE}")
string(REPLACE "/" "_" generated_name "${generated_name}")
string(TOUPPER "${generated_name}" generated_name)

# Same byte-array layout as cmake/EmbedResource.cmake.
file(READ "${expanded_file}" hex_content HEX)
string(REPEAT "[0-9a-f]" 32 pattern)
string(REGEX REPLACE "(${pattern})" "\\1\n" content "${hex_content}")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " content "${content}")
string(REGEX REPLACE ", $" "" content "${content}")
set(array_definition "static const char ${generated_name}[] =\n{\n${content}\n};")
set(source "// Auto generated file.\n${array_definition}\n")
file(WRITE "${OUTPUT_DIR}/${FILE}.h" "${source}")
