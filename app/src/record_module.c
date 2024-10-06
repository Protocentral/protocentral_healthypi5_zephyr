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

LOG_MODULE_REGISTER(record_module);

extern struct fs_mount_t *mp;
extern struct healthypi_session_log_header_t healthypi_session_log_header;


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



void create_record()
{
    struct fs_file_t file;
    struct fs_dir_t dir;
    
    char fname[]="/lfs/log/test_file";
    fs_dir_t_init(&dir);
    fs_file_t_init(&file);  
    int rc;


    rc = fs_opendir(&dir, mp->mnt_point);
    printk("%s opendir: %d\n", mp->mnt_point, rc);

    if (rc < 0) {
        printk("Failed to open directory");
    }

    while (rc >= 0) {
        struct fs_dirent ent = { 0 };

        rc = fs_readdir(&dir, &ent);
        if (rc < 0) {
            printk("Failed to read directory entries");
            break;
        }
        if (ent.name[0] == 0) {
            printk("End of files\n");
            break;
        }
        printk("  %c %u %s\n",
            (ent.type == FS_DIR_ENTRY_FILE) ? 'F' : 'D',
            ent.size,
            ent.name);
    }

    rc = fs_open(&file, fname, FS_O_CREATE | FS_O_RDWR);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d\\n", fname, rc);
        return;
    }

    uint32_t boot_count[5] = {23,45,67,87,34};

    rc = fs_write(&file, &boot_count, sizeof(boot_count));
    printk("%s write new boot count %u: %d\n", fname,*boot_count, rc);

    rc = fs_close(&file);


    uint32_t read_boot_count[5];

    rc = fs_open(&file, fname, FS_O_RDWR);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d\\n", fname, rc);
        return;
    }

    rc = fs_read(&file, &read_boot_count, sizeof(read_boot_count));
    printk("%s read count %u: %d\n", fname, *read_boot_count, rc);
    

    rc = fs_close(&file);

    fs_closedir(&dir);

    for (int i=0;i<sizeof(read_boot_count)/sizeof(uint32_t);i++)
    {
        printk("%d\n",read_boot_count[i]);
    }


}

void record_write_to_file(int current_session_log_counter, struct hpi_sensor_logging_data_t *current_session_log_points)
{
    struct fs_file_t file;
    struct fs_statvfs sbuf;

    fs_file_t_init(&file);

    char fname[30] = "/lfs/log/";

    printf("Write to file... %d\n", healthypi_session_log_header.session_id);
    char session_id_str[20];
    sprintf(session_id_str, "%d", healthypi_session_log_header.session_id);
    strcat(fname, session_id_str);

    int rc = fs_open(&file, fname, FS_O_CREATE | FS_O_RDWR | FS_O_APPEND);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d", fname, rc);
    }

    for (int i = 0; i < current_session_log_counter; i++)
    {
        rc = fs_write(&file, &current_session_log_points[i], 10);
    }


    rc = fs_close(&file);
    rc = fs_sync(&file);

    rc = fs_statvfs(mp->mnt_point, &sbuf);
    if (rc < 0)
    {
        printk("FAIL: statvfs: %d\n", rc);
    }
    printk("Log buffer data written to log file %d\n",healthypi_session_log_header.session_id);
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