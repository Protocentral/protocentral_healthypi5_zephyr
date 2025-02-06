
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/fatal.h>

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include "fs_module.h"

#include <app_version.h>

#include "ble_module.h"
// #include "tf/main_functions.h"

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
LOG_MODULE_REGISTER(healthypi5, LOG_LEVEL);

int main(void)
{
	LOG_INF("HealthyPi 5 started !! FW version: %d.%d.%d", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_PATCHLEVEL);

	return 0;
}

/*void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf)
{
    LOG_PANIC();

    LOG_ERR("Fatal error: %u. Rebooting...", reason);
    sys_reboot(SYS_REBOOT_COLD);

    CODE_UNREACHABLE;
}*/
