cmake_minimum_required(VERSION 3.16)

# Application Version
set(APP_VERSION_MAJOR 0)
set(APP_VERSION_MINOR 1)
set(APP_VERSION_PATCH 0)
set(APP_NAME "C Compiler")
set(target_name "cc")

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
project(${target_name} C ASM)

# Include menuconfig support (optional)
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/menuconfig.cmake)
    include(${CMAKE_CURRENT_LIST_DIR}/menuconfig.cmake)
endif()

add_executable(${target_name}
    "src/main.c"
)

target_include_directories(${target_name} PRIVATE $ENV{ZVB_SDK_PATH}/include)
target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

# Link with .lib extension for SDCC
target_link_libraries(${target_name} zvb_gfx core keyboard)

target_compile_options(${target_name} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:--nostdlib>
)

# Add menuconfig support to the zshell target (if available)
if(COMMAND add_menuconfig_support)
    add_menuconfig_support(${target_name})
endif()

zos_add_outputs(${target_name})

# Rename target, and move debug files
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
add_dependencies(${target_name} increment_build_number)