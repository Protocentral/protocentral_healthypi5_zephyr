#pragma once

void ble_module_init();

void ble_bas_notify(uint8_t batt_level);
void ble_spo2_notify(uint16_t spo2_val);
void ble_temp_notify(uint16_t temp_val);
void ble_hrs_notify(uint16_t hr_val);
