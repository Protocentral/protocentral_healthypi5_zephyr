# Install script for directory: /Users/akw/Documents/GitHub/my_workspace/zephyr

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/opt/nordic/ncs/toolchains/20d68df7e5/opt/zephyr-sdk/arm-zephyr-eabi/bin/arm-zephyr-eabi-objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/zephyr/arch/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/zephyr/lib/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/zephyr/soc/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/zephyr/boards/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/zephyr/subsys/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/zephyr/drivers/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/healthypi5_zephyr/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/cmsis/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/fatfs/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/hal_nordic/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/hal_rpi_pico/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/libmetal/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/littlefs/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/lvgl/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/mcuboot/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/open-amp/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/tinycrypt/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/modules/zcbor/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/zephyr/kernel/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/zephyr/cmake/flash/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/zephyr/cmake/usage/cmake_install.cmake")
  include("/Users/akw/Documents/GitHub/my_workspace/healthypi5_zephyr/app_move/build/zephyr/cmake/reports/cmake_install.cmake")

endif()

