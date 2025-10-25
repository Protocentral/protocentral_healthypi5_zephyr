/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Ashwin Whitchurch, ProtoCentral Electronics
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */



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

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
LOG_MODULE_REGISTER(healthypi5, LOG_LEVEL);

int main(void)
{
	LOG_INF("HealthyPi 5 started !! FW version: %d.%d.%d", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_PATCHLEVEL);

	return 0;
}

void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    LOG_PANIC();

    LOG_ERR("========================================");
    LOG_ERR("FATAL ERROR at uptime: %lld ms", k_uptime_get());
    LOG_ERR("Reason: %u", reason);
    
    // Log reason codes
    switch (reason) {
        case K_ERR_CPU_EXCEPTION:
            LOG_ERR("CPU Exception");
            break;
        case K_ERR_KERNEL_PANIC:
            LOG_ERR("Kernel Panic");
            break;
        case K_ERR_STACK_CHK_FAIL:
            LOG_ERR("Stack Check Failure (Stack Overflow)");
            break;
        case K_ERR_KERNEL_OOPS:
            LOG_ERR("Kernel Oops");
            break;
        default:
            LOG_ERR("Unknown fatal error");
            break;
    }
    LOG_ERR("========================================");
    
    // Give time for log to flush
    k_sleep(K_MSEC(100));
    
    // Don't reboot - stay in error state for debugging
    while (1) {
        k_sleep(K_FOREVER);
    }

    CODE_UNREACHABLE;
}

