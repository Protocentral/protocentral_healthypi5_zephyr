#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/reboot.h>

#include "cmd_module.h"

#include "fs_module.h"
// #include "tdcs3.h"

//#define ESP_UART_DEVICE_NODE DT_ALIAS(esp_uart)
#define MAX_MSG_SIZE 32

#define CMDIF_BLE_UART_MAX_PKT_SIZE 128 // Max Packet Size in bytes

K_SEM_DEFINE(sem_ble_connected, 0, 1);
K_SEM_DEFINE(sem_ble_disconnected, 0, 1);

//static const struct device *const esp_uart_dev = DEVICE_DT_GET(ESP_UART_DEVICE_NODE);
static volatile int ecs_rx_state = 0;

int cmd_pkt_len;
int cmd_pkt_pos_counter, cmd_pkt_data_counter;
int cmd_pkt_pkttype;
uint8_t ces_pkt_data_buffer[1000]; // = new char[1000];
volatile bool cmd_module_ble_connected = false;

extern int global_dev_status;

struct wiser_cmd_data_fifo_obj_t
{
    void *fifo_reserved; /* 1st word reserved for use by FIFO */

    uint8_t pkt_type;
    uint8_t data_len;
    uint8_t data[MAX_MSG_SIZE];
};

#define PARTITION_NODE DT_NODELABEL(lfs1)

#if DT_NODE_EXISTS(PARTITION_NODE)
FS_FSTAB_DECLARE_ENTRY(PARTITION_NODE);
#else  /* PARTITION_NODE */
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
    .type = FS_LITTLEFS,
    .fs_data = &storage,
    .storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
    .mnt_point = "/lfs",
};
#endif /* PARTITION_NODE */

struct fs_mount_t *mp =
#if DT_NODE_EXISTS(PARTITION_NODE)
    &FS_FSTAB_ENTRY(PARTITION_NODE)
#else
    &lfs_storage_mnt
#endif
    ;

K_FIFO_DEFINE(cmd_data_fifo);

struct wiser_cmd_data_fifo_obj_t cmd_data_obj;

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

void hpi_decode_data_packet(uint8_t *in_pkt_buf, uint8_t pkt_len)
{
    int rc;
    uint8_t cmd_cmd_id = in_pkt_buf[0];

    printk("Recd Command: %X\n", cmd_cmd_id);

    switch (cmd_cmd_id)
    {

    case HPI_CMD_GET_DEVICE_STATUS:
        printk("Recd Get Device Status Command\n");
        //cmdif_send_ble_device_status_response();
        break;
    
    case HPI_CMD_RESET:
        printk("Recd Reset Command\n");
        printk("Rebooting...\n");
        k_sleep(K_MSEC(1000));
        sys_reboot(SYS_REBOOT_COLD);
        break;

    case CMD_LOGGING_START:
        
        printk("Start logging Command\n");
        struct fs_statvfs sbuf;
        rc = fs_statvfs(mp->mnt_point, &sbuf);
        if (rc < 0)
        {
            printk("FAILED to return stats");
            return rc;
        }
        printk("%s: bsize = %lu ; frsize = %lu ; blocks = %lu ; bfree = %lu\n", mp->mnt_point,sbuf.f_bsize, sbuf.f_frsize, sbuf.f_blocks, sbuf.f_bfree);
        break;

    default:
        printk("Recd Unknown Command\n");
        break;
    }
}

// TODO: implement BLE UART
void cmdif_send_ble_data(const char *in_data_buf, size_t in_data_len)
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
}

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
        //uart_poll_out(esp_uart_dev, cmd_pkt[i]);
    }
}

void cmdif_send_ble_device_status_response(void)
{
    //cmdif_send_ble_status(WISER_CMD_GET_DEVICE_STATUS, global_dev_status);
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
        //uart_poll_out(esp_uart_dev, cmd_pkt[i]);
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

void cmd_thread(void)
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
}

#define CMD_THREAD_STACKSIZE 1024
#define CMD_THREAD_PRIORITY 7

K_THREAD_DEFINE(cmd_thread_id, CMD_THREAD_STACKSIZE, cmd_thread, NULL, NULL, NULL, CMD_THREAD_PRIORITY, 0, 0);
