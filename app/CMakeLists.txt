#-------------------------------------------------------------------------------
# Zephyr Example Application
#
# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Apply Zephyr patches before build
#file(GLOB_RECURSE files RELATIVE ${CMAKE_SOURCE_DIR} "patches/*.diff")
#foreach(file ${files})
#    execute_process(COMMAND 
#                    patch -p1 -d $ENV{ZEPHYR_BASE} -i ${CMAKE_CURRENT_SOURCE_DIR}/${file} -r - --no-backup-if-mismatch)
#endforeach()

project(app LANGUAGES C)

FILE(GLOB app_sources src/*.c)
FILE(GLOB display_sources src/display/*.c src/images/*.c src/ui/*.c src/ui/screens/*.c)

target_sources(app PRIVATE ${app_sources} ${display_sources})

# Add vital stats module
target_sources(app PRIVATE src/vital_stats.c)

# Add display module only if enabled
#target_sources_ifdef(CONFIG_HEALTHYPI_DISPLAY_ENABLED app PRIVATE ${display_sources})

#FILE(GLOB bt_sources src/bt/*.c)
#target_sources_ifdef(CONFIG_BT app PRIVATE ${bt_sources})

zephyr_include_directories(src)
zephyr_include_directories(src/display)
zephyr_include_directories(src/ui)
zephyr_include_directories(src/bt)