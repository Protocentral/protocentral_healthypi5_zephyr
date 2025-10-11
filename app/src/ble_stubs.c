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


/*
 * BLE stubs - provide no-op implementations for BLE symbols when
 * CONFIG_HEALTHYPI_BLE_ENABLED is not set. This avoids linker errors
 * from modules that reference BLE APIs while allowing the BLE module
 * to be excluded from the build.
 */

#include <zephyr/kernel.h>
#include <stdint.h>

#include "ble_module.h"

#ifndef CONFIG_HEALTHYPI_BLE_ENABLED

/* Provide semaphores referenced elsewhere */
K_SEM_DEFINE(sem_ble_connected, 0, 1);
K_SEM_DEFINE(sem_ble_disconnected, 0, 1);

/* No-op initializer */
void ble_module_init(void)
{
    /* BLE disabled - nothing to initialize */
}

/* Notification helpers: no-op when BLE disabled */
void ble_ecg_notify_single(int32_t ecg_data) { (void)ecg_data; }
void ble_bioz_notify_single(int32_t resp_data) { (void)resp_data; }
void ble_ecg_notify(int32_t *ecg_data, uint8_t len) { (void)ecg_data; (void)len; }
void ble_bioz_notify(int32_t *resp_data, uint8_t len) { (void)resp_data; (void)len; }

/* Command service data sender: no-op */
void healthypi5_service_send_data(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}

#endif /* CONFIG_HEALTHYPI_BLE_ENABLED */
