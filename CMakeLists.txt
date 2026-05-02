# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

# Generate compile_commands.json for clangd and other tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ATS_STM32)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Build only the small set of source files needed for the current bring-up task.
# This keeps old project-specific code out of the build while the new hardware
# test program is being brought up.
target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hardware.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/adc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gpio.cpp
)

