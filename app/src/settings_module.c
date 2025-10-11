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


#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/settings/settings.h>

#include "hpi_common_types.h"
#include "settings_module.h"

LOG_MODULE_REGISTER(settings_module);

extern struct fs_mount_t *mp;
extern bool sd_card_present;

#define SETTINGS_FILE_PATH "/SD:/hpi_settings.txt"

/* Default values are assigned to settings values consuments
 * All of them will be overwritten if storage contain proper key-values
 */
uint8_t angle_val;
uint64_t length_val = 100;
uint16_t length_1_val = 40;
uint32_t length_2_val = 60;
int32_t voltage_val = -3000;

int hpi_settings_handle_set(const char *name, size_t len, settings_read_cb read_cb,
                            void *cb_arg);
int hpi_settings_handle_commit(void);
int hpi_settings(int (*cb)(const char *name,
                           const void *value, size_t val_len));

/* dynamic main tree handler */
struct settings_handler hpi_handler = {
    .name = "hpi",
    .h_get = NULL,
    .h_set = hpi_settings_handle_set,
    .h_commit = hpi_settings_handle_commit,
    .h_export = hpi_settings};

int hpi_settings_handle_set(const char *name, size_t len, settings_read_cb read_cb,
                            void *cb_arg)
{
    const char *next;
    size_t next_len;
    int rc;

    if (settings_name_steq(name, "angle/1", &next) && !next)
    {
        if (len != sizeof(angle_val))
        {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &angle_val, sizeof(angle_val));
        printk("<hpi/angle/1> = %d\n", angle_val);
        return 0;
    }

    next_len = settings_name_next(name, &next);

    if (!next)
    {
        return -ENOENT;
    }

    if (!strncmp(name, "length", next_len))
    {
        next_len = settings_name_next(name, &next);

        if (!next)
        {
            rc = read_cb(cb_arg, &length_val, sizeof(length_val));
            printk("<hpi/length> = %" PRId64 "\n", length_val);
            return 0;
        }

        if (!strncmp(next, "1", next_len))
        {
            rc = read_cb(cb_arg, &length_1_val,
                         sizeof(length_1_val));
            printk("<hpi/length/1> = %d\n", length_1_val);
            return 0;
        }

        if (!strncmp(next, "2", next_len))
        {
            rc = read_cb(cb_arg, &length_2_val,
                         sizeof(length_2_val));
            printk("<hpi/length/2> = %d\n", length_2_val);
            return 0;
        }

        return -ENOENT;
    }

    return -ENOENT;
}

int hpi_settings_handle_commit(void)
{
    printk("loading all settings is done\n");
    return 0;
}

int hpi_settings(int (*cb)(const char *name,
                           const void *value, size_t val_len))
{
    printk("export keys under <hpi> handler\n");
    (void)cb("hpi/angle/1", &angle_val, sizeof(angle_val));
    (void)cb("hpi/length", &length_val, sizeof(length_val));
    (void)cb("hpi/length/1", &length_1_val, sizeof(length_1_val));
    (void)cb("hpi/length/2", &length_2_val, sizeof(length_2_val));

    return 0;
}

void init_settings()
{
    int rc;

    rc = settings_subsys_init();
    if (rc)
    {
        printk("settings subsys initialization: fail (err %d)\n", rc);
        return;
    }

    rc = settings_register(&hpi_handler);
    if (rc)
    {
        printk("subtree <%s> handler registered: fail (err %d)\n",
               hpi_handler.name, rc);
    }

    // settings_load();

    int32_t val_s32 = 25;
    /* save certain key-value directly*/
    printk("\nsave <hpi/beta/voltage> key directly: ");
    rc = settings_save_one("hpi/beta/voltage", (const void *)&val_s32,
                           sizeof(val_s32));
}

/**
 * Save HR source to filesystem
 * Format: "hr_source=0" or "hr_source=1" in /SD:/hpi_settings.txt
 */
void settings_save_hr_source(enum hpi_hr_source source)
{
    if (!sd_card_present) {
        LOG_WRN("SD card not present, cannot save HR source setting");
        return;
    }

    struct fs_file_t file;
    fs_file_t_init(&file);
    
    int rc = fs_open(&file, SETTINGS_FILE_PATH, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
    if (rc < 0) {
        LOG_ERR("Failed to open settings file for writing: %d", rc);
        return;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "hr_source=%d\n", (int)source);
    
    ssize_t written = fs_write(&file, buf, strlen(buf));
    if (written < 0) {
        LOG_ERR("Failed to write HR source to file: %d", (int)written);
    } else {
        LOG_INF("Saved HR source: %s", source == HR_SOURCE_ECG ? "ECG" : "PPG");
    }

    fs_close(&file);
}

/**
 * Load HR source from filesystem
 * Returns: HR_SOURCE_ECG (default) or HR_SOURCE_PPG
 */
enum hpi_hr_source settings_load_hr_source(void)
{
    enum hpi_hr_source source = HR_SOURCE_ECG; // Default
    
    if (!sd_card_present) {
        LOG_INF("SD card not present, using default HR source: ECG");
        return source;
    }

    struct fs_file_t file;
    fs_file_t_init(&file);
    
    int rc = fs_open(&file, SETTINGS_FILE_PATH, FS_O_READ);
    if (rc < 0) {
        LOG_INF("Settings file not found, using default HR source: ECG");
        return source;
    }

    char buf[32];
    ssize_t bytes_read = fs_read(&file, buf, sizeof(buf) - 1);
    fs_close(&file);

    if (bytes_read > 0) {
        buf[bytes_read] = '\0'; // Null terminate
        
        // Parse "hr_source=X"
        char *eq = strchr(buf, '=');
        if (eq != NULL) {
            int value = atoi(eq + 1);
            if (value == 1) {
                source = HR_SOURCE_PPG;
                LOG_INF("Loaded HR source from file: PPG");
            } else {
                LOG_INF("Loaded HR source from file: ECG");
            }
        }
    }

    return source;
}