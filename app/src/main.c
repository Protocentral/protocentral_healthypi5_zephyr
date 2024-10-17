
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

#include "fs_module.h"

#include <app_version.h>

#include "ble_module.h"
// #include "tf/main_functions.h"

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
LOG_MODULE_REGISTER(healthypi5, LOG_LEVEL);

int main(void)
{
	printk("\nHealthyPi 5 RP2040 started !! FW version: %d.%d.%d \n\n", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_PATCHLEVEL);

	return 0;
}