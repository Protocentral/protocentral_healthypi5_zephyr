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

#include "record_module.h"
#include "ble_module.h"
#include "data_module.h"
#include "hw_module.h"
#include "sampling_module.h"

// #include "tdcs3.h"

// #define ESP_UART_DEVICE_NODE DT_ALIAS(esp_uart)
#define MAX_MSG_SIZE 32

#define FILE_TRANSFER_BLE_PACKET_SIZE 64 // (16*7)
#define CMDIF_BLE_UART_MAX_PKT_SIZE 128  // Max Packet Size in bytes

K_SEM_DEFINE(sem_ble_connected, 0, 1);
K_SEM_DEFINE(sem_ble_disconnected, 0, 1);

K_MSGQ_DEFINE(q_cmd_msg, sizeof(struct hpi_cmd_data_obj_t), 16, 1);

// static const struct device *const esp_uart_dev = DEVICE_DT_GET(ESP_UART_DEVICE_NODE);
static volatile int ecs_rx_state = 0;

int cmd_pkt_len;
int cmd_pkt_pos_counter, cmd_pkt_data_counter;
int cmd_pkt_pkttype;
uint8_t ces_pkt_data_buffer[1000]; // = new char[1000];
volatile bool cmd_module_ble_connected = false;

extern struct k_msgq q_sample;
extern int global_dev_status;
extern struct fs_mount_t *mp_sd;
struct healthypi_session_header_t healthypi_session_header_data;
bool settings_log_data_enabled = false;

int8_t data_pkt[272];
uint8_t buf_log[1024]; // 56 bytes / session, 18 sessions / packet

struct wiser_cmd_data_fifo_obj_t
{
    void *fifo_reserved; /* 1st word reserved for use by FIFO */

    uint8_t pkt_type;
    uint8_t data_len;
    uint8_t data[MAX_MSG_SIZE];
};

K_FIFO_DEFINE(cmd_data_fifo);

struct wiser_cmd_data_fifo_obj_t cmd_data_obj;

void write_header_to_new_session()
{
    struct fs_file_t file;
    int rc;
    char session_name[50] = "/SD:/";

    char session_id_str[20];
    sprintf(session_id_str, "%d", healthypi_session_header_data.session_id);
    strcat(session_name, session_id_str);
    strcat(session_name, ".csv");

    char session_record_details[200] = "Session started at: ";
    char session_record_time[2];

    sprintf(session_record_time, "%d", healthypi_session_header_data.session_start_time.day);
    strcat(session_record_details, session_record_time);

    strcat(session_record_details, "/");

    sprintf(session_record_time, "%d", healthypi_session_header_data.session_start_time.month);
    strcat(session_record_details, session_record_time);

    strcat(session_record_details, "/");

    sprintf(session_record_time, "%d", healthypi_session_header_data.session_start_time.year);
    strcat(session_record_details, session_record_time);

    strcat(session_record_details, " ");

    sprintf(session_record_time, "%d", healthypi_session_header_data.session_start_time.hour);
    strcat(session_record_details, session_record_time);

    strcat(session_record_details, ":");

    sprintf(session_record_time, "%d", healthypi_session_header_data.session_start_time.minute);
    strcat(session_record_details, session_record_time);

    strcat(session_record_details, ":");

    sprintf(session_record_time, "%d", healthypi_session_header_data.session_start_time.second);
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

    printf("Header written to file... %d\n", healthypi_session_header_data.session_id);
}


void set_current_session_id(uint8_t m_sec, uint8_t m_min, uint8_t m_hour, uint8_t m_day, uint8_t m_month, uint8_t m_year)
{
    //printk("m_sec %d m_min %d, m_hour %d m_day %d m_month %d m_year %d\n", m_sec, m_min, m_hour, m_day, m_month, m_year);
    uint8_t second, minute, hour, day, month, year;
    year = m_year;
    month = m_month;
    day = m_day;
    hour = m_hour;
    minute = m_min;
    second = m_sec;

    flush_current_session_logs(true);

    // update structure with new session start time
    healthypi_session_header_data.session_start_time.year = year;
    healthypi_session_header_data.session_start_time.month = month;
    healthypi_session_header_data.session_start_time.day = day;
    healthypi_session_header_data.session_start_time.hour = hour;
    healthypi_session_header_data.session_start_time.minute = minute;
    healthypi_session_header_data.session_start_time.second = second;

    uint8_t rand[2];
    sys_rand_get(rand, sizeof(rand));
    healthypi_session_header_data.session_id = (rand[0] | (rand[1] << 8));
    healthypi_session_header_data.session_size = 0;

    printk("Header data for session %d set\n", healthypi_session_header_data.session_id);
}

char* get_session_header(uint16_t session_id)
{
    printk("Getting header for session %u\n", session_id);

    struct healthypi_session_header_t m_header;

    char m_session_name[32];
    snprintf(m_session_name, sizeof(m_session_name), "/SD:/%u.csv", session_id);

    struct fs_file_t m_file;
    fs_file_t_init(&m_file);

    int rc = 0;
    rc = fs_open(&m_file, m_session_name, FS_O_READ);

    if (rc != 0)
    {
        printk("Error opening file %d\n", rc);
    }

    char header_data[37];

    rc = fs_read(&m_file, header_data, sizeof(header_data));
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
    printk("%s\n",header_data);

    return header_data;
}

uint16_t get_session_count(void)
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

int get_all_session_headers(void)
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
            char *saveptr;
            uint16_t session_id = atoi(entry.name);
            uint16_t session_size = entry.size;

            //snprintf(session_header, sizeof(session_header), "%d %d %d %d %d %d %d %d", session_id,session_size,);
            char session_data_time = get_session_header(session_id);

            /*TODO 
            strtok_r(session_data_time, " ", &saveptr);  // "Session"
            strtok_r(NULL, " ", &saveptr); // "started"
            strtok_r(NULL, " ", &saveptr); // "at:"
            
            // Get the date part (14/10/24)
            char *day = strtok_r(NULL, "/", &saveptr);

            printf("Date: %s\n", day);*/

            struct healthypi_session_header_t testing;
            testing.session_id = session_id;
            testing.session_size = session_size;
            testing.session_start_time.day = 14;
            testing.session_start_time.hour = 11;
            testing.session_start_time.minute = 13;
            testing.session_start_time.month = 10;
            testing.session_start_time.second = 6;
            testing.session_start_time.year = 24;

            printk("Session name %d session size %d\n",session_id,session_size);

        
            memcpy(&buf_log, &testing, sizeof(struct healthypi_session_header_t));
            cmdif_send_ble_data_idx(buf_log, sizeof(struct healthypi_session_header_t));
            printk("Header of session id %d sent\n", session_id);
        }
    }
    fs_closedir(&dirp);

    return res;
}

uint32_t get_actual_session_length(char *m_file_name)
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
    return session_len-49;
}

void transfer_session_data(uint16_t session_id)
{
    int8_t m_buffer[FILE_TRANSFER_BLE_PACKET_SIZE] = {0};

    char m_session_name[30];
    char m_session_path[30];

    sprintf(m_session_name, "%d.csv", session_id);
    uint32_t session_len = get_actual_session_length(m_session_name);

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

    fs_seek(&m_file,49,FS_SEEK_SET);

    for (i = 0; i < number_writes; i++)
    {
        rc = fs_read(&m_file, m_buffer, FILE_TRANSFER_BLE_PACKET_SIZE);
        if (rc < 0)
        {
            printk("Error reading file %d\n", rc);
            return;
        }

        /*for (int i=0;i<FILE_TRANSFER_BLE_PACKET_SIZE;i++)
        {
            printk("%d\n",m_buffer[i]);
        }*/
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

void fetch_session_data(uint16_t session_id)
{
    printk("Getting session id %u data\n", session_id);
    transfer_session_data(session_id);
}

void ces_parse_packet(char rxch)
{
    // printk("%0x\n", rxch);

    switch (ecs_rx_state)
    {
    case CMD_SM_STATE_INIT:
        if (rxch == CES_CMDIF_PKT_START_1)
            ecs_rx_state = CMD_SM_STATE_SOF1_FOUND;
        break;

    case CMD_SM_STATE_SOF1_FOUND:
        if (rxch == CES_CMDIF_PKT_START_2)
            ecs_rx_state = CMD_SM_STATE_SOF2_FOUND;
        else
            ecs_rx_state = CMD_SM_STATE_INIT; // Invalid Packet, reset state to init
        break;

    case CMD_SM_STATE_SOF2_FOUND:

        ecs_rx_state = CMD_SM_STATE_PKTLEN_FOUND;
        cmd_pkt_len = (int)rxch;
        cmd_pkt_pos_counter = CES_CMDIF_IND_LEN;
        cmd_pkt_data_counter = 0;
        break;

    case CMD_SM_STATE_PKTLEN_FOUND:

        cmd_pkt_pos_counter++;
        if (cmd_pkt_pos_counter < CES_CMDIF_PKT_OVERHEAD) // Read Header
        {
            if (cmd_pkt_pos_counter == CES_CMDIF_IND_LEN_MSB)
                cmd_pkt_len = (int)((rxch << 8) | cmd_pkt_len);
            else if (cmd_pkt_pos_counter == CES_CMDIF_IND_PKTTYPE)
                cmd_pkt_pkttype = (int)rxch;
        }
        else if ((cmd_pkt_pos_counter >= CES_CMDIF_PKT_OVERHEAD) && (cmd_pkt_pos_counter < CES_CMDIF_PKT_OVERHEAD + cmd_pkt_len + 1)) // Read Data
        {
            ces_pkt_data_buffer[cmd_pkt_data_counter++] = (char)(rxch); // Buffer that assigns the data separated from the packet
        }
        else // All data received
        {
            if (rxch == CES_CMDIF_PKT_STOP_2)
            {
                printk("Packet Received len: %d, type: %d\n", cmd_pkt_len, cmd_pkt_pkttype);
                cmd_pkt_pos_counter = 0;
                cmd_pkt_data_counter = 0;
                ecs_rx_state = 0;
                cmd_data_obj.pkt_type = cmd_pkt_pkttype;
                cmd_data_obj.data_len = cmd_pkt_len;
                memcpy(cmd_data_obj.data, ces_pkt_data_buffer, cmd_pkt_len);

                k_fifo_put(&cmd_data_fifo, &cmd_data_obj);
            }
        }
    }
}

void delete_all_session_files(void)
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
            printk("Deleting %s\n", session_name);
            fs_unlink(session_name);
        }   
    }

    fs_closedir(&dir);

    printk("All sessions deleted\n");
}

void delete_session_file(uint16_t session_id)
{
    char session_name[30];

    snprintf(session_name, sizeof(session_name), "/SD:/%d.csv", session_id);

    fs_unlink(session_name);
    printk("File %d deleted %s\n", session_name);
}

void hpi_decode_data_packet(uint8_t *in_pkt_buf, uint8_t pkt_len)
{
    int rc;
    uint8_t cmd_cmd_id = in_pkt_buf[0];

    // printk("Recd Command: %X\n", cmd_cmd_id);

    switch (cmd_cmd_id)
    {

    case HPI_CMD_GET_DEVICE_STATUS:
        printk("Recd Get Device Status Command\n");
        // cmdif_send_ble_device_status_response();
        break;

    case HPI_CMD_RESET:
        printk("Recd Reset Command\n");
        printk("Rebooting...\n");
        k_sleep(K_MSEC(1000));
        sys_reboot(SYS_REBOOT_COLD);
        break;

    case CMD_LOG_GET_COUNT:
        printk("Comamnd to send log count\n");
        get_session_count();
        break;

    case CMD_LOG_SESSION_HEADERS:
        printk("Sending all session headers\n");
        get_all_session_headers();
        break;

    case CMD_FETCH_LOG_FILE_DATA:
        printk("Command to fetch file data\n");
        fetch_session_data(in_pkt_buf[2] | (in_pkt_buf[1] << 8));
        break;

    case CMD_SESSION_WIPE_ALL:
        printk("Command to delete all files\n");
        delete_all_session_files();
        break;

    case CMG_SESSION_DELETE:
        printk("Command to delete file\n");
        delete_session_file(in_pkt_buf[2] | (in_pkt_buf[1] << 8));
        break;

    case CMD_LOGGING_END:
        printk("Command to end logging\n");
        settings_log_data_enabled = false;
        break;

    case CMD_LOGGING_START:
        //bool header_set_flag = false;
        printk("Command to start logging\n");

        set_current_session_id(in_pkt_buf[1], in_pkt_buf[2], in_pkt_buf[3], in_pkt_buf[4], in_pkt_buf[5], in_pkt_buf[6]);
        
        struct fs_statvfs sbuf;
        int rc = fs_statvfs(mp_sd->mnt_point, &sbuf);

        if (rc < 0)
        {
            printk("FAILED to return stats");
        }

        printk("free: %lu, available : %f\n",sbuf.f_bfree,(0.25 * sbuf.f_blocks));

        if (sbuf.f_bfree >= (0.25 * sbuf.f_blocks))
        {
            settings_log_data_enabled = true;
            cmdif_send_memory_status(CMD_LOGGING_MEMORY_FREE);
            write_header_to_new_session();

        }
        else
        {
            //if memory available is less than 25%
            cmdif_send_memory_status(CMD_LOGGING_MEMORY_NOT_AVAILABLE);
        }        

        break;

    default:
        printk("Recd Unknown Command\n");
        break;
    }
}

void cmdif_send_ble_data_idx(uint8_t *m_data, uint8_t m_data_len)
{
    uint8_t cmd_pkt[1 + m_data_len];
    cmd_pkt[0] = CES_CMDIF_TYPE_LOG_IDX;

    for (int i = 0; i < m_data_len; i++)
    {
        cmd_pkt[1 + i] = m_data[i];
    }

    healthypi5_service_send_data(cmd_pkt, 1 + m_data_len);
}

void cmdif_send_memory_status(uint8_t m_cmd)
{
    // printk("Sending BLE Status\n");
    uint8_t cmd_pkt[3];

    cmd_pkt[0] = CES_CMDIF_TYPE_STATUS;
    cmd_pkt[1] = 0x55;
    cmd_pkt[2] = m_cmd;

    healthypi5_service_send_data(cmd_pkt, 3);
}

void cmdif_send_session_count(uint8_t m_cmd)
{
    // printk("Sending BLE Status\n");
    uint8_t cmd_pkt[3];

    cmd_pkt[0] = CES_CMDIF_TYPE_CMD_RSP;
    cmd_pkt[1] = 0x54;
    cmd_pkt[2] = m_cmd;

    // printk("sending response\n");
    healthypi5_service_send_data(cmd_pkt, 3);
}

void cmdif_send_ble_session_data(int8_t *m_data, uint8_t m_data_len)
{
    // printk("Sending BLE Data: %d\n", m_data_len);

    data_pkt[0] = CES_CMDIF_TYPE_DATA;

    for (int i = 0; i < m_data_len; i++)
    {
        data_pkt[1 + i] = m_data[i];
    }
    healthypi5_service_send_data(data_pkt, 1 + m_data_len);
}

// TODO: implement BLE UART
/*void cmdif_send_ble_data(const char *in_data_buf, size_t in_data_len)
{
    uint8_t dataPacket[50];

    dataPacket[0] = CES_CMDIF_PKT_START_1;
    dataPacket[1] = CES_CMDIF_PKT_START_2;
    dataPacket[2] = in_data_len;
    dataPacket[3] = 0;
    dataPacket[4] = CES_CMDIF_TYPE_DATA;

    for (int i = 0; i < in_data_len; i++)
    {
        dataPacket[i + 5] = in_data_buf[i];
        //printk("Data %x: %d\n", i, in_data_buf[i]);
    }

    dataPacket[in_data_len + 5] = CES_CMDIF_PKT_STOP_1;
    dataPacket[in_data_len + 6] = CES_CMDIF_PKT_STOP_2;

    //printk("Sending UART data: %d\n", in_data_len);

    for (int i = 0; i < (in_data_len + 7); i++)
    {
        //uart_poll_out(esp_uart_dev, dataPacket[i]);
    }
}*/

void cmdif_send_ble_progress(uint8_t m_stage, uint16_t m_total_time, uint16_t m_curr_time, uint16_t m_current, uint16_t m_imped)
{
    uint8_t cmd_pkt[16];
    cmd_pkt[0] = CES_CMDIF_PKT_START_1;
    cmd_pkt[1] = CES_CMDIF_PKT_START_2;
    cmd_pkt[2] = 0x09;
    cmd_pkt[3] = 0x00;
    cmd_pkt[4] = CES_CMDIF_TYPE_PROGRESS;
    cmd_pkt[5] = m_stage;
    cmd_pkt[6] = (uint8_t)(m_total_time & 0x00FF);
    cmd_pkt[7] = (uint8_t)((m_total_time >> 8) & 0x00FF);
    cmd_pkt[8] = (uint8_t)(m_curr_time & 0x00FF);
    cmd_pkt[9] = (uint8_t)((m_curr_time >> 8) & 0x00FF);
    cmd_pkt[10] = (uint8_t)(m_current & 0x00FF);
    cmd_pkt[11] = (uint8_t)((m_current >> 8) & 0x00FF);
    cmd_pkt[12] = (uint8_t)(m_imped & 0x00FF);
    cmd_pkt[13] = (uint8_t)((m_imped >> 8) & 0x00FF);
    cmd_pkt[14] = CES_CMDIF_PKT_STOP_1;
    cmd_pkt[15] = CES_CMDIF_PKT_STOP_2;

    for (int i = 0; i < 16; i++)
    {
        // uart_poll_out(esp_uart_dev, cmd_pkt[i]);
    }
}

void cmdif_send_ble_device_status_response(void)
{
    // cmdif_send_ble_status(WISER_CMD_GET_DEVICE_STATUS, global_dev_status);
}

void cmdif_send_ble_command(uint8_t m_cmd)
{
    printk("Sending BLE Command: %X\n", m_cmd);
    uint8_t cmd_pkt[8];
    cmd_pkt[0] = CES_CMDIF_PKT_START_1;
    cmd_pkt[1] = CES_CMDIF_PKT_START_2;
    cmd_pkt[2] = 0x01;
    cmd_pkt[3] = 0x00;
    cmd_pkt[4] = CES_CMDIF_TYPE_CMD;
    cmd_pkt[5] = m_cmd;
    cmd_pkt[6] = CES_CMDIF_PKT_STOP_1;
    cmd_pkt[7] = CES_CMDIF_PKT_STOP_2;

    for (int i = 0; i < 8; i++)
    {
        // uart_poll_out(esp_uart_dev, cmd_pkt[i]);
    }
}

/*void send_uart(char *buf)
{
    int msg_len = strlen(buf);

    for (int i = 0; i < msg_len; i++)
    {
        uart_poll_out(esp_uart_dev, buf[i]);
    }
}*/

/*
void cmd_serial_cb(const struct device *dev, void *user_data)
{
    uint8_t c;

    if (!uart_irq_update(esp_uart_dev))
    {
        return;
    }

    if (!uart_irq_rx_ready(esp_uart_dev))
    {
        return;
    }

    while (uart_fifo_read(esp_uart_dev, &c, 1) == 1)
    {
        ces_parse_packet(c);
    }
}
*/

static void cmd_init(void)
{
    printk("CMD Module Init\n");

    /*if (!device_is_ready(esp_uart_dev))
    {
        printk("UART device not found!");
        return;
    }

    int ret = uart_irq_callback_user_data_set(esp_uart_dev, cmd_serial_cb, NULL);

    if (ret < 0)
    {
        if (ret == -ENOTSUP)
        {
            printk("Interrupt-driven UART API support not enabled\n");
        }
        else if (ret == -ENOSYS)
        {
            printk("UART device does not support interrupt-driven API\n");
        }
        else
        {
            printk("Error setting UART callback: %d\n", ret);
        }
        return;
    }
    uart_irq_rx_enable(esp_uart_dev);
    */
}

/*void cmd_thread(void)
{
    printk("CMD Thread Started\n");

    cmd_init();

    struct wiser_cmd_data_fifo_obj_t *rx_cmd_data_obj;

    for (;;)
    {
        rx_cmd_data_obj = k_fifo_get(&cmd_data_fifo, K_FOREVER);
        printk("Recd Packet Type: %d\n", rx_cmd_data_obj->pkt_type);
        if (rx_cmd_data_obj->pkt_type == CES_CMDIF_TYPE_DATA)
        {
            printk("Recd BLE Packet len: %d \n", rx_cmd_data_obj->data_len);
            for (int i = 0; i < rx_cmd_data_obj->data_len; i++)
            {
                printk("%02X ", rx_cmd_data_obj->data[i]);
            }
            printk("\n");
            hpi_decode_data_packet(rx_cmd_data_obj->data, rx_cmd_data_obj->data_len);
        }
        else if (rx_cmd_data_obj->pkt_type == CES_CMDIF_TYPE_CMD)
        {
            printk("Recd Command Packet : %d \n", rx_cmd_data_obj->data[0]);
            if (rx_cmd_data_obj->data[0] == HPI_CMD_GET_DEVICE_STATUS)
            {
                printk("Recd Get Device Status Command\n");
                cmdif_send_ble_device_status_response();
            }
            else if (rx_cmd_data_obj->data[0] == HPI_CMD_RESET)
            {
                printk("Recd Reset Command\n");
                printk("Rebooting...\n");
                k_sleep(K_MSEC(1000));
                sys_reboot(SYS_REBOOT_COLD);
            }
            else
            {
                printk("Recd Unknown Command\n");
            }
        }
        else if (rx_cmd_data_obj->pkt_type == CES_CMDIF_TYPE_PROGRESS)
        {
            printk("Recd Progress Packet : %d \n", rx_cmd_data_obj->data[0]);
            if (rx_cmd_data_obj->data[0] == 0x01)
            {
                printk("Recd Progress Packet : %d \n", rx_cmd_data_obj->data[0]);
                // disp_update_ble_progress(rx_cmd_data_obj->data[1], rx_cmd_data_obj->data[2], rx_cmd_data_obj->data[3], rx_cmd_data_obj->data[4], rx_cmd_data_obj->data[5]);
            }
        }
        else if (rx_cmd_data_obj->pkt_type == CES_CMDIF_TYPE_STATUS)
        {
            // printk("Recd Status Packet : %d \n", rx_cmd_data_obj->data[0]);
            if (rx_cmd_data_obj->data[0] == BLE_STATUS_CONNECTED)
            {
                printk("BLE Connected\n");
                // disp_update_ble_conn_status(true);
                k_sem_give(&sem_ble_connected);
                cmd_module_ble_connected = true;
            }
            else if (rx_cmd_data_obj->data[0] == BLE_STATUS_DISCONNECTED)
            {
                printk("BLE Disconnected\n");
                // disp_update_ble_conn_status(false);
                k_sem_give(&sem_ble_disconnected);
                cmd_module_ble_connected = false;
            }
        }
        else
        {
            printk("Recd Unknown Data\n");
        }

        // cmdif_send_ble_command(0x07);

        k_sleep(K_MSEC(1000));
    }
}*/

void cmd_thread(void)
{
    printk("CMD Thread Started\n");

    struct hpi_cmd_data_obj_t rx_cmd_data_obj;

    for (;;)
    {
        if (k_msgq_get(&q_cmd_msg, &rx_cmd_data_obj, K_NO_WAIT) == 0)
        {

            printk("Recd BLE Packet len: %d", rx_cmd_data_obj.data_len);
            for (int i = 0; i < rx_cmd_data_obj.data_len; i++)
            {
                //printk("%02X\t", rx_cmd_data_obj.data[i]);
            }
            printk("\n");
            hpi_decode_data_packet(rx_cmd_data_obj.data, rx_cmd_data_obj.data_len);
        }

        k_sleep(K_MSEC(1000));
    }
}

#define CMD_THREAD_STACKSIZE 2048
#define CMD_THREAD_PRIORITY 7

K_THREAD_DEFINE(cmd_thread_id, CMD_THREAD_STACKSIZE, cmd_thread, NULL, NULL, NULL, CMD_THREAD_PRIORITY, 0, 0);