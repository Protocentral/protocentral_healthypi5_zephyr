#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <stdio.h>
#include <zephyr/random/random.h> 

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/settings/settings.h>

#include "cmd_module.h"
#include "sys_sm_module.h"
#include "sampling_module.h"
#include "data_module.h"

#include "stdio.h"
#include "string.h"

LOG_MODULE_REGISTER(record_module);

uint8_t buf_log[1024] = {0};// 56 bytes / session, 18 sessions / packet

extern struct fs_mount_t *mp;
struct healthypi_session_log_header_t healthypi_session_log_header;
extern bool start_log_command_recieved;


#define FILE_TRANSFER_BLE_PACKET_SIZE    	64 // (16*7)

uint32_t transfer_get_file_length(char *m_file_name)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;
    uint32_t file_len = 0;

    fs_dir_t_init(&dirp);
    const char *path = "/lfs/log";

    res = fs_opendir(&dirp, path);
    if (res)
    {
        printk("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    for (;;)
    {
        res = fs_readdir(&dirp, &entry);
        if (res || entry.name[0] == 0)
        {
            if (res < 0)
            {
                printk("Error reading dir [%d]\n", res);
            }
            break;
        }

        if (strncmp(m_file_name,entry.name,sizeof(m_file_name)) == 0)
        {
            //printk(" file name %s : size %d\n",entry.name,entry.size);
            file_len = entry.size;
        }
    }
    fs_closedir(&dirp);
    return file_len;
}

void update_session_size_in_header (uint16_t file_size,char *m_file_path)
{
    struct fs_file_t file;

    struct healthypi_session_log_header_t k_header;

    char fname[50] = "/lfs/log/";

    fs_file_t_init(&file);

    int rc = fs_open(&file, m_file_path, FS_O_CREATE | FS_O_RDWR);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d", fname, rc);
    }

    rc = fs_seek(&file, 0, FS_SEEK_SET);
    
    rc = fs_read(&file, (struct healthypi_session_log_header_t *)&k_header, sizeof(struct healthypi_session_log_header_t));

    rc = fs_close(&file);

    k_header.session_size = file_size;


    rc = fs_open(&file, m_file_path, FS_O_CREATE | FS_O_RDWR);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d", fname, rc);
    }

    rc = fs_seek(&file, 0, FS_SEEK_SET);

    rc = fs_write(&file, &k_header, sizeof(struct healthypi_session_log_header_t));

    rc = fs_close(&file);

    rc = fs_sync(&file);
    printk("Header updated with file size... %d\n", k_header.session_id);

}




void transfer_send_file(uint16_t file_id)
{
    int8_t m_buffer[FILE_TRANSFER_BLE_PACKET_SIZE] = {0};
    uint8_t file_read_buffer[FILE_TRANSFER_BLE_PACKET_SIZE];

    char m_file_name[30];
    char m_file_path[30];
    
    sprintf(m_file_name,"%d",file_id);
    uint32_t file_len = transfer_get_file_length(m_file_name);

    uint16_t number_writes = file_len / FILE_TRANSFER_BLE_PACKET_SIZE;

    uint32_t i = 0;
    struct fs_file_t m_file;
    int rc = 0;

    if (file_len % FILE_TRANSFER_BLE_PACKET_SIZE != 0)
    {
        number_writes++; // Last write will be smaller than 64 bytes
    }

    printk("File name: %s Size:%d NW: %d \n", m_file_name, file_len, number_writes);
    snprintf(m_file_path, sizeof(m_file_path), "/lfs/log/%d", file_id);

    update_session_size_in_header(number_writes,m_file_path);

    fs_file_t_init(&m_file);
    
    rc = fs_open(&m_file, m_file_path, FS_O_READ);

    if (rc != 0)
    {
        printk("Error opening file %d\n", rc);
        return;
    }

    for (i = 0; i < number_writes; i++)
    {
        rc = fs_read(&m_file, m_buffer, FILE_TRANSFER_BLE_PACKET_SIZE);
        if (rc < 0)
        {
            printk("Error reading file %d\n", rc);
            return;
        }

        cmdif_send_ble_file_data(m_buffer,FILE_TRANSFER_BLE_PACKET_SIZE); //FILE_TRANSFER_BLE_PACKET_SIZE);
        k_sleep(K_MSEC(50));
        
    }

    rc = fs_close(&m_file);
    if (rc != 0)
    {
        printk("Error closing file %d\n", rc);
        return;
    }

    printk("File sent\n");
}


void fetch_file_data(uint16_t session_id)
{
    printk("Getting Log id %u data\n", session_id);
    transfer_send_file(session_id);
}

void write_header_to_new_file()
{
    struct fs_file_t file;
    struct fs_statvfs sbuf;

    int rc = fs_mkdir("/lfs/log");

    char fname[50] = "/lfs/log/";

    char session_id_str[20];
    sprintf(session_id_str, "%d", healthypi_session_log_header.session_id);
    strcat(fname, session_id_str);

    fs_file_t_init(&file);

    rc = fs_open(&file, fname, FS_O_CREATE | FS_O_RDWR);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d", fname, rc);
    }
    
    rc = fs_write(&file, &healthypi_session_log_header, sizeof(struct healthypi_session_log_header_t));

    rc = fs_close(&file);

    printf("Header written to file... %d\n", healthypi_session_log_header.session_id);
}


void set_current_session_log_id(uint8_t m_sec, uint8_t m_min, uint8_t m_hour, uint8_t m_day, uint8_t m_month, uint8_t m_year)
{

    //store new log start time temporily
    uint8_t second=0, minute=0, hour=0, day=0, month=0, year=0;
    year = m_year;
    month = m_month;
    day = m_day;
    hour = m_hour;
    minute = m_min;
    second = m_sec;

    record_init_next_session_log();

    //update structure with new log start time
    healthypi_session_log_header.session_start_time.year = year;
    healthypi_session_log_header.session_start_time.month = month;
    healthypi_session_log_header.session_start_time.day = day;
    healthypi_session_log_header.session_start_time.hour = hour;
    healthypi_session_log_header.session_start_time.minute = minute;
    healthypi_session_log_header.session_start_time.second = second;

    uint8_t rand[2];
    sys_rand_get(rand, sizeof(rand));
    healthypi_session_log_header.session_id = (rand[0] | (rand[1] << 8));
    healthypi_session_log_header.session_size = 0;

    printk("Header data for log file %d set\n",healthypi_session_log_header.session_id);
}

uint16_t log_get_count(void)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;

    uint16_t log_count = 0;

    fs_dir_t_init(&dirp);

    const char *path = "/lfs/log";

    /* Verify fs_opendir() */
    res = fs_opendir(&dirp, path);
    if (res)
    {
        printk("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    for (;;)
    {
        /* Verify fs_readdir() */
        res = fs_readdir(&dirp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0)
        {
            if (res < 0)
            {
                printk("Error reading dir [%d]\n", res);
            }
            break;
        }

        if (entry.type != FS_DIR_ENTRY_DIR)
        {
            log_count++;
        }
    }

    /* Verify fs_closedir() */
    fs_closedir(&dirp);

    printk("Total log count: %d\n", log_count);

    cmdif_send_file_count(log_count,0x54);

    return log_count;
}

struct healthypi_session_log_header_t log_get_file_header(uint16_t file_id)
{
    printk("Getting header for file %u\n", file_id);

    struct healthypi_session_log_header_t m_header;

    //printk("Header size: %d\n", sizeof(struct healthypi_session_log_header_t));

    char m_file_name[30];
    snprintf(m_file_name, sizeof(m_file_name), "/lfs/log/%u", file_id);

    struct fs_file_t m_file;
    fs_file_t_init(&m_file);

    int rc = 0;
    rc = fs_open(&m_file, m_file_name, FS_O_READ);

    if (rc != 0)
    {
        printk("Error opening file %d\n", rc);
    }

    rc = fs_read(&m_file, (struct healthypi_session_log_header_t *)&m_header, sizeof(struct healthypi_session_log_header_t));
    if (rc < 0)
    {
        printk("Error reading file %d\n", rc);
    }

    rc = fs_close(&m_file);
    if (rc != 0)
    {
        printk("Error closing file %d\n", rc);
        // return;
    }

    return m_header;
}


void delete_log_file(uint16_t session_id)
{
    char log_file_name[30];

    snprintf(log_file_name, sizeof(log_file_name), "/lfs/log/%d", session_id);

    fs_unlink(log_file_name);
    printk("File deleted %s\n", log_file_name);
}

void delete_all_log_files(void)
{
    int err;
    struct fs_dir_t dir;

    char file_name[30];

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
        strcpy(file_name, "/lfs/log/");
        strcat(file_name, entry.name);

        printk("Deleting %s\n", file_name);
        fs_unlink(file_name);
    }

    fs_closedir(&dir);

    printk("All files deleted\n");
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

int log_get_all_file_header(void)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;

    uint16_t log_count = 0;
    uint16_t buf_log_index = 0;

    fs_dir_t_init(&dirp);

    const char *path = "/lfs/log";

    /* Verify fs_opendir() */
    res = fs_opendir(&dirp, path);
    if (res)
    {
        printk("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    for (;;)
    {
        /* Verify fs_readdir() */
        res = fs_readdir(&dirp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0)
        {
            if (res < 0)
            {
                printk("Error reading dir [%d]\n", res);
            }

            break;
        }

        if (entry.type != FS_DIR_ENTRY_DIR)
        {
            uint16_t session_id = atoi(entry.name);
            struct healthypi_session_log_header_t m_header = log_get_file_header(session_id);
            memcpy(&buf_log, &m_header, sizeof(struct healthypi_session_log_header_t));
            cmdif_send_ble_data_idx(buf_log, sizeof(struct healthypi_session_log_header_t));
            printk("Header of file id %d sent\n",session_id);
        }

    }
    fs_closedir(&dirp);

    return res;
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