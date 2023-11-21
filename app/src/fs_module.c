#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <stdio.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/disk_access.h>

#include <zephyr/storage/flash_map.h>
#include <zephyr/settings/settings.h>

#include "sys_sm_module.h"
#include "sampling_module.h"

LOG_MODULE_REGISTER(fs_module);

K_SEM_DEFINE(sem_fs_module, 0, 1);

const char fname_sessions[30] = "/lfs/sessions";


FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
    .type = FS_LITTLEFS,
    .fs_data = &storage,
    .storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
    .mnt_point = "/lfs",
};

struct fs_mount_t *mp = &lfs_storage_mnt;

// SD Storage

#include <ff.h>

/*
 *  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
 *  in ffconf.h
 */
#define DISK_DRIVE_NAME "SDMMC"
#define DISK_MOUNT_PT "/"DISK_DRIVE_NAME":"

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp_sd = {
	.type = FS_FATFS,
    .flags = FS_MOUNT_FLAG_NO_FORMAT,
    .storage_dev = (void *)DISK_DRIVE_NAME,
	.fs_data = &fat_fs,
};

static const char *disk_mount_pt = DISK_MOUNT_PT;

static int littlefs_flash_erase(unsigned int id)
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

    /* Optional wipe flash contents */
    if (IS_ENABLED(CONFIG_APP_WIPE_STORAGE))
    {
        rc = flash_area_erase(pfa, 0, pfa->fa_size);
        LOG_ERR("Erasing flash area ... %d", rc);
    }

    flash_area_close(pfa);
    return rc;
}

static int littlefs_mount(struct fs_mount_t *mp)
{
    int rc;

    rc = littlefs_flash_erase((uintptr_t)mp->storage_dev);
    if (rc < 0)
    {
        return rc;
    }

    /* Do not mount if auto-mount has been enabled */
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
}

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

void sd_fs_init(void)
{
    mp_sd.mnt_point = disk_mount_pt;

    int res = fs_mount(&mp_sd);
}

void fs_module_init(void)
{
    int rc;
    struct fs_statvfs sbuf;

    printk("Initing FS...\n");

    rc = littlefs_mount(mp);
    if (rc < 0)
    {
        return;
    }

    rc = fs_statvfs(mp->mnt_point, &sbuf);
    if (rc < 0)
    {
        // printk("FAIL: statvfs: %d\n", rc);
        // goto out;
    }

    printk("%s: bsize = %lu ; frsize = %lu ;"
           " blocks = %lu ; bfree = %lu\n",
           mp->mnt_point,
           sbuf.f_bsize, sbuf.f_frsize,
           sbuf.f_blocks, sbuf.f_bfree);

    rc = lsdir("/lfs");
    if (rc < 0)
    {
        LOG_PRINTK("FAIL: lsdir %s: %d\n", mp->mnt_point, rc);
        // goto out;
    }
    
    //sd_fs_init();
}
