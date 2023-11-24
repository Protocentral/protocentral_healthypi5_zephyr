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

LOG_MODULE_REGISTER(record_module);

extern struct fs_mount_t *mp;

void record_write_to_file(int session_log_id, struct hpi_sensor_data_t *session_log_points, int session_log_length)
{
    struct fs_file_t file;
    struct fs_statvfs sbuf;

    fs_file_t_init(&file);

    fs_mkdir("/lfs/log");

    char fname[30] = "/lfs/log/";

    printf("Write to file... %d\n", session_log_id);
    char session_id_str[5];
    sprintf(session_id_str, "%d", session_log_id);
    strcat(fname, session_id_str);

    printf("Session Length: %d\n", session_log_length);

    int rc = fs_open(&file, fname, FS_O_CREATE | FS_O_RDWR | FS_O_APPEND);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d", fname, rc);
    }
    // Session log header
    // rc=fs_write(&file, &current_session_log_, sizeof(current_session_log_id));

    for (int i = 0; i < session_log_length; i++)
    {
        rc = fs_write(&file, &session_log_points[i], sizeof(struct hpi_sensor_data_t));
    }

    rc = fs_close(&file);
    rc = fs_sync(&file);

    rc = fs_statvfs(mp->mnt_point, &sbuf);
    if (rc < 0)
    {
        printk("FAIL: statvfs: %d\n", rc);
        // goto out;
    }
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