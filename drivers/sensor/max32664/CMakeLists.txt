zephyr_include_directories(.)
zephyr_library()
    
zephyr_library_sources_ifdef(CONFIG_SENSOR_MAX32664 max32664.c)