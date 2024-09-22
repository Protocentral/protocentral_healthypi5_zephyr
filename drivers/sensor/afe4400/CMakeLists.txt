# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)
zephyr_library()    
zephyr_library_sources(afe4400.c)
zephyr_library_sources_ifdef(CONFIG_SENSOR_ASYNC_API afe4400_async.c afe4400_decoder.c)
