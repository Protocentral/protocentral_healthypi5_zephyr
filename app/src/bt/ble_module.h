#pragma once

void ble_module_init();

void ble_bas_notify(uint8_t batt_level);
void ble_spo2_notify(uint16_t spo2_val);
void ble_temp_notify(int16_t temp_val);
void ble_hrs_notify(uint16_t hr_val);
void ble_resp_rate_notify(uint16_t resp_rate);

void ble_ecg_notify(int32_t *ecg_data, uint8_t len);
void ble_ppg_notify(int16_t *ppg_data, uint8_t len);
void ble_resp_notify(int32_t *resp_data, uint8_t len);

static ssize_t on_receive_cmd(struct bt_conn *conn,const struct bt_gatt_attr *attr,const void *buf,uint16_t len,uint16_t offset,uint8_t flags);
static void cmd_on_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value);
