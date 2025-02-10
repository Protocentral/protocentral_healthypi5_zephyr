#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <stdio.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/settings/settings.h>

#include "hpi_common_types.h"

LOG_MODULE_REGISTER(settings_module);

extern struct fs_mount_t *mp;

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