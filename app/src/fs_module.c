#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <stdio.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>

#include <zephyr/storage/flash_map.h>
#include <zephyr/settings/settings.h>

#include "sampling_module.h"
#include "fs_module.h"
#include "cmd_module.h"


#if defined(CONFIG_FAT_FILESYSTEM_ELM)
#include <ff.h>
#endif

LOG_MODULE_REGISTER(fs_module);
bool sd_card_present = false;

static FATFS fat_fs;
static struct fs_mount_t sd_fs_mnt = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
    .mnt_point = "/SD:",
};
struct fs_mount_t *mp_sd = &sd_fs_mnt;



/*static int littlefs_flash_erase(unsigned int id)
{
    const struct flash_area *pfa;
    int rc;

    rc = flash_area_open(id, &pfa);
    if (rc < 0)
    {
        LOG_ERR("FAIL: unable to find flash area %u: %d\n",
                id, rc);
        return rc;
    }

    printk("Area %u at 0x%x on %s for %u bytes\n",
           id, (unsigned int)pfa->fa_off, pfa->fa_dev->name,
           (unsigned int)pfa->fa_size);

    // Optional wipe flash contents
    if (IS_ENABLED(CONFIG_APP_WIPE_STORAGE))
    {
        rc = flash_area_erase(pfa, 0, pfa->fa_size);
        LOG_ERR("Erasing flash area ... %d", rc);
    }

    flash_area_close(pfa);
    return rc;
}*/

/*static int littlefs_mount(struct fs_mount_t *mp)
{
    int rc;

    rc = littlefs_flash_erase((uintptr_t)mp->storage_dev);
    if (rc < 0)
    {
        return rc;
    }

    // Do not mount if auto-mount has been enabled 
#if !DT_NODE_EXISTS(PARTITION_NODE) || \
    !(FSTAB_ENTRY_DT_MOUNT_FLAGS(PARTITION_NODE) & FS_MOUNT_FLAG_AUTOMOUNT)
    rc = fs_mount(mp);
    if (rc < 0)
    {
        printk("FAIL: mount id %" PRIuPTR " at %s: %d\n",
               (uintptr_t)mp->storage_dev, mp->mnt_point, rc);
        return rc;
    }
    printk("%s mount: %d\n", mp->mnt_point, rc);
#else
    printk("%s automounted\n", mp->mnt_point);
#endif

    return 0;
}*/

static int lsdir(const char *path)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;

    fs_dir_t_init(&dirp);

    // Verify fs_opendir() 
    res = fs_opendir(&dirp, path);
    if (res)
    {
        LOG_ERR("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    LOG_PRINTK("\nListing dir %s ...\n", path);
    for (;;)
    {
        // Verify fs_readdir() 
        res = fs_readdir(&dirp, &entry);

        // entry.name[0] == 0 means end-of-dir
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

    printk("\n");

    // Verify fs_closedir() 
    fs_closedir(&dirp);

    return res;
}


#ifdef CONFIG_HEALTHYPI_SD_CARD_ENABLED

static int mount_sd_fs()
{
    int rc;
    struct fs_statvfs sbuf;

    rc = fs_mount(&sd_fs_mnt);
    k_sleep(K_MSEC(50));

    if(rc==0)
    {
        printk("\nSuccessfully Mounted FS %s\n", sd_fs_mnt.mnt_point);
        sd_card_present = true;
    }
    else
    {
        printk("\nFailed to mount FS %s\n", sd_fs_mnt.mnt_point);
        sd_card_present = false;
        return rc;
    }


    if (sd_card_present)
    {
        rc = fs_statvfs(sd_fs_mnt.mnt_point, &sbuf);
        if (rc < 0)
        {
            printk("FAIL: statvfs: %d\n", rc);
            return rc;
        }

        printk("%s: bsize = %lu ; frsize = %lu ;"
            " blocks = %lu ; bfree = %lu\n",
            mp_sd->mnt_point,
            sbuf.f_bsize, sbuf.f_frsize,
            sbuf.f_blocks, sbuf.f_bfree);

        rc = lsdir("/SD:");

        return rc;

    }
    else
    {
        printk("unable to access SD card\n");
    }
    return 0;
}

#endif

void fs_module_init(void)
{
    #ifdef CONFIG_HEALTHYPI_SD_CARD_ENABLED
        mount_sd_fs();
    #endif
}


