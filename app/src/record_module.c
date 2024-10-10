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

#include "sys_sm_module.h"
#include "sampling_module.h"
#include "cmd_module.h"
#include "fs_module.h"

LOG_MODULE_REGISTER(record_module);

extern struct fs_mount_t *mp;

#define FILE_TRANSFER_BLE_PACKET_SIZE    	64 // (16*7)

static int lsdir(const char *path)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;

    fs_dir_t_init(&dirp);

    /* Verify fs_opendir() */
    res = fs_opendir(&dirp, path);
    if (res)
    {
        LOG_ERR("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    LOG_PRINTK("\nListing dir %s ...\n", path);
    for (;;)
    {
        /* Verify fs_readdir() */
        res = fs_readdir(&dirp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0)
        {
            if (res < 0)
            {
                LOG_ERR("Error reading dir [%d]\n", res);
            }
            break;
        }

        if (entry.type == FS_DIR_ENTRY_DIR)
        {
            LOG_PRINTK("[DIR ] %s\n", entry.name);
        }
        else
        {
            LOG_PRINTK("[FILE] %s (size = %zu)\n",
                       entry.name, entry.size);
        }
    }

    /* Verify fs_closedir() */
    fs_closedir(&dirp);

    return res;
}





void record_wipe_all(void)
{
    int err;
    struct fs_dir_t dir;

    char file_name[100] = "";

    fs_dir_t_init(&dir);

    err = fs_opendir(&dir, "/lfs/log");
    if (err)
    {
        printk("Unable to open (err %d)", err);
    }

    while (1)
    {
        struct fs_dirent entry;

        err = fs_readdir(&dir, &entry);
        if (err)
        {
            printk("Unable to read directory");
            break;
        }

        /* Check for end of directory listing */
        if (entry.name[0] == '\0')
        {
            break;
        }

        // printk("%s%s %d\n", entry.name,
        //	      (entry.type == FS_DIR_ENTRY_DIR) ? "/" : "",entry.size);

        // if (strstr(entry.name, "") != NULL)
        //{
        strcpy(file_name, "/lfs/log/");
        strcat(file_name, entry.name);

        printk("Deleting %s\n", file_name);
        fs_unlink(file_name);
    }

    fs_closedir(&dir);
}