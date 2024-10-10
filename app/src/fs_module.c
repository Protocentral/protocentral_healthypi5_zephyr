#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <stdio.h>

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>

#include <zephyr/storage/flash_map.h>
#include <zephyr/settings/settings.h>

#include <zephyr/random/random.h>


#include "sys_sm_module.h"
#include "sampling_module.h"
#include "fs_module.h"
#include "cmd_module.h"


#if defined(CONFIG_FAT_FILESYSTEM_ELM)
#include <ff.h>
#endif

LOG_MODULE_REGISTER(fs_module);

K_SEM_DEFINE(sem_fs_module, 0, 1);

struct healthypi_session_log_header_t healthypi_session_log_header_data;



/*FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
    .type = FS_LITTLEFS,
    .fs_data = &storage,
    .storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
    .mnt_point = "/lfs",
};*/


static FATFS fat_fs;
static struct fs_mount_t sd_fs_mnt = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
    .mnt_point = "/SD:",
};
struct fs_mount_t *mp_sd = &sd_fs_mnt;


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

    printk("\n");

    /* Verify fs_closedir() */
    fs_closedir(&dirp);

    return res;
}

void write_header_to_new_file()
{
    struct fs_file_t file;
    int rc;
    char session_name[50] = "/SD:/";

    char session_id_str[20];
    sprintf(session_id_str, "%d", healthypi_session_log_header_data.session_id);
    strcat(session_name, session_id_str);
    strcat(session_name, ".csv");

    char session_record_details[200] = "Session started at: ";
    char session_record_time[2];

    sprintf(session_record_time, "%d", healthypi_session_log_header_data.session_start_time.day);
    strcat(session_record_details, session_record_time);

    strcat(session_record_details, "/");

    sprintf(session_record_time, "%d", healthypi_session_log_header_data.session_start_time.month);
    strcat(session_record_details, session_record_time);

    strcat(session_record_details, "/");

    sprintf(session_record_time, "%d", healthypi_session_log_header_data.session_start_time.year);
    strcat(session_record_details, session_record_time);

    strcat(session_record_details, " ");

    sprintf(session_record_time, "%d", healthypi_session_log_header_data.session_start_time.hour);
    strcat(session_record_details, session_record_time);

    strcat(session_record_details, ":");

    sprintf(session_record_time, "%d", healthypi_session_log_header_data.session_start_time.minute);
    strcat(session_record_details, session_record_time);

    strcat(session_record_details, ":");

    sprintf(session_record_time, "%d", healthypi_session_log_header_data.session_start_time.second);
    strcat(session_record_details, session_record_time);
    strcat(session_record_details, "\n");

    char session_vital_header[100] = "ECG,PPG,RESP\n";

    fs_file_t_init(&file);

    rc = fs_open(&file, session_name, FS_O_CREATE | FS_O_RDWR);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d", session_name, rc);
    }

    rc = fs_write(&file, session_record_details, strlen(session_record_details));
    rc = fs_write(&file, session_vital_header, strlen(session_vital_header));


    rc = fs_close(&file);
    rc = fs_sync(&file);

    printf("Header written to file... %d\n", healthypi_session_log_header_data.session_id);
}

void set_current_session_log_id(uint8_t m_sec, uint8_t m_min, uint8_t m_hour, uint8_t m_day, uint8_t m_month, uint8_t m_year)
{
    //printk("m_sec %d m_min %d, m_hour %d m_day %d m_month %d m_year %d\n", m_sec, m_min, m_hour, m_day, m_month, m_year);
    uint8_t second, minute, hour, day, month, year;
    year = m_year;
    month = m_month;
    day = m_day;
    hour = m_hour;
    minute = m_min;
    second = m_sec;

    // update structure with new log start time
    healthypi_session_log_header_data.session_start_time.year = year;
    healthypi_session_log_header_data.session_start_time.month = month;
    healthypi_session_log_header_data.session_start_time.day = day;
    healthypi_session_log_header_data.session_start_time.hour = hour;
    healthypi_session_log_header_data.session_start_time.minute = minute;
    healthypi_session_log_header_data.session_start_time.second = second;

    uint8_t rand[2];
    sys_rand_get(rand, sizeof(rand));
    healthypi_session_log_header_data.session_id = (rand[0] | (rand[1] << 8));
    healthypi_session_log_header_data.session_size = 0;

    printk("Header data for log file %d set\n", healthypi_session_log_header_data.session_id);
}

void record_write_to_file(int ecg_ppg_counter, struct hpi_sensor_logging_data_t *current_session_log_points)
{
    struct fs_file_t file;
    fs_file_t_init(&file);

    char session_name[50] = "/SD:/";
    char session_id_str[20];
    char sensor_data[50];

    printf("Write to file... %d\n", healthypi_session_log_header_data.session_id);
    
    sprintf(session_id_str, "%d", healthypi_session_log_header_data.session_id);
    strcat(session_name, session_id_str);
    strcat(session_name, '.csv');

    int rc = fs_open(&file, session_name, FS_O_CREATE | FS_O_RDWR | FS_O_APPEND);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d", session_name, rc);
    }

    for (int i = 0; i < ecg_ppg_counter; i++)
    {
        sprintf(sensor_data,"%d %d",current_session_log_points[i].ecg_sample,current_session_log_points[i].raw_ir);
        rc = fs_write(&file, sensor_data, strlen(sensor_data));
    }


    rc = fs_close(&file);
    rc = fs_sync(&file);

    printk("Log buffer data written to log file %d\n",healthypi_session_log_header_data.session_id);
}



#ifdef CONFIG_HEALTHYPI_SD_CARD_ENABLED

static int mount_sd_fs()
{
    int rc;
    struct fs_statvfs sbuf;
    struct fs_dir_t dir;

    rc = fs_mount(&sd_fs_mnt);
    k_sleep(K_MSEC(50));

    if(rc==0)
    {
        printk("\nSuccessfully Mounted FS %s\n", sd_fs_mnt.mnt_point);
    }
    else
    {
        printk("\nFailed to mount FS %s\n", sd_fs_mnt.mnt_point);
        return rc;
    }

    rc = fs_statvfs(sd_fs_mnt.mnt_point, &sbuf);
    if (rc < 0)
    {
        printk("FAIL: statvfs: %d\n", rc);
        return;
    }

    printk("%s: bsize = %lu ; frsize = %lu ;"
           " blocks = %lu ; bfree = %lu\n",
           mp_sd->mnt_point,
           sbuf.f_bsize, sbuf.f_frsize,
           sbuf.f_blocks, sbuf.f_bfree);

    rc = lsdir("/SD:");

    return rc;
}

#endif

void fs_module_init(void)
{
    /*int rc;
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

    //fs_mkdir("/lfs/log");
    rc = lsdir("/lfs/log");
    if (rc < 0)
    {
        LOG_PRINTK("FAIL: lsdir %s: %d\n", mp->mnt_point, rc);
        // goto out;
    }*/

#ifdef CONFIG_HEALTHYPI_SD_CARD_ENABLED
    mount_sd_fs();
#endif
}


