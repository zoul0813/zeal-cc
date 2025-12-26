cmake_minimum_required(VERSION 3.16)

# Application Version
set(APP_VERSION_MAJOR 0)
set(APP_VERSION_MINOR 1)
set(APP_VERSION_PATCH 0)
set(APP_NAME "C Compiler")

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)

# Ensure the bin directory exists
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)

zos_use(zvb)
find_package(ZVB_SDK REQUIRED)

zos_use(coreutils)
find_package(COREUTILS REQUIRED)

# When using CMake as a build system, it's possible to mix ASM and C in the compilation
project(cc C ASM)

function(add_program target_name)
    cmake_parse_arguments(ARG "" "" "SRCS;LIBS;ILIBS;DEFS" ${ARGN})

    add_executable(${target_name} ${ARG_UNPARSED_ARGUMENTS})
    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
    target_include_directories(${target_name} PRIVATE $ENV{ZVB_SDK_PATH}/include)

    target_compile_options(${target_name} PRIVATE
        $<$<COMPILE_LANGUAGE:C>:--nostdlib>
        $<$<COMPILE_LANGUAGE:C>:--opt-code-size>
    )
    if(ARG_DEFS)
        target_compile_definitions(${target_name} PRIVATE ${ARG_DEFS})
    endif()

    if(ARG_LIBS)
        target_link_libraries(${target_name} PRIVATE ${ARG_LIBS})
    endif()

    if(ARG_ILIBS)
        foreach(lib IN LISTS ARG_ILIBS)
            # target_link_libraries(${target_name} PUBLIC "-k ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} -l ${lib}.lib")
            zos_link_libraries(${target_name} PRIVATE ${lib})
        endforeach()
    endif()

    zos_add_outputs(${target_name})

    add_custom_command(TARGET ${target_name}_bin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/debug
        COMMAND ${CMAKE_COMMAND} -E rename
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_name}.cdb
            ${CMAKE_SOURCE_DIR}/debug/${target_name}.cdb
        COMMAND ${CMAKE_COMMAND} -E rename
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_name}.map
            ${CMAKE_SOURCE_DIR}/debug/${target_name}.map
        COMMAND ${CMAKE_COMMAND} -E rename
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_name}.ihx
            ${CMAKE_SOURCE_DIR}/debug/${target_name}.ihx
        COMMAND ${CMAKE_COMMAND} -E rename
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_name}.bin
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target_name}
        COMMENT "Moving debug files to debug directory"
        VERBATIM
    )
endfunction()

add_program(cc
    "src/cc/main.c"
    ILIBS core
)

add_program(cc_parse
    "src/parser/main.c"
    "src/parser/lexer.c"
    "src/parser/parser.c"
    "src/common/common.c"
    "src/common/type.c"
    "src/common/ast_write.c"
    "src/target/zeal8bit/target_args.c"
    "src/target/zeal8bit/target_io.c"
    ILIBS core
)

add_program(cc_codegen
    "src/codegen/main.c"
    "src/codegen/codegen.c"
    "src/codegen/codegen_strings.c"
    "src/common/common.c"
    "src/common/ast_read.c"
    "src/common/ast_reader/ast_reader_init.c"
    "src/common/ast_reader/ast_reader_load_strings.c"
    "src/common/ast_reader/ast_reader_string.c"
    "src/common/ast_reader/ast_reader_read_type_info.c"
    "src/common/ast_reader/ast_reader_begin_program.c"
    "src/common/ast_reader/ast_reader_skip_tag.c"
    "src/common/ast_reader/ast_reader_skip_node.c"
    "src/common/ast_reader/ast_reader_destroy.c"
    "src/target/zeal8bit/target_args.c"
    "src/target/zeal8bit/target_io.c"
    ILIBS core
)

add_program(ast_dump
    "src/tools/ast_dump.c"
    "src/common/common.c"
    "src/common/type.c"
    "src/common/ast_read.c"
    "src/common/ast_reader/ast_reader_init.c"
    "src/common/ast_reader/ast_reader_load_strings.c"
    "src/common/ast_reader/ast_reader_string.c"
    "src/common/ast_reader/ast_reader_read_type_info.c"
    "src/common/ast_reader/ast_reader_begin_program.c"
    "src/common/ast_reader/ast_reader_destroy.c"
    "src/target/zeal8bit/target_io.c"
    ILIBS core
)


# Create a custom target that increments build number on every build
add_custom_target(increment_build_number
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_NUMBER_FILE=${CMAKE_CURRENT_LIST_DIR}/build_number.txt
        -DVERSION_TEMPLATE=${CMAKE_CURRENT_LIST_DIR}/src/version.h.in
        -DVERSION_OUTPUT=${CMAKE_CURRENT_LIST_DIR}/src/version.h
        -DAPP_VERSION_MAJOR=${APP_VERSION_MAJOR}
        -DAPP_VERSION_MINOR=${APP_VERSION_MINOR}
        -DAPP_VERSION_PATCH=${APP_VERSION_PATCH}
        -DAPP_NAME=${APP_NAME}
        -P ${CMAKE_CURRENT_LIST_DIR}/increment_version.cmake
    BYPRODUCTS ${CMAKE_CURRENT_LIST_DIR}/src/version.h
    COMMENT "Incrementing build number and generating version.h"
)

# Make zshell depend on the version increment
add_dependencies(cc increment_build_number)
