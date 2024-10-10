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
extern struct healthypi_session_log_header_t healthypi_session_log_header_data;

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

void record_write_to_file(int ecg_ppg_counter, struct hpi_sensor_logging_data_t *current_session_log_points)
{
    printf("Write to file... %d\n", healthypi_session_log_header_data.session_id);

    struct fs_file_t file;
    fs_file_t_init(&file);

    char session_name[50] = "/SD:/";
    char session_id_str[20];
    char sensor_data[50];

    
    sprintf(session_id_str, "%d", healthypi_session_log_header_data.session_id);
    strcat(session_name, session_id_str);
    strcat(session_name, ".csv");

    printk("session_name %s\n",session_name);

    int rc = fs_open(&file, session_name, FS_O_CREATE | FS_O_RDWR | FS_O_APPEND);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d", session_name, rc);
    }

    for (int i = 0; i < ecg_ppg_counter; i++)
    {
        //printk("ecg_sample %d",current_session_log_points[i].ecg_sample);
        //printk("raw_ir %d\n",current_session_log_points[i].raw_ir);
        //sprintf(sensor_data,"%d, %d\n",current_session_log_points[i].ecg_sample,current_session_log_points[i].raw_ir);
        sprintf(sensor_data,"%d\n",current_session_log_points[i].ecg_sample);
        rc = fs_write(&file, sensor_data, strlen(sensor_data));
    }


    rc = fs_close(&file);
    rc = fs_sync(&file);

    printk("Log buffer data written to log file %d\n",healthypi_session_log_header_data.session_id);
}