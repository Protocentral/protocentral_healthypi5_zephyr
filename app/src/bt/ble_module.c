#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/sys/ring_buffer.h>

#include <zephyr/settings/settings.h>

#include "cmd_module.h"

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
LOG_MODULE_REGISTER(ble_module);

struct bt_conn *current_conn;

static uint8_t spo2_att_ble[5];
static uint8_t temp_att_ble[2];

// BLE GATT Identifiers

#define HPI_SPO2_SERVICE BT_UUID_DECLARE_16(BT_UUID_POS_VAL)
#define HPI_SPO2_CHAR BT_UUID_DECLARE_16(BT_UUID_GATT_PLX_SCM_VAL)

#define HPI_TEMP_SERVICE BT_UUID_DECLARE_16(BT_UUID_HTS_VAL)
#define HPI_TEMP_CHAR BT_UUID_DECLARE_16(BT_UUID_TEMPERATURE_VAL)

// ECG/Resp Service
#define HPI_ECG_RESP_SERV BT_UUID_DECLARE_16(0x1122)
#define HPI_CHAR_ECG BT_UUID_DECLARE_16(0x1424)

//babe4a4c-7789-11ed-a1eb-0242ac120002
#define HPI_CHAR_RESP BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xbabe4a4c, 0x7789, 0x11ed, 0xa1eb, 0x0242ac120002))

static void spo2_on_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void temp_on_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void ecg_resp_on_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
}

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
				  BT_UUID_16_ENCODE(BT_UUID_HRS_VAL),
				  // BT_UUID_16_ENCODE(BT_UUID_POS_VAL)
				  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
				  BT_UUID_16_ENCODE(BT_UUID_DIS_VAL))};

BT_GATT_SERVICE_DEFINE(hpi_spo2_service,
					   BT_GATT_PRIMARY_SERVICE(HPI_SPO2_SERVICE),
					   BT_GATT_CHARACTERISTIC(HPI_SPO2_CHAR,
											  BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE,
											  BT_GATT_PERM_READ,
											  NULL, NULL, NULL),
					   BT_GATT_CCC(spo2_on_cccd_changed,
								   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

BT_GATT_SERVICE_DEFINE(hpi_temp_service,
					   BT_GATT_PRIMARY_SERVICE(HPI_TEMP_SERVICE),
					   BT_GATT_CHARACTERISTIC(HPI_TEMP_CHAR,
											  BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE,
											  BT_GATT_PERM_READ,
											  NULL, NULL, NULL),
					   BT_GATT_CCC(temp_on_cccd_changed,
								   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

BT_GATT_SERVICE_DEFINE(hpi_ecg_resp_service,
					   BT_GATT_PRIMARY_SERVICE(HPI_ECG_RESP_SERV),
					   BT_GATT_CHARACTERISTIC(HPI_CHAR_ECG,
											  BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE,
											  BT_GATT_PERM_READ,
											  NULL, NULL, NULL),
					   BT_GATT_CHARACTERISTIC(HPI_CHAR_RESP,
											  BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE,
											  BT_GATT_PERM_READ,
											  NULL, NULL, NULL),
					   BT_GATT_CCC(ecg_resp_on_cccd_changed,
								   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

void ble_spo2_notify(uint16_t spo2_val)
{

	spo2_att_ble[0] = 0x00;
	spo2_att_ble[1] = (uint8_t)spo2_val;
	spo2_att_ble[2] = (uint8_t)(spo2_val >> 8);
	spo2_att_ble[3] = 0;
	spo2_att_ble[4] = 0;

	bt_gatt_notify(NULL, &hpi_spo2_service.attrs[2], &spo2_att_ble, sizeof(spo2_att_ble));
}

void ble_ecg_notify(uint16_t *ecg_data, uint8_t len)
{
	//Attribute table: 0 = Service, 1 = Primary service, 2 = ECG, 3 = RESP, 4 = CCC
	bt_gatt_notify(NULL, &hpi_ecg_resp_service.attrs[2], ecg_data, len);
}

void ble_temp_notify(uint16_t temp_val)
{
	temp_val = temp_val / 10;
	temp_att_ble[0] = (uint8_t)temp_val;
	temp_att_ble[1] = (uint8_t)(temp_val >> 8);

	bt_gatt_notify(NULL, &hpi_temp_service.attrs[2], &temp_att_ble, sizeof(temp_att_ble));
}

void ble_hrs_notify(uint16_t hr_val)
{
	bt_hrs_notify(hr_val);
}

void ble_bas_notify(uint8_t batt_level)
{
	bt_bas_set_battery_level(batt_level);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err)
	{
		printk("Connection failed (err 0x%02x)\n", err);
	}
	else
	{
		printk("Connected\n");
		// send_status_serial(BLE_STATUS_CONNECTED);
		current_conn = bt_conn_ref(conn);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);
	// send_status_serial(BLE_STATUS_DISCONNECTED);
	if (current_conn)
	{
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
}

/*static void security_changed(struct bt_conn *conn, bt_security_t level,
							 enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err)
	{
		printk("Security changed: %s level %u\n", addr, level);
	}
	else
	{
		printk("Security failed: %s level %u err %d\n", addr, level,
			   err);
	}
}*/

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	//.security_changed = security_changed,
};

static void bt_ready(void)
{
	int err;

	printk("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS))
	{
		settings_load();
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err)
	{
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	char passkey_str[7];
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	snprintk(passkey_str, ARRAY_SIZE(passkey_str), "%06u", passkey);

	printk("Passkey for %s: %s\n", addr, passkey_str);

	// k_poll_signal_raise(&passkey_enter_signal, 0);
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char passkey_str[7];
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	snprintk(passkey_str, ARRAY_SIZE(passkey_str), "%06u", passkey);

	LOG_DBG("Passkey for %s: %s", addr, passkey_str);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
	.passkey_display = auth_passkey_display,
	.passkey_confirm = auth_passkey_confirm,
};

void remove_separators(char *str)
{
	char *pr = str, *pw = str;
	char c = ':';
	while (*pr)
	{
		*pw = *pr++;
		pw += (*pw != c);
	}
	*pw = '\0';
}

void ble_module_init()
{
	int err = 0;

	err = bt_enable(NULL);
	if (err)
	{
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}

	bt_ready();

	bt_conn_auth_cb_register(&auth_cb_display);
}