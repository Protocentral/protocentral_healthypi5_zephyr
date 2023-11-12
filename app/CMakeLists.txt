#-------------------------------------------------------------------------------
# Zephyr Example Application
#
# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# Add display module only if enabled
FILE(GLOB display_sources src/display/*.c src/images/*.c)
target_sources_ifdef(CONFIG_HEALTHYPI_DISPLAY_ENABLED app PRIVATE ${display_sources})

zephyr_include_directories(src)
zephyr_include_directories(src/display)