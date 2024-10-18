#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/random/random.h>

#include "fs_module.h"
#include "cmd_module.h"

#include "ble_module.h"
#include "data_module.h"
#include "hw_module.h"
#include "sampling_module.h"

#include "datalog_module.h"

uint8_t buf_log[1024]; // 56 bytes / session, 18 sessions / packet

extern struct fs_mount_t *mp_sd;
extern uint16_t current_session_ecg_counter;
extern struct hpi_sensor_logging_data_t log_buffer[LOG_BUFFER_LENGTH];
struct hpi_log_session_header_t hpi_log_session_header;
extern bool settings_log_data_enabled;

void write_header_to_new_session()
{
    struct fs_file_t file;
    int rc;
    char session_name[50] = "/SD:/";

    char session_id_str[20];
    sprintf(session_id_str, "%d", hpi_log_session_header.session_id);
    strcat(session_name, session_id_str);
    strcat(session_name, ".csv");

    char session_record_details[200];

    sprintf(session_record_details, "Session started at: %d/%d/%d %d:%d:%d\n",
            hpi_log_session_header.session_start_time.day,
            hpi_log_session_header.session_start_time.month,
            hpi_log_session_header.session_start_time.year,
            hpi_log_session_header.session_start_time.hour,
            hpi_log_session_header.session_start_time.minute,
            hpi_log_session_header.session_start_time.second);

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

    printf("Header written to file... %d\n", hpi_log_session_header.session_id);
}

void set_current_session_id(uint8_t m_sec, uint8_t m_min, uint8_t m_hour, uint8_t m_day, uint8_t m_month, uint8_t m_year)
{
    // printk("m_sec %d m_min %d, m_hour %d m_day %d m_month %d m_year %d\n", m_sec, m_min, m_hour, m_day, m_month, m_year);
    uint8_t second, minute, hour, day, month, year;
    year = m_year;
    month = m_month;
    day = m_day;
    hour = m_hour;
    minute = m_min;
    second = m_sec;

    flush_current_session_logs(true);

    // update structure with new session start time
    hpi_log_session_header.session_start_time.year = year;
    hpi_log_session_header.session_start_time.month = month;
    hpi_log_session_header.session_start_time.day = day;
    hpi_log_session_header.session_start_time.hour = hour;
    hpi_log_session_header.session_start_time.minute = minute;
    hpi_log_session_header.session_start_time.second = second;

    uint8_t rand[2];
    sys_rand_get(rand, sizeof(rand));
    hpi_log_session_header.session_id = (rand[0] | (rand[1] << 8));
    hpi_log_session_header.session_size = 0;

    printk("Header data for session %d set\n", hpi_log_session_header.session_id);
}

void get_session_header(uint16_t session_id, struct hpi_log_session_header_t *session_header_data)
{
    struct hpi_log_session_header_t m_header;

    char m_session_name[100];
    snprintf(m_session_name, sizeof(m_session_name), "/SD:/%u.csv", session_id);

    struct fs_file_t m_file;
    fs_file_t_init(&m_file);

    int rc = 0;
    rc = fs_open(&m_file, m_session_name, FS_O_READ);

    if (rc != 0)
    {
        printk("Error opening file %d\n", rc);
    }

    char header_data[36];

    rc = fs_read(&m_file, header_data, 36);
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

    char *saveptr;
    strtok_r(header_data, " ", &saveptr); // "Session"
    strtok_r(NULL, " ", &saveptr);        // "started"
    strtok_r(NULL, " ", &saveptr);        // "at:"

    // Get the date part (14/10/24)
    session_header_data->session_start_time.day = (uint8_t)atoi(strtok_r(NULL, "/", &saveptr));
    session_header_data->session_start_time.month = (uint8_t)atoi(strtok_r(NULL, "/", &saveptr));
    session_header_data->session_start_time.year = (uint8_t)atoi(strtok_r(NULL, " ", &saveptr)); // Space after year

    // Get the time part (11:15:20)
    session_header_data->session_start_time.hour = (uint8_t)atoi(strtok_r(NULL, ":", &saveptr));
    session_header_data->session_start_time.minute = (uint8_t)atoi(strtok_r(NULL, ":", &saveptr));
    session_header_data->session_start_time.second = (uint8_t)atoi(strtok_r(NULL, " ", &saveptr));
}

uint16_t hpi_get_session_count(void)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;

    uint16_t session_count = 0;

    fs_dir_t_init(&dirp);

    const char *path = "/SD:";

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
            session_count++;
        }
    }

    /* Verify fs_closedir() */
    fs_closedir(&dirp);

    printk("Total session count: %d\n", session_count);

    cmdif_send_session_count(session_count);

    return session_count;
}

int hpi_get_session_index(void)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;

    fs_dir_t_init(&dirp);

    const char *path = "/SD:/";

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

            char session_header[80];
            struct hpi_log_session_header_t session_header_data;
            uint16_t session_id = atoi(entry.name);
            uint16_t session_size = entry.size;
            int day, month, year, hour, minute, second;

            get_session_header(session_id, &session_header_data);

            session_header_data.session_id = session_id;
            session_header_data.session_size = session_size;

            memcpy(&buf_log, &session_header_data, sizeof(struct hpi_log_session_header_t));
            cmdif_send_ble_data_idx(buf_log, sizeof(struct hpi_log_session_header_t));
            printk("Header of session id %d sent\n", session_id);
        }
    }
    fs_closedir(&dirp);

    return res;
}

uint32_t hpi_log_session_get_length(char *m_file_name)
{
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;
    uint32_t session_len = 0;

    fs_dir_t_init(&dirp);

    const char *path = "/SD:/";

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

        if (strncmp(m_file_name, entry.name, sizeof(m_file_name)) == 0)
        {
            // printk(" file name %s : size %d\n",entry.name,entry.size);
            session_len = entry.size;
        }
    }
    fs_closedir(&dirp);
    return session_len;
}

void hpi_session_fetch(uint16_t session_id)
{
    int8_t m_buffer[FILE_TRANSFER_BLE_PACKET_SIZE] = {0};

    char m_session_name[30];
    char m_session_path[30];

    sprintf(m_session_name, "%d.csv", session_id);
    uint32_t session_len = hpi_log_session_get_length(m_session_name);

    uint16_t number_writes = session_len / FILE_TRANSFER_BLE_PACKET_SIZE;

    uint32_t i = 0;
    struct fs_file_t m_file;
    int rc = 0;

    if (session_len % FILE_TRANSFER_BLE_PACKET_SIZE != 0)
    {
        number_writes++; // Last write will be smaller than 64 bytes
    }

    printk("session id: %s Size: %d NW: %d \n", m_session_name, session_len, number_writes);
    snprintf(m_session_path, sizeof(m_session_path), "/SD:/%d.csv", session_id);

    fs_file_t_init(&m_file);

    rc = fs_open(&m_file, m_session_path, FS_O_READ);

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
        
        cmdif_send_ble_session_data(m_buffer, FILE_TRANSFER_BLE_PACKET_SIZE); // FILE_TRANSFER_BLE_PACKET_SIZE);
        k_sleep(K_MSEC(50));
        // printk("\n");
    }

    rc = fs_close(&m_file);
    if (rc != 0)
    {
        printk("Error closing file %d\n", rc);
        return;
    }

    printk("sess sent\n");
}

void hpi_datalog_delete_all(void)
{
    int res;
    struct fs_dir_t dir;

    char session_name[100] = "";

    fs_dir_t_init(&dir);

    res = fs_opendir(&dir, "/SD:/");
    if (res)
    {
        printk("Unable to open (err %d)", res);
    }

    while (1)
    {
        struct fs_dirent entry;

        res = fs_readdir(&dir, &entry);
        if (res)
        {
            printk("Unable to read directory");
            break;
        }

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
            strcpy(session_name, "/SD:/");
            strcat(session_name, entry.name);
            fs_unlink(session_name);
            printk("Deleting %s\n", session_name);

        }
    }

    fs_closedir(&dir);

    printk("All sessions deleted\n");
}

void hpi_datalog_delete_session(uint16_t session_id)
{
    char session_name[30];

    snprintf(session_name, sizeof(session_name), "/SD:/%d.csv", session_id);

    fs_unlink(session_name);
    printk("File %d deleted %s\n", session_name);
}

void hpi_datalog_start_session(uint8_t *in_pkt_buf)
{
    set_current_session_id(in_pkt_buf[1], in_pkt_buf[2], in_pkt_buf[3], in_pkt_buf[4], in_pkt_buf[5], in_pkt_buf[6]);

    struct fs_statvfs sbuf;
    int rc = fs_statvfs(mp_sd->mnt_point, &sbuf);

    if (rc < 0)
    {
        printk("FAILED to return stats");
    }

    printk("free: %lu, available : %f\n", sbuf.f_bfree, (0.25 * sbuf.f_blocks));

    if (sbuf.f_bfree >= (0.25 * sbuf.f_blocks))
    {
        settings_log_data_enabled = true;
        cmdif_send_memory_status(CMD_LOGGING_MEMORY_FREE);
        write_header_to_new_session();
    }
    else
    {
        // if memory available is less than 25%
        cmdif_send_memory_status(CMD_LOGGING_MEMORY_NOT_AVAILABLE);
    }
}

void hpi_log_session_write_file()
{
    //printf("Write to file... %d\n", hpi_log_session_header.session_id);

    struct fs_file_t file;
    fs_file_t_init(&file);

    char session_name[50] = "/SD:/";
    char session_id_str[20];
    char sensor_data[32];

    
    sprintf(session_id_str, "%d", hpi_log_session_header.session_id);
    strcat(session_name, session_id_str);
    strcat(session_name, ".csv");

    //printk("session_name %s\n",session_name);

    int rc = fs_open(&file, session_name, FS_O_CREATE | FS_O_RDWR | FS_O_APPEND);
    if (rc < 0)
    {
        printk("FAIL: open %s: %d", session_name, rc);
    }

    for (int i = 0; i < current_session_ecg_counter; i++)
    {
        snprintf(sensor_data, sizeof(sensor_data), "%d,%d,%d\n", log_buffer[i].log_ecg_sample,log_buffer[i].log_ppg_sample,log_buffer[i].log_bioz_sample);
        rc = fs_write(&file, sensor_data, strlen(sensor_data));
    }


    rc = fs_close(&file);
    //rc = fs_sync(&file);

    //printk("Log buffer data written to log file %d\n",hpi_log_session_header.session_id);
}