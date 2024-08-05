# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

add_subdirectory_ifdef(CONFIG_SENSOR_MAX30205 max30205)
add_subdirectory_ifdef(CONFIG_SENSOR_AFE4400  afe4400)