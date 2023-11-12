#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>

#include "hpi_data_service.h"

#include "cmd_module.h"

#define MAX_TRANSMIT_SIZE 240
uint8_t data_rx[MAX_TRANSMIT_SIZE];
uint8_t data_tx[MAX_TRANSMIT_SIZE];

#define BT_UUID_HPI_DATA_SERVICE BT_UUID_DECLARE_128(HPI_DATA_SERVICE_UUID)
#define BT_UUID_HPI_DATA_SERVICE_RX BT_UUID_DECLARE_128(RX_CHARACTERISTIC_UUID)
#define BT_UUID_HPI_DATA_SERVICE_TX BT_UUID_DECLARE_128(TX_CHARACTERISTIC_UUID)

extern struct bt_conn *current_conn;

/* This function is called whenever the CCCD register has been changed by the client*/
void spo2_on_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    switch (value)
    {
    case BT_GATT_CCC_NOTIFY:
        // Start sending stuff!
        break;

    case BT_GATT_CCC_INDICATE:
        // Start sending stuff via indications
        break;

    case 0:
        // Stop sending stuff
        break;

    default:
        printk("Error, CCCD has been set to an invalid value");
    }
}

volatile uint8_t in_data_buffer[50];

/* This function is called whenever the RX Characteristic has been written to by a Client */
static ssize_t on_receive(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          const void *buf,
                          uint16_t len,
                          uint16_t offset,
                          uint8_t flags)
{
    const uint8_t *buffer = buf;

    printk("Received data len %d \n", len);

    for (uint8_t i = 0; i < len; i++)
    {
        in_data_buffer[i] = buffer[i];
        printk("%02X", buffer[i]);
    }
    printk("\n");
    
    //send_data_serial(in_data_buffer, len);

    return len;
}

/* This function is called whenever a Notification has been sent by the TX Characteristic */
static void on_sent(struct bt_conn *conn, void *user_data)
{
    ARG_UNUSED(user_data);
    ARG_UNUSED(conn);

    //const bt_addr_le_t *addr = bt_conn_get_dst(conn);

    //printk("Data sent to Address 0x %02X %02X %02X %02X %02X %02X \n", addr->a.val[0], addr->a.val[1], addr->a.val[2], addr->a.val[3], addr->a.val[4], addr->a.val[5]);
}

/* LED Button Service Declaration and Registration */
BT_GATT_SERVICE_DEFINE(wiser_service,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_HPI_DATA_SERVICE),
                       BT_GATT_CHARACTERISTIC(BT_UUID_HPI_DATA_SERVICE_RX,
                                              BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                                              NULL, on_receive, NULL),
                       BT_GATT_CHARACTERISTIC(BT_UUID_HPI_DATA_SERVICE_TX,
                                              BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ,
                                              NULL, NULL, NULL),
                       BT_GATT_CCC(spo2_on_cccd_changed,
                                   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

/* This function sends a notification to a Client with the provided data,
given that the Client Characteristic Control Descripter has been set to Notify (0x1).
It also calls the on_sent() callback if successful*/

void hpi_service_send(struct bt_conn *conn, const uint8_t *data, uint16_t len)
{
    /*
    The attribute for the TX characteristic is used with bt_gatt_is_subscribed
    to check whether notification has been enabled by the peer or not.
    Attribute table: 0 = Service, 1 = Primary service, 2 = RX, 3 = TX, 4 = CCC.
    */
    const struct bt_gatt_attr *attr = &wiser_service.attrs[3];

    struct bt_gatt_notify_params params =
        {
            .uuid = BT_UUID_HPI_DATA_SERVICE_TX,
            .attr = attr,
            .data = data,
            .len = len,
            .func = on_sent};

    // Check whether notifications are enabled or not
    if (bt_gatt_is_subscribed(conn, attr, BT_GATT_CCC_NOTIFY))
    {
        // Send the notification
        if (bt_gatt_notify_cb(conn, &params))
        {
            printk("Error, unable to send notification\n");
        }
    }
    else
    {
        printk("Warning, notification not enabled on the selected attribute\n");
    }
}

void send_data_ble(uint8_t *data, uint16_t len)
{
    if (current_conn)
    {
        hpi_service_send(current_conn, data, len);
    }
}
int hpi_service_init(void)
{
    int err = 0;

    memset(&data_rx, 0, MAX_TRANSMIT_SIZE);
    memset(&data_tx, 0, MAX_TRANSMIT_SIZE);

    return err;
}
